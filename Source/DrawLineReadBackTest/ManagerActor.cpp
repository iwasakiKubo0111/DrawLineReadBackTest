// Fill out your copyright notice in the Description page of Project Settings.


#include "ManagerActor.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include <Kismet/KismetRenderingLibrary.h>
#include "RT_DisplayWidget.h"
#include "RT_Display10.h"
#include <Components/PoseableMeshComponent.h>
#include "CanvasTypes.h"        // FCanvas 本体（ET_Line/ET_Triangle もこれで解決）
#include "BatchedElements.h"    // FBatchedElements
#include "ImageUtils.h"         // FImageUtils::SaveImageByExtension
#include "ImageCore.h"

#define DPI_SCALE 1.5
#define WINDOW_HIGHT_SIZE 2160 / DPI_SCALE
#define WINDOW_WIDTH_SIZE 3840 / DPI_SCALE

static FORCEINLINE void PutPixel(TArray<FColor>& Buf, int32 W, int32 H, int32 X, int32 Y, const FColor& C)
{
	if (X < 0 || Y < 0 || X >= W || Y >= H) return;
	Buf[Y * W + X] = C;
}

static void DrawFilledCircleCPU(TArray<FColor>& Buf, int32 W, int32 H, FIntPoint Center, int32 Radius, const FColor& C)
{
	for (int32 dy = -Radius; dy <= Radius; ++dy)
		for (int32 dx = -Radius; dx <= Radius; ++dx)
			if (dx * dx + dy * dy <= Radius * Radius)
				PutPixel(Buf, W, H, Center.X + dx, Center.Y + dy, C);
}

static void DrawThickLineCPU(TArray<FColor>& Buf, int32 W, int32 H,
	FIntPoint A, FIntPoint B, const FColor& C, int32 Thickness)
{
	const int32 r = FMath::Max(0, Thickness / 2);
	int32 x0 = A.X, y0 = A.Y, x1 = B.X, y1 = B.Y;
	const int32 dx = FMath::Abs(x1 - x0), dy = -FMath::Abs(y1 - y0);
	const int32 sx = (x0 < x1) ? 1 : -1;
	const int32 sy = (y0 < y1) ? 1 : -1;
	int32 err = dx + dy;

	while (true)
	{
		// 太さぶん円形に塗る
		for (int32 oy = -r; oy <= r; ++oy)
			for (int32 ox = -r; ox <= r; ++ox)
				if (ox * ox + oy * oy <= r * r)
					PutPixel(Buf, W, H, x0 + ox, y0 + oy, C);

		if (x0 == x1 && y0 == y1) break;
		const int32 e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

// Sets default values
AManagerActor::AManagerActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	ConstructorHelpers::FClassFinder<UUserWidget> WidgetBPClass(TEXT("/Game/WBP_MTRTDIsplay"));
	if (WidgetBPClass.Succeeded())
	{
		m_mtRtWidgetClass = WidgetBPClass.Class;
	}

	ConstructorHelpers::FClassFinder<UUserWidget> WidgetBPClass2(TEXT("/Game/WBP_RTDIsplay10"));
	if (WidgetBPClass2.Succeeded())
	{
		m_rt10WidgetClass = WidgetBPClass2.Class;
	}

	//シーンキャプチャの用意
	m_sceneCapture2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	m_sceneCapture2D->bCaptureEveryFrame = true;
	m_sceneCapture2D->bCaptureOnMovement = false;
	m_sceneCapture2D->ProjectionType = ECameraProjectionMode::Orthographic;
	//m_sceneCapture2D->OrthoWidth = MASTER_RT_WIDTH;
	m_sceneCapture2D->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;	// 画面と同じ見た目になる LDR 最終色。重ければ SCS_SceneColorHDR 等に変更する。
	// SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	// SceneCapture->ShowOnlyActors.Add(SomeCopyActor);

	RootComponent = m_sceneCapture2D;
}

// Called when the game starts or when spawned
void AManagerActor::BeginPlay()
{
	Super::BeginPlay();
	
	UUserWidget* widgetInstance = NULL;

	//マスターレンダーターゲット
	{
		//ウィジェットBPをロードしウィンドウ作成
		if (m_mtRtWidgetClass)
		{

			widgetInstance = CreateWidget<UUserWidget>(GetWorld(), m_mtRtWidgetClass);

			if (widgetInstance)
			{
				FVector2D windowPosition{ 0,0 };
				FVector2D windowSize{ WINDOW_WIDTH_SIZE,WINDOW_HIGHT_SIZE };
				FText windowText = FText::FromString(TEXT("previewWindow"));
				TSharedPtr<class SOverlay> windowOverlay = CreateWindow(windowSize, windowPosition, windowText);

				SetWidget(widgetInstance, windowOverlay);
			}
		}

		// OrthoWidth をマスター幅に合わせる（1cm/px を維持）
		m_sceneCapture2D->OrthoWidth = (float)m_layout.MasterWidth();

		//RTを作成
		m_masterRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			this,
			m_layout.MasterWidth(),
			m_layout.MasterHeight(),
			RTF_RGBA8,
			FLinearColor::White,
			/*bAutoGenerateMipMaps=*/false);

		// キャプチャ先として割り当て これ以降、毎フレーム RT へ描画される。
		if (m_sceneCapture2D && m_masterRenderTarget)
		{
			m_sceneCapture2D->TextureTarget = m_masterRenderTarget;
		}

		FVector newLocation(800.000000f, 1110.000000f, 92.012604f);
		//SetActorLocation(newLocation);

		m_sceneCapture2D->SetWorldLocation(newLocation);

		//
		URT_DisplayWidget* rtDisplayWidget = Cast<URT_DisplayWidget>(widgetInstance);
		if (rtDisplayWidget)
		{
			//rtDisplayWidget->BindRenderTarget(m_masterRenderTarget);
			//rtDisplayWidget->BindCell(m_masterRenderTarget,0, 1, 1);//全体表示
			//rtDisplayWidget->BindCell(m_masterRenderTarget,0, DISPLAY_HORIZONTAL_CELLS, DISPLAY_VERTICAL_CELLS);//右上
			rtDisplayWidget->BindCell(m_masterRenderTarget, 0, 1, 1);//左下
			//rtDisplayWidget->BindCell(m_masterRenderTarget,1,1,1);
		}
	}

	//10個のレンダーターゲット
	{
		//ウィジェットBPをロードしウィンドウ作成
		if (m_rt10WidgetClass)
		{

			widgetInstance = CreateWidget<UUserWidget>(GetWorld(), m_rt10WidgetClass);

			if (widgetInstance)
			{
				FVector2D windowPosition{ 0,0 };
				FVector2D windowSize{ WINDOW_WIDTH_SIZE,WINDOW_HIGHT_SIZE };
				FText windowText = FText::FromString(TEXT("previewWindow"));
				TSharedPtr<class SOverlay> windowOverlay = CreateWindow(windowSize, windowPosition, windowText);

				SetWidget(widgetInstance, windowOverlay);
			}
		}

		// キャプチャ先として割り当て これ以降、毎フレーム RT へ描画される。
		if (m_sceneCapture2D && m_masterRenderTarget)
		{
			m_sceneCapture2D->TextureTarget = m_masterRenderTarget;
		}

		FVector newLocation(800.000000f, 1110.000000f, 92.012604f);
		//SetActorLocation(newLocation);

		m_sceneCapture2D->SetWorldLocation(newLocation);

		//
		URT_Display10* rtDisplayWidget = Cast<URT_Display10>(widgetInstance);
		if (rtDisplayWidget)
		{
			//rtDisplayWidget->BindRenderTarget(m_masterRenderTarget);
			//rtDisplayWidget->BindCell(m_masterRenderTarget,0, 1, 1);//全体表示
			//rtDisplayWidget->BindCell(m_masterRenderTarget,0, DISPLAY_HORIZONTAL_CELLS, DISPLAY_VERTICAL_CELLS);//右上
			rtDisplayWidget->BindCell(m_masterRenderTarget, m_layout);//左下
			//rtDisplayWidget->BindCell(m_masterRenderTarget,1,1,1);
		}
	}

	// USkeletalMeshComponentを配置
	{
		const int32 cellCount = m_layout.Cols * m_layout.Rows; // 10
		for (int32 i = 0; i < cellCount; i++)
		{
			const int32 Col = i % m_layout.Cols;
			const int32 Row = i / m_layout.Cols;

			USkeletalMeshComponent* follower = NewObject<USkeletalMeshComponent>(this);
			//follower->bVisibleInSceneCaptureOnly = true;
			follower->RegisterComponent();
			follower->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			follower->SetWorldLocation(GetCellCenterWorld(Col, Row,500.f));

			// テスト確認用（リーダー未設定でもリファレンスポーズが見える）
			if (m_testMesh) { follower->SetSkeletalMeshAsset(m_testMesh); }

			//m_sceneCapture2D->ShowOnlyComponents.Add(follower);
			m_followerComponents.Add(follower);

			// メインビューには出さず、キャプチャにだけ映す
			//pose->SetVisibleInSceneCaptureOnly(true);
			// このキャプチャの ShowOnly 対象に登録（他は撮らない）
			//m_sceneCapture2D->ShowOnlyComponents.Add(pose);
		}
	}

	// キュー配列だけ確保
	{
		m_cellQueues.SetNum(m_layout.Cols* m_layout.Rows);
	}

	LoadMarkTextureToCPU();
}

void AManagerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 破棄対象をローカルへ移し、メンバは先に空にしておく
	// （RemoveAll が走っても空配列への no-op で済み、反復中の書き換えを防げる）
	TArray<TSharedPtr<SWindow>> windowsToClose = MoveTemp(m_windows);
	m_windows.Empty();

	if (FSlateApplication::IsInitialized())
	{
		for (const TSharedPtr<SWindow>& window : windowsToClose)
		{
			if (window.IsValid())
			{
				// this を掴んだラムダが破棄後に走らないよう、先に解除しておく
				window->SetOnWindowClosed(FOnWindowClosed());
				FSlateApplication::Get().RequestDestroyWindow(window.ToSharedRef());
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void AManagerActor::Tick(float DeltaTime)
{
	//Tickで毎フレーム「差し替え進行→今フレーム撮るぶんを集めて1Readback→完了ジョブの保存」を回す

	Super::Tick(DeltaTime);

	UpdateFollowerRotations();   // リアルタイム表示の向き合わせ

	ProcessSnapshotQueues();

	ProcessTrailSaveQueue();

	// 完了したReadbackジョブから保存（複数本並行、完了順に処理）
	for (int32 i = m_readbackJobs.Num() - 1; i >= 0; --i)
	{
		TSharedPtr<FSnapshotReadbackJob> Job = m_readbackJobs[i];
		if (Job.IsValid() && Job->Readback.IsValid() && Job->Readback->IsReady())
		{
			SaveFromAtlas(*Job);
			m_readbackJobs.RemoveAt(i);
		}
	}
}

TSharedPtr<class SOverlay> AManagerActor::CreateWindow(const FVector2D& windowSize, const FVector2D& windowPos, const FText& tilte)
{
	if (GEngine && GEngine->GameViewport) {
		// ウィンドウ作成
		TSharedPtr<class SWindow> newWindow = SNew(SWindow)
			.Title(tilte/*LOCTEXT("CreateWindow", "Title")*/)
			.AutoCenter(EAutoCenter::None)
			.ScreenPosition(windowPos)
			.ClientSize(windowSize)
			.CreateTitleBar(true);

		TSharedPtr<class SOverlay> windowOverlay = SNew(SOverlay);


		//ウィンドウに設定する
		newWindow->SetContent(windowOverlay.ToSharedRef());
		//m_CreateWindow->OnKeyDown(FOnKeyDown::CreateRaw(this, &AWindowCreateActor::HandleKeyDown));

		// 作成したウィンドウを親ウィンドウを設定して即座表示する
		//FSlateApplication::Get().AddWindowAsNativeChild(m_CreateWindow.ToSharedRef(), GEngine->GameViewport->GetWindow().ToSharedRef(), true);
		FSlateApplication::Get().AddWindow(newWindow.ToSharedRef(), true);
		FSlateApplication::Get().SetKeyboardFocus(newWindow.ToSharedRef());
		//FSlateApplication::Get().OnKeyDown

		m_windows.Add(newWindow);

		return windowOverlay;
	}

	return nullptr;
}

void AManagerActor::SetWidget(UWidget* setWidget, TSharedPtr<class SOverlay> overlay)
{
	if (overlay)
	{
		// ウィットを設定するためにオーバーレイに指定されたウィジェットを設定する
		auto OVerlaySlot = overlay.Get()->AddSlot();
		OVerlaySlot.AttachWidget(setWidget->TakeWidget());
	}
}

void AManagerActor::SetCaptureEnabled(bool bEnabled)
{
	// 表示ウィンドウが閉じている間は止めておくとキャプチャコストを払わずに済む。
	if (m_sceneCapture2D)
	{
		m_sceneCapture2D->bCaptureEveryFrame = bEnabled;
	}
}

FVector AManagerActor::GetCellCenterWorld(int32 Col, int32 Row, float DistanceInFront) const
{
	const FVector C = m_sceneCapture2D->GetComponentLocation();
	const FVector F = m_sceneCapture2D->GetForwardVector();
	const FVector R = m_sceneCapture2D->GetRightVector();
	const FVector U = m_sceneCapture2D->GetUpVector();

	const float MW = (float)m_layout.MasterWidth();
	const float MH = (float)m_layout.MasterHeight();
	const float W = m_sceneCapture2D->OrthoWidth;     // = MW（cm）, 1cm/px
	const float H = W * (MH / MW);

	// セル中身の中心ピクセル
	const FIntPoint O = m_layout.CellPixelOrigin(Col, Row);
	const float cx = O.X + m_layout.CellSize * 0.5f;
	const float cy = O.Y + m_layout.CellSize * 0.5f;

	// 正規化（-0.5〜+0.5）。ピクセルyは下方向に増えるのでUは反転
	const float rightFrac = cx / MW - 0.5f;
	const float upFrac = 0.5f - cy / MH;

	return C + F * DistanceInFront + R * (rightFrac * W) + U * (upFrac * H);
}

void AManagerActor::SetCellTarget(int32 CellIndex, AActor* NewTarget)
{
	if (!m_followerComponents.IsValidIndex(CellIndex)) return;
	USkeletalMeshComponent* follower = m_followerComponents[CellIndex];
	if (!follower) return;

	if (!NewTarget)
	{
		// 対象が無くなったらリンク解除
		follower->SetLeaderPoseComponent(nullptr);
		follower->SetSkeletalMeshAsset(nullptr);
		return;
	}

	USkeletalMeshComponent* leader = NewTarget->FindComponentByClass<USkeletalMeshComponent>();
	if (!leader || !leader->GetSkeletalMeshAsset()) return;

	// ① フォロワーをリーダーと同じメッシュに（ボーン構成を一致させる）
	follower->SetSkeletalMeshAsset(leader->GetSkeletalMeshAsset());

	// ② 血・焦げ等の反映用にマテリアル（MID含む）をリーダーから共有
	const int32 numMats = leader->GetNumMaterials();
	for (int32 m = 0; m < numMats; m++)
	{
		follower->SetMaterial(m, leader->GetMaterial(m));
	}

	// ③ リーダーのポーズに追従
	follower->SetLeaderPoseComponent(leader);

	// ★差し替えのたびにセル収めスケールを適用（近傍パネル基準のバウンズで算出）
	ApplyFitScale(CellIndex, NewTarget);

	// 回転追従の対象として記録
	if (m_cellRotationTargets.Num() != m_layout.Cols * m_layout.Rows)
	{
		m_cellRotationTargets.SetNum(m_layout.Cols * m_layout.Rows);
	}
	if (m_cellRotationTargets.IsValidIndex(CellIndex))
	{
		m_cellRotationTargets[CellIndex] = NewTarget;
	}

	CacheCenterOffsetLocal(CellIndex);
}

void AManagerActor::TestSnapShot(const FSnapshotRequest& RequestIn)
{
	const int32 cellCount = m_layout.Cols * m_layout.Rows;
	if (RequestIn.CellIndex < 0 || RequestIn.CellIndex >= cellCount) return;
	if (m_cellQueues.Num() != cellCount) m_cellQueues.SetNum(cellCount);

	// ① 連番採番＆パス確定
	const int32 ShotId = ++m_shotCounter;
	FSnapshotRequest Request = RequestIn; // コピーして書き換える
	Request.OutputPath = FString::Printf(TEXT("%sshoot%d.png"), *m_outputDir, ShotId);
	const FString TrailPath = FString::Printf(TEXT("%strail%d.png"), *m_outputDir, ShotId);

	TArray<FSnapshotRequest>& Queue = m_cellQueues[Request.CellIndex];
	const bool bBecomesHead = (Queue.Num() == 0);
	Queue.Add(Request);

	if (bBecomesHead)
	{
		FSnapshotRequest& Head = Queue[0];
		SetCellTarget(Request.CellIndex, Head.TargetActor);
		// 回転は UpdateFollowerRotations が毎フレーム入れるので、ここでは入れない方が整合
		Head.bSwapped = true;
		Head.SwapFrame = GFrameCounter;
	}

	// ② 着弾ピクセルを計算（差し替え済み前提。複数着弾点があれば代表点でよい）
	FVector2D HitPixel(m_layout.CellSize * 0.5f, m_layout.CellSize * 0.5f);
	if (Request.LocalHitPoints.Num() > 0)
	{
		HitPixel = CellLocalPixelFromLocalHit(Request.CellIndex, Request.LocalHitPoints[0]);
	}

	// ③ 軌跡用：今の着弾点を記録＆2秒後保存を予約
	RecordTrailPoint(HitPixel);
	RequestTrailSave(Request.CellIndex, TrailPath);
}

//void AManagerActor::EnqueueSnapshotDrawAndReadback(int32 CellIndex)
//{
//	if (!m_masterRenderTarget || !m_snapshotRT) return;
//
//	const int32 Col = CellIndex % m_layout.Cols;
//	const int32 Row = CellIndex / m_layout.Cols;
//
//	FTextureRenderTargetResource* MasterRes = m_masterRenderTarget->GameThread_GetRenderTargetResource();
//	FTextureRenderTargetResource* SnapRes = m_snapshotRT->GameThread_GetRenderTargetResource();
//	const FIntPoint CellOrigin = m_layout.CellPixelOrigin(Col, Row); // ★指定セルの左上(px)
//	const int32 CellSize = m_layout.CellSize;
//
//	const FTexture* MarkTex =
//		(m_markTexture && m_markTexture->GetResource()) ? m_markTexture->GetResource() : nullptr;
//
//	// ---- テスト用の描画データ（座標はセルローカル 0〜CellSize）----
//	TArray<FVector2D> Points;
//	{
//		FVector2D P(CellSize * 0.5f, CellSize * 0.5f);
//		for (int32 i = 0; i < 300; ++i)
//		{
//			P += FVector2D(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f));
//			P.X = FMath::Clamp(P.X, 0.f, (float)CellSize);
//			P.Y = FMath::Clamp(P.Y, 0.f, (float)CellSize);
//			Points.Add(P);
//		}
//	}
//	TArray<FVector2D> MarkPoints;
//	for (int32 i = 0; i < 5; ++i)
//	{
//		MarkPoints.Add(FVector2D(FMath::FRandRange(0.f, (float)CellSize),
//			FMath::FRandRange(0.f, (float)CellSize)));
//	}
//
//	ENQUEUE_RENDER_COMMAND(SnapshotDrawReadbackCmd)(
//		[this, MasterRes, SnapRes, CellOrigin, CellSize, MarkTex,
//		Points = MoveTemp(Points), MarkPoints = MoveTemp(MarkPoints)]
//		(FRHICommandListImmediate& RHICmdList)
//		{
//			FRHITexture* Src = MasterRes ? MasterRes->GetRenderTargetTexture() : nullptr;
//			FRHITexture* Dst = SnapRes ? SnapRes->GetRenderTargetTexture() : nullptr;
//			if (!Src || !Dst) return;
//
//			// ① マスターRTのセル0領域をスナップショットRTへGPUコピー
//			RHICmdList.Transition(FRHITransitionInfo(Src, ERHIAccess::Unknown, ERHIAccess::CopySrc));
//			RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::Unknown, ERHIAccess::CopyDest));
//
//			FRHICopyTextureInfo CopyInfo;
//			CopyInfo.SourcePosition = FIntVector(CellOrigin.X, CellOrigin.Y, 0);
//			CopyInfo.DestPosition = FIntVector(0, 0, 0);
//			CopyInfo.Size = FIntVector(CellSize, CellSize, 1);
//			RHICmdList.CopyTexture(Src, Dst, CopyInfo);
//
//			// コピー後、マスターは読み取り系へ、スナップは描画先へ
//			RHICmdList.Transition(FRHITransitionInfo(Src, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
//			RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::CopyDest, ERHIAccess::RTV));
//
//			// ② コピーしたセル画像の上に線・マークを描く（★Clearしない＝上描き）
//			FCanvas Canvas(SnapRes, nullptr, FGameTime(), GMaxRHIFeatureLevel);
//			const FHitProxyId HitId = Canvas.GetHitProxyId();
//
//			if (Points.Num() >= 2)
//			{
//				FBatchedElements* LineBatch = Canvas.GetBatchedElements(FCanvas::ET_Line);
//				LineBatch->AddReserveLines(Points.Num() - 1, false, /*bThickLines=*/true);
//				for (int32 i = 1; i < Points.Num(); ++i)
//				{
//					LineBatch->AddLine(
//						FVector(Points[i - 1].X, Points[i - 1].Y, 0.f),
//						FVector(Points[i].X, Points[i].Y, 0.f),
//						FLinearColor::Red, HitId, 2.0f);
//				}
//			}
//
//			if (MarkTex && MarkPoints.Num() > 0)
//			{
//				const ESimpleElementBlendMode BlendMode = SE_BLEND_Masked;
//				FBatchedElements* SpriteBatch =
//					Canvas.GetBatchedElements(FCanvas::ET_Triangle, nullptr, MarkTex, BlendMode);
//				for (const FVector2D& M : MarkPoints)
//				{
//					SpriteBatch->AddSprite(
//						FVector(M.X, M.Y, 0.f), 12.f, 12.f, MarkTex,
//						FLinearColor::White, HitId,
//						0.f, 0.f, 0.f, 0.f, (uint8)BlendMode, 0.33f);
//				}
//			}
//
//			Canvas.Flush_RenderThread(RHICmdList);
//
//			// ③ 描き終わったスナップショットRTをそのまま非同期Readback依頼
//			m_readback.EnqueueCopy(RHICmdList, Dst);
//		});
//}

//void AManagerActor::SaveSnapshotPNG()
//{
//	int32 RowPitchInPixels = 0;
//	void* Data = m_readback.Lock(RowPitchInPixels);
//	if (!Data) { UE_LOG(LogTemp, Error, TEXT("[Snapshot] Lock失敗")); return; }
//
//	const int32 W = m_layout.CellSize, H = m_layout.CellSize;
//	TArray<FColor> Pixels;
//	Pixels.SetNumUninitialized(W * H);
//	const FColor* SrcPx = reinterpret_cast<const FColor*>(Data);
//	for (int32 Y = 0; Y < H; ++Y)
//	{
//		FMemory::Memcpy(&Pixels[Y * W], &SrcPx[Y * RowPitchInPixels], W * sizeof(FColor));
//	}
//	m_readback.Unlock();
//
//	// PNG圧縮とディスク書き込みは重いのでワーカースレッドへ（ヒッチ回避）
//	Async(EAsyncExecution::ThreadPool,
//		[Pixels = MoveTemp(Pixels), W, H, Path = m_snapshotOutputPath]() mutable
//		{
//			FImageView ImageView(Pixels.GetData(), W, H, ERawImageFormat::BGRA8);
//			FImageUtils::SaveImageByExtension(*Path, ImageView);
//		});
//}

void AManagerActor::ProcessSnapshotQueues()
{
	TArray<FSnapshotRequest> ShotsThisFrame; // 今フレーム撮る（差し替え済みの先頭）セル群

	for (int32 cell = 0; cell < m_cellQueues.Num(); ++cell)
	{
		TArray<FSnapshotRequest>& Queue = m_cellQueues[cell];
		if (Queue.Num() == 0) continue;

		FSnapshotRequest& Head = Queue[0];

		if (Head.bSwapped)
		{
			// 既に差し替え済み（前フレームで差し替えた）→ 今フレームのアトラスに正しい絵が乗っている
			// ★差し替えたフレームより「後」のフレームでのみ撮る（アトラス反映を1フレーム待つ）
			if (GFrameCounter > Head.SwapFrame)
			{
				ShotsThisFrame.Add(Head);
			}
			// まだ同フレームなら今回は撮らない（次フレームで撮る）
			// ※キューからの除去は「撮影＝Readback発行後」に行う（下のEnqueue内で実施）
		}
		else
		{
			// 未差し替え → ここで差し替え（次フレームのアトラスに反映される）
			SetCellTarget(cell, Head.TargetActor);
			if (m_followerComponents.IsValidIndex(cell) && m_followerComponents[cell])
			{
				m_followerComponents[cell]->SetWorldRotation(Head.Rotation); // 指定の向きで撮る
			}
			Head.bSwapped = true;
			Head.SwapFrame = GFrameCounter;
			// このフレームでは撮らない（次フレームでShotsThisFrame入り）
		}
	}

	if (ShotsThisFrame.Num() > 0)
	{
		EnqueueAtlasReadback(ShotsThisFrame);

		// 撮影発行したぶんを各セルキューの先頭から除去し、次の先頭があれば即差し替え
		for (const FSnapshotRequest& Shot : ShotsThisFrame)
		{
			TArray<FSnapshotRequest>& Queue = m_cellQueues[Shot.CellIndex];
			if (Queue.Num() > 0) Queue.RemoveAt(0);

			if (Queue.Num() > 0)
			{
				// 次の先頭を「その場で差し替え」して次フレーム撮影に備える
				FSnapshotRequest& Next = Queue[0];
				SetCellTarget(Shot.CellIndex, Next.TargetActor);
				if (m_followerComponents.IsValidIndex(Shot.CellIndex) && m_followerComponents[Shot.CellIndex])
				{
					m_followerComponents[Shot.CellIndex]->SetWorldRotation(Next.Rotation);
				}
				Next.bSwapped = true;
				Next.SwapFrame = GFrameCounter;   // ★次の先頭も記録
			}
			else
			{
				// このセルは撮り終わった → 本来狙っているアクタへ戻す
				AActor* Def = m_cellDefaultTargets.IsValidIndex(Shot.CellIndex)
					? m_cellDefaultTargets[Shot.CellIndex] : nullptr;
				SetCellTarget(Shot.CellIndex, Def);
				// 戻したフォロワーの回転は、リアルタイム表示用の向きに戻す
				// （プレイヤーカメラ角度を再現する回転を別途持っているならそれを適用）
			}
		}
	}
}

void AManagerActor::EnqueueAtlasReadback(const TArray<FSnapshotRequest>& ShotsThisFrame)
{
	if (!m_masterRenderTarget) return;

	TSharedPtr<FSnapshotReadbackJob> Job = MakeShared<FSnapshotReadbackJob>();
	Job->Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("AtlasSnapshotReadback"));
	Job->Requests = ShotsThisFrame; // このReadbackで保存する対象（セル＋保存パス）

	FTextureRenderTargetResource* MasterRes = m_masterRenderTarget->GameThread_GetRenderTargetResource();
	FRHIGPUTextureReadback* ReadbackPtr = Job->Readback.Get();

	ENQUEUE_RENDER_COMMAND(AtlasReadbackCmd)(
		[MasterRes, ReadbackPtr](FRHICommandListImmediate& RHICmdList)
		{
			if (FRHITexture* Tex = MasterRes ? MasterRes->GetRenderTargetTexture() : nullptr)
			{
				ReadbackPtr->EnqueueCopy(RHICmdList, Tex); // アトラス全体を1回コピー
			}
		});

	m_readbackJobs.Add(Job);
}

void AManagerActor::SaveFromAtlas(FSnapshotReadbackJob& Job)
{
	int32 RowPitchInPixels = 0;
	void* Data = Job.Readback->Lock(RowPitchInPixels);
	if (!Data) { UE_LOG(LogTemp, Error, TEXT("[Snapshot] Atlas Lock失敗")); return; }

	const int32 CS = m_layout.CellSize;
	const FColor* Atlas = reinterpret_cast<const FColor*>(Data);

	for (const FSnapshotRequest& Req : Job.Requests)
	{
		const int32 Col = Req.CellIndex % m_layout.Cols;
		const int32 Row = Req.CellIndex / m_layout.Cols;
		const FIntPoint O = m_layout.CellPixelOrigin(Col, Row);

		// セル領域(CS×CS)を切り出し
		TArray<FColor> Cell;
		Cell.SetNumUninitialized(CS * CS);
		for (int32 y = 0; y < CS; ++y)
		{
			const FColor* SrcRow = &Atlas[(O.Y + y) * RowPitchInPixels + O.X];
			FMemory::Memcpy(&Cell[y * CS], SrcRow, CS * sizeof(FColor));
		}

		// Cell 切り出し直後、Async に渡す前
		const FColor Center = Cell[(CS / 2) * CS + (CS / 2)];
		UE_LOG(LogTemp, Log, TEXT("[Snapshot] Cell=%d centerPixel=(R=%d G=%d B=%d A=%d)"),
			Req.CellIndex, Center.R, Center.G, Center.B, Center.A);

		for (FColor& Px : Cell) { Px.A = 255; }

		// 着弾点をセルローカルピクセルへ変換（GameThread上で。フォロワーTransform参照のため）
		TArray<FIntPoint> HitPixels;
		for (const FVector& LocalHit : Req.LocalHitPoints)
		{
			const FVector2D P = CellLocalPixelFromLocalHit(Req.CellIndex, LocalHit);
			HitPixels.Add(FIntPoint(FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y)));
		}

		// 切り出し済みピクセル＋着弾ピクセルをワーカースレッドへ渡して、描画＋保存
		Async(EAsyncExecution::ThreadPool,
			[Cell = MoveTemp(Cell), CS, HitPixels = MoveTemp(HitPixels),
			Mark = m_markPixels, MW = m_markW, MH = m_markH, DrawSize = m_markDrawSize,
			Path = Req.OutputPath]() mutable
			{
				for (const FIntPoint& HP : HitPixels)
				{
					BlitMarkScaledCPU(Cell, CS, CS, Mark, MW, MH, HP, DrawSize);
				}
				FImageView ImageView(Cell.GetData(), CS, CS, ERawImageFormat::BGRA8);
				FImageUtils::SaveImageByExtension(*Path, ImageView);
			});
	}

	Job.Readback->Unlock();
}

float AManagerActor::GetTargetBoundsExtentSize(AActor* Target) const
{
	if (!Target) return 0.f;

	// パネル（子）を含めず、本体スケルタルメッシュのバウンズだけを使う
	USkeletalMeshComponent* Mesh = Target->FindComponentByClass<USkeletalMeshComponent>();
	if (Mesh)
	{
		const FBoxSphereBounds B = Mesh->Bounds;
		const FVector FullSize = B.BoxExtent * 2.f;
		const float MaxSize = FMath::Max3(FullSize.X, FullSize.Y, FullSize.Z);
		UE_LOG(LogTemp, Log, TEXT("[Bounds] %s mesh 最大辺=%.1f"), *Target->GetName(), MaxSize);
		return MaxSize;
	}
	return 0.f;
}

void AManagerActor::ApplyFitScale(int32 CellIndex, AActor* Target)
{
	if (!m_followerComponents.IsValidIndex(CellIndex) || !m_followerComponents[CellIndex]) return;
	USkeletalMeshComponent* Follower = m_followerComponents[CellIndex];

	const float BoundsSize = GetTargetBoundsExtentSize(Target);
	if (BoundsSize <= KINDA_SMALL_NUMBER)
	{
		Follower->SetWorldScale3D(FVector(1.f));
		return;
	}

	// セルのワールド一辺(cm) = CellSize（1cm/px）。その fillRatio ぶんに最大辺を収める。
	const float CellWorld = (float)m_layout.CellSize;
	const float Scale = (CellWorld * m_cellFillRatio / BoundsSize) * m_scaleMultiplier;

	Follower->SetWorldScale3D(FVector(Scale));

	UE_LOG(LogTemp, Log,
		TEXT("[FitScale] Cell=%d Bounds=%.1f CellWorld=%.1f fill=%.2f mult=%.2f → Scale=%.4f"),
		CellIndex, BoundsSize, CellWorld, m_cellFillRatio, m_scaleMultiplier, Scale);
}

FVector2D AManagerActor::CellLocalPixelFromLocalHit(int32 CellIndex, const FVector& LocalHit) const
{
	if (!m_followerComponents.IsValidIndex(CellIndex) || !m_followerComponents[CellIndex])
		return FVector2D(-1.f, -1.f);

	USkeletalMeshComponent* Follower = m_followerComponents[CellIndex];

	// ① リーダーローカルの着弾点を、フォロワーのワールド変換（位置・回転・スケール込み）で再構成
	const FVector WorldOnFollower = Follower->GetComponentTransform().TransformPosition(LocalHit);

	// ② 正射影でアトラスピクセルへ（1cm/px）
	const FVector C = m_sceneCapture2D->GetComponentLocation();
	const FVector R = m_sceneCapture2D->GetRightVector();
	const FVector U = m_sceneCapture2D->GetUpVector();
	const float MW = (float)m_layout.MasterWidth();
	const float MH = (float)m_layout.MasterHeight();

	const FVector D = WorldOnFollower - C;
	const float atlasX = MW * 0.5f + FVector::DotProduct(D, R);
	const float atlasY = MH * 0.5f - FVector::DotProduct(D, U); // 上(+U)は画面上＝y小さい

	// ③ セル原点を引いてローカルへ
	const int32 Col = CellIndex % m_layout.Cols;
	const int32 Row = CellIndex / m_layout.Cols;
	const FIntPoint O = m_layout.CellPixelOrigin(Col, Row);
	return FVector2D(atlasX - O.X, atlasY - O.Y);
}

void AManagerActor::LoadMarkTextureToCPU()
{
	m_markPixels.Reset();
	m_markW = m_markH = 0;

	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *m_markTexturePath);
	if (!Tex)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mark] テクスチャのロード失敗: %s"), *m_markTexturePath);
		return;
	}

#if WITH_EDITORONLY_DATA
	// エディタ/PIE では Source から確実にCPUピクセルを取得できる
	 FImage Img;
    if (Tex->Source.IsValid() && Tex->Source.GetMipImage(Img, 0))
    {
        Img.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
        m_markW = Img.GetWidth();
        m_markH = Img.GetHeight();

        // AsBGRA8() は TArrayView（参照）なので、実体へコピーして保持する
        const TArrayView<const FColor> View = Img.AsBGRA8();
        m_markPixels = TArray<FColor>(View.GetData(), View.Num());

        UE_LOG(LogTemp, Log, TEXT("[Mark] 読込成功 %dx%d"), m_markW, m_markH);
        return;
    }
#endif

	UE_LOG(LogTemp, Warning, TEXT("[Mark] Source から取得できず（Cooked等）。別経路が必要"));
}

void AManagerActor::BlitMarkScaledCPU(TArray<FColor>& Dst, int32 DW, int32 DH, const TArray<FColor>& Mark, int32 MW, int32 MH, FIntPoint Center, int32 DrawSize)
{
	if (Mark.Num() == 0 || MW <= 0 || MH <= 0) return;

	const int32 OutW = (DrawSize > 0) ? DrawSize : MW;
	const int32 OutH = (DrawSize > 0) ? DrawSize : MH;
	const int32 ox = Center.X - OutW / 2;
	const int32 oy = Center.Y - OutH / 2;

	for (int32 y = 0; y < OutH; ++y)
	{
		for (int32 x = 0; x < OutW; ++x)
		{
			// 出力座標 → 元テクスチャ座標（最近傍サンプリング）
			const int32 sx = (OutW > 0) ? (x * MW / OutW) : 0;
			const int32 sy = (OutH > 0) ? (y * MH / OutH) : 0;
			const FColor& Src = Mark[sy * MW + sx];
			if (Src.A == 0) continue; // 透明はスキップ

			const int32 dx = ox + x, dy = oy + y;
			if (dx < 0 || dy < 0 || dx >= DW || dy >= DH) continue;

			FColor& D = Dst[dy * DW + dx];
			const float a = Src.A / 255.f;
			D.R = (uint8)(Src.R * a + D.R * (1 - a));
			D.G = (uint8)(Src.G * a + D.G * (1 - a));
			D.B = (uint8)(Src.B * a + D.B * (1 - a));
			D.A = 255; // 保存画像は不透明前提
		}
	}
}

void AManagerActor::UpdateFollowerRotations()
{
	if (!m_playerCamera || !m_sceneCapture2D) return;

	const FQuat CamQ = m_playerCamera->GetComponentQuat();
	const FQuat CaptureQ = m_sceneCapture2D->GetComponentQuat();
	const FQuat CamInv = CamQ.Inverse();

	for (int32 cell = 0; cell < m_followerComponents.Num(); ++cell)
	{
		USkeletalMeshComponent* Follower = m_followerComponents[cell];
		if (!Follower) continue;

		AActor* Target = m_cellRotationTargets.IsValidIndex(cell) ? m_cellRotationTargets[cell] : nullptr;
		if (!Target) continue;

		// カメラ空間でのアクタの向き → キャプチャ基準へ置き直す
		const FQuat ActorQ = Target->GetActorQuat();
		const FQuat RelToCamera = CamInv * ActorQ;          // カメラから見たアクタの相対回転
		const FQuat FollowerQ = CaptureQ * RelToCamera;   // それをキャプチャ基準で再現

		Follower->SetWorldRotation(FollowerQ);

		// 回転のあとにセンターに戻す
		const int32 Col = cell % m_layout.Cols;
		const int32 Row = cell / m_layout.Cols;
		const FVector CellCenter = GetCellCenterWorld(Col, Row, 500.f);

		const FVector LocalCenter = m_cellCenterLocal.IsValidIndex(cell) ? m_cellCenterLocal[cell] : FVector::ZeroVector;

		// 「ローカル中心」が現在の回転・スケールでどれだけ原点から離れるか
		const FVector RotatedScaledOffset =
			Follower->GetComponentTransform().TransformVector(LocalCenter); // 回転＋スケールを反映、平行移動なし

		// バウンズ中心がセル中心に来るよう原点を置く
		Follower->SetWorldLocation(CellCenter - RotatedScaledOffset);
		//PlaceFollowerCentered(cell);
	}
}

void AManagerActor::PlaceFollowerCentered(int32 CellIndex)
{
	USkeletalMeshComponent* Follower = m_followerComponents[CellIndex];
	if (!Follower) return;

	const int32 Col = CellIndex % m_layout.Cols;
	const int32 Row = CellIndex / m_layout.Cols;
	const FVector CellCenter = GetCellCenterWorld(Col, Row, 500.f);

	Follower->UpdateBounds();
	// 原点→バウンズ中心のズレ（現在の配置基準で測る）
	const FVector CenterOffset = Follower->Bounds.Origin - Follower->GetComponentLocation();
	// セル中心にバウンズ中心が来る絶対位置へ置き直す
	Follower->SetWorldLocation(CellCenter - CenterOffset);
}

void AManagerActor::CacheCenterOffsetLocal(int32 CellIndex)
{
	USkeletalMeshComponent* Follower = m_followerComponents[CellIndex];
	if (!Follower) return;
	Follower->UpdateBounds();
	if (m_cellCenterLocal.Num() != m_layout.Cols * m_layout.Rows)
		m_cellCenterLocal.SetNum(m_layout.Cols * m_layout.Rows);

	// ワールドのバウンズ中心を、フォロワーのローカル空間へ戻して保存
	const FVector WorldCenter = Follower->Bounds.Origin;
	m_cellCenterLocal[CellIndex] = Follower->GetComponentTransform().InverseTransformPosition(WorldCenter);
}

