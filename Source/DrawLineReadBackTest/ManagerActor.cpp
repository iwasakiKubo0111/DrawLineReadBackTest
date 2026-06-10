// Fill out your copyright notice in the Description page of Project Settings.


#include "ManagerActor.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include <Kismet/KismetRenderingLibrary.h>
#include "RT_DisplayWidget.h"
#include "RT_Display10.h"
#include <Components/PoseableMeshComponent.h>

#define DPI_SCALE 1.5
#define WINDOW_HIGHT_SIZE 2160 / DPI_SCALE
#define WINDOW_WIDTH_SIZE 3840 / DPI_SCALE

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
			FLinearColor::Black,
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
	Super::Tick(DeltaTime);

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
}

//https://claude.ai/share/891bba44-837d-4d94-bbcd-4725dbbcf6be
//https://claude.ai/share/86a5c282-4bb3-45ae-830d-c2288e5df2d5