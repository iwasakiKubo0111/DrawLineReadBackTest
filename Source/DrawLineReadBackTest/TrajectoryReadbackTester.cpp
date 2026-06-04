// Test harness for DrawTrajectoryAndRequest

#include "TrajectoryReadbackTester.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "CanvasTypes.h"
#include "BatchedElements.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

// ========================= 設定（リテラル：環境に合わせて差し替え）=========================
// uasset のオブジェクトパス（末尾に .アセット名 が付く形式）
//static const TCHAR* GMarkTexturePath = TEXT("/Game/Test/T_XMark.T_XMark");
static const TCHAR* GMarkTexturePath = TEXT("/Game/texture1.texture1");
// 出力先（フルパス）。フォルダは存在している前提
//static const FString GOutputPath = TEXT("C:/Temp/TrajectoryReadback.png");
static const FString GOutputPath = TEXT("H:\\HDriveUE5Projects\\Lancher5.3\\DrawLineReadBackTest\\TrajectoryReadback.png");
// ==========================================================================================

ATrajectoryReadbackTester::ATrajectoryReadbackTester()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATrajectoryReadbackTester::BeginPlay()
{
	Super::BeginPlay();

	// 1. レンダーターゲット作成（RTF_RGBA8 = PF_B8G8R8A8 → FColor(BGRA) で読み戻せる）
	//RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, RTWidth, RTHeight, RTF_RGBA8, FLinearColor::Black, /*bAutoGenerateMips=*/false, /*bSupportUAVs=*/false);
	
	//透過対応版
	RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, RTWidth, RTHeight, RTF_RGBA8, FLinearColor::Transparent, false, false);

	if (!RenderTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("[Tester] RenderTarget の作成に失敗"));
		return;
	}

	// 2. InPoints 用のランダム点（連続軌跡っぽくするためランダムウォーク）
	TArray<FVector2D> Points;
	{
		const int32 NumPoints = 3000;
		FVector2D P(FMath::FRandRange(0.f, (float)RTWidth), FMath::FRandRange(0.f, (float)RTHeight));
		Points.Reserve(NumPoints);
		for (int32 i = 0; i < NumPoints; ++i)
		{
			P += FVector2D(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f));
			P.X = FMath::Clamp(P.X, 0.f, (float)RTWidth);
			P.Y = FMath::Clamp(P.Y, 0.f, (float)RTHeight);
			Points.Add(P);
		}
	}

	// 3. テクスチャを uasset パスからロード（UTexture でロード → UTexture2D にキャスト）
	if (UTexture* Loaded = LoadObject<UTexture>(nullptr, GMarkTexturePath))
	{
		MarkTexture = Cast<UTexture2D>(Loaded);
	}
	if (!MarkTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tester] MarkTexture のロード/キャストに失敗: %s（マーク無しで続行）"), GMarkTexturePath);
	}
	else
	{
		// 描画時に必ずミップが乗っているように（テスト用途の保険）
		MarkTexture->SetForceMipLevelsToBeResident(30.0f);
	}

	// 4. InMarkPoints 用のランダム点
	TArray<FVector2D> MarkPoints;
	{
		const int32 NumMarks = 10;
		MarkPoints.Reserve(NumMarks);
		for (int32 i = 0; i < NumMarks; ++i)
		{
			MarkPoints.Add(FVector2D(
				FMath::FRandRange(0.f, (float)RTWidth),
				FMath::FRandRange(0.f, (float)RTHeight)));
		}
	}

	// 5. 既存の関数へ渡す（描画＋EnqueueCopy）
	DrawTrajectoryAndRequest(RenderTarget, Points, MarkTexture, MarkPoints);
	bWaitingForReadback = true;
}

void ATrajectoryReadbackTester::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 6. IsReady を毎フレーム非ブロッキングで確認（busy-while の安全版）。
	//    GameThread を止めず、完了フレームで一度だけ読み出す。
	if (bWaitingForReadback && !bDone && Readback.IsReady())
	{
		ReadbackAndSavePNG();
		bWaitingForReadback = false;
		bDone = true;
	}
}

// ============================================================================
// #5 既存の関数（前回作成したもの。実プロジェクトに同等品があるなら差し替え可）
// ============================================================================
void ATrajectoryReadbackTester::DrawTrajectoryAndRequest(
	UTextureRenderTarget2D* RT,
	const TArray<FVector2D>& InPoints,
	UTexture2D* InMarkTexture,
	const TArray<FVector2D>& InMarkPoints)
{
	if (!RT)
	{
		return;
	}

	FTextureRenderTargetResource* RTResource = RT->GameThread_GetRenderTargetResource();

	// × マークのテクスチャ描画リソースをゲームスレッドで取得して渡す
	const FTexture* MarkTex =
		(InMarkTexture && InMarkTexture->GetResource()) ? InMarkTexture->GetResource() : nullptr;

	ENQUEUE_RENDER_COMMAND(DrawTrajectoryAndRequestCmd)(
		[this, RTResource, Points = InPoints, MarkTex, MarkPoints = InMarkPoints]
		(FRHICommandListImmediate& RHICmdList)
		{
			FCanvas Canvas(RTResource, nullptr, FGameTime(), GMaxRHIFeatureLevel);
			
			//Canvas.Clear(FLinearColor::Black);

			//透過対応版
			Canvas.Clear(FLinearColor::Transparent);

			const FHitProxyId HitId = Canvas.GetHitProxyId();

			// (1) 軌跡の線：1つのラインバッチに集約
			if (Points.Num() >= 2)
			{
				FBatchedElements* LineBatch = Canvas.GetBatchedElements(FCanvas::ET_Line);
				LineBatch->AddReserveLines(Points.Num() - 1, false, /*bThickLines=*/true);

				const FLinearColor LineColor = FLinearColor::Red;
				const float Thickness = 2.0f;
				for (int32 i = 1; i < Points.Num(); ++i)
				{
					LineBatch->AddLine(
						FVector(Points[i - 1].X, Points[i - 1].Y, 0.f),
						FVector(Points[i].X,     Points[i].Y,     0.f),
						LineColor, HitId, Thickness);
				}
			}

			// (2) × マーク(PNG)：同一テクスチャ・同一ブレンドで1バッチ
			if (MarkTex && MarkPoints.Num() > 0)
			{
				const ESimpleElementBlendMode BlendMode = SE_BLEND_Masked;
				FBatchedElements* SpriteBatch =
					Canvas.GetBatchedElements(FCanvas::ET_Triangle, nullptr, MarkTex, BlendMode);

				const float HalfSize = 12.0f; // 中心からの半径（実寸は約2倍）。見た目で調整。
				for (const FVector2D& M : MarkPoints)
				{
					SpriteBatch->AddSprite(
						FVector(M.X, M.Y, 0.f),   // 中心位置
						HalfSize, HalfSize,       // 半サイズ
						MarkTex,
						FLinearColor::White,      // 無着色（テクスチャそのまま）
						HitId,
						0.f, 0.f, 0.f, 0.f,       // U,UL,V,VL = 0 → テクスチャ全体（テクセル単位）
						(uint8)BlendMode,
						0.33f);// ← Masked のしきい値（このαより低い画素は破棄＝透明のまま）
				}
			}

			// 線バッチ＋スプライトバッチをまとめて1回でフラッシュ
			Canvas.Flush_RenderThread(RHICmdList);

			// 描き終わった同じRTをそのまま読み戻し（同一コマンド内なので順序が保証される）
			if (FRHITexture2D* RHITexture = RTResource->GetRenderTargetTexture())
			{
				Readback.EnqueueCopy(RHICmdList, RHITexture);
			}
		});
}

// ============================================================================
// #7 ピクセル受け取り → PNG保存
// ============================================================================
void ATrajectoryReadbackTester::ReadbackAndSavePNG()
{
	// ★ここの Lock のシグネチャは RHIGPUReadback.h（RHIモジュール）で要確認。
	//   UE5系では void* Lock(int32& OutRowPitchInPixels, int32* OutBufferHeight = nullptr) を想定。
	int32 RowPitchInPixels = 0;
	void* Data = Readback.Lock(RowPitchInPixels);
	if (!Data)
	{
		UE_LOG(LogTemp, Error, TEXT("[Tester] Readback.Lock が null を返した"));
		return;
	}

	// RTF_RGBA8 → FColor(BGRA) 配列として読める。
	// 行ピッチ(RowPitchInPixels)が幅と異なる（パディングされる）場合があるため行ごとにコピー。
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(RTWidth * RTHeight);
	const FColor* Src = reinterpret_cast<const FColor*>(Data);
	for (int32 Y = 0; Y < RTHeight; ++Y)
	{
		FMemory::Memcpy(
			&Pixels[Y * RTWidth],
			&Src[Y * RowPitchInPixels],
			RTWidth * sizeof(FColor));
	}

	Readback.Unlock();

	// FColor 配列を PNG バイト列へ圧縮して保存
	//TArray<uint8> PNGData;
	//FImageUtils::CompressImageArray(RTWidth, RTHeight, Pixels, PNGData);

	//if (FFileHelper::SaveArrayToFile(PNGData, *GOutputPath))
	//{
	//	UE_LOG(LogTemp, Display, TEXT("[Tester] 保存成功: %s"), *GOutputPath);
	//}
	//else
	//{
	//	UE_LOG(LogTemp, Error, TEXT("[Tester] 保存失敗: %s（フォルダの存在/書込権限を確認）"), *GOutputPath); //https://claude.ai/share/9621d55c-9c3a-4493-819d-4f9164e3534d
	//}

	FImageView ImageView(Pixels.GetData(), RTWidth, RTHeight, ERawImageFormat::BGRA8);
	FImageUtils::SaveImageByExtension(*GOutputPath, ImageView);
}