void AManagerActor::RecordTrailPoint(const FVector2D& Pixel)
{
	const float Now = GetWorld()->GetTimeSeconds();
	m_trailPoints.Add({ Now, Pixel });

	// 古い点は破棄（before+after より十分古いものは不要）
	const float KeepSec = m_trailBeforeSec + m_trailAfterSec + 1.0f;
	int32 RemoveCount = 0;
	for (const FTrailPoint& P : m_trailPoints)
	{
		if (Now - P.Time > KeepSec) ++RemoveCount; else break;
	}
	if (RemoveCount > 0) m_trailPoints.RemoveAt(0, RemoveCount);
}

void AManagerActor::RequestTrailSave(int32 CellIndex, const FString& Path)
{
	FTrailSaveRequest Req;
	Req.HitTime = GetWorld()->GetTimeSeconds();
	Req.CellIndex = CellIndex;
	Req.OutputPath = Path;
	m_trailSaveQueue.Add(Req);
}

void AManagerActor::ProcessTrailSaveQueue()
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (int32 i = m_trailSaveQueue.Num() - 1; i >= 0; --i)
	{
		FTrailSaveRequest& Req = m_trailSaveQueue[i];
		if (Now < Req.HitTime + m_trailAfterSec) continue; // まだ2秒経っていない

		// 基準時刻の前後範囲の点を、青/赤に振り分けて線分リスト化
		const float TMin = Req.HitTime - m_trailBeforeSec;
		const float TMax = Req.HitTime + m_trailAfterSec;

		TArray<FVector2D> Pts;   // 範囲内の点（時刻順）
		TArray<float>     Times; // 各点の時刻（色分け用）
		for (const FTrailPoint& P : m_trailPoints)
		{
			if (P.Time >= TMin && P.Time <= TMax)
			{
				Pts.Add(P.Pixel);
				Times.Add(P.Time);
			}
		}

		const int32 CS = m_layout.CellSize;
		const float HitTime = Req.HitTime;
		const int32 Thickness = m_trailThickness;
		const FString Path = Req.OutputPath;

		// 描画＋保存はワーカースレッドへ
		Async(EAsyncExecution::ThreadPool,
			[Pts = MoveTemp(Pts), Times = MoveTemp(Times), CS, HitTime, Thickness, Path]() mutable
			{
				TArray<FColor> Buf;
				Buf.Init(FColor(0, 0, 0, 0), CS * CS); // 透過背景

				for (int32 k = 1; k < Pts.Num(); ++k)
				{
					// 線分の色は「後ろ側の点の時刻」で判定（着弾より後なら赤、前なら青）
					const bool bAfter = (Times[k] > HitTime);
					const FColor Col = bAfter ? FColor(255, 0, 0, 255) : FColor(0, 0, 255, 255);

					DrawThickLineCPU(Buf, CS, CS,
						FIntPoint(FMath::RoundToInt(Pts[k - 1].X), FMath::RoundToInt(Pts[k - 1].Y)),
						FIntPoint(FMath::RoundToInt(Pts[k].X), FMath::RoundToInt(Pts[k].Y)),
						Col, Thickness);
				}

				FImageView ImageView(Buf.GetData(), CS, CS, ERawImageFormat::BGRA8);
				FImageUtils::SaveImageByExtension(*Path, ImageView);
			});

		m_trailSaveQueue.RemoveAt(i);
	}
}



//https://claude.ai/share/891bba44-837d-4d94-bbcd-4725dbbcf6be
//https://claude.ai/share/86a5c282-4bb3-45ae-830d-c2288e5df2d5