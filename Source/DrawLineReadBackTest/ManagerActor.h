// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RHIGPUReadback.h"
#include "GameFramework/Actor.h"
#include <Components/SceneCaptureComponent2D.h>
#include "Engine/TextureRenderTarget2D.h"
#include "ManagerActor.generated.h"

class UWidget;
class UPoseableMeshComponent;

enum class ESnapshotState : uint8 
{
	Idle,
	PendingReadback,
	WaitingReadback
};

// 1回の撮影リクエスト（今後フィールド追加しやすいよう構造体に）
USTRUCT(BlueprintType)
struct FSnapshotRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) int32 CellIndex = 0;
	UPROPERTY(BlueprintReadWrite) AActor* TargetActor = nullptr;
	UPROPERTY(BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadWrite) FString OutputPath;

	uint64 SwapFrame = 0;   // 差し替えを行ったGFrameCounter

	// 内部進行状態（差し替え済みか）
	bool bSwapped = false;
};

// 進行中の1回ぶんのReadback（複数本並行用）
struct FSnapshotReadbackJob
{
	TUniquePtr<FRHIGPUTextureReadback> Readback;
	TArray<FSnapshotRequest> Requests; // この1回のReadbackに含まれる撮影対象（セルごと）
};

USTRUCT(BlueprintType)
struct FCellLayout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) int32 Cols = 5;
	UPROPERTY(EditAnywhere) int32 Rows = 2;
	UPROPERTY(EditAnywhere) int32 CellSize = 512;   // UV切り取りで担保する解像度(px)
	UPROPERTY(EditAnywhere) int32 Margin = 0;       // セル間の余白(px)
	UPROPERTY(EditAnywhere) int32 EdgeMargin = 0;   // 外周の余白(px) 0でもOK

	int32 MasterWidth()  const { return EdgeMargin * 2 + CellSize * Cols + Margin * FMath::Max(0, Cols - 1); }
	int32 MasterHeight() const { return EdgeMargin * 2 + CellSize * Rows + Margin * FMath::Max(0, Rows - 1); }

	// セル中身の左上ピクセル座標
	FIntPoint CellPixelOrigin(int32 Col, int32 Row) const
	{
		return FIntPoint(EdgeMargin + Col * (CellSize + Margin),
			EdgeMargin + Row * (CellSize + Margin));
	}

	// 正規化UV（スケールとオフセット）。余白を考慮してセル中身だけを指す
	void GetCellUV(int32 Col, int32 Row, FVector2D& OutScale, FVector2D& OutOffset) const
	{
		const FIntPoint O = CellPixelOrigin(Col, Row);
		const float W = (float)MasterWidth();
		const float H = (float)MasterHeight();
		OutScale = FVector2D((float)CellSize / W, (float)CellSize / H);
		OutOffset = FVector2D((float)O.X / W, (float)O.Y / H);
	}
};

UCLASS()
class DRAWLINEREADBACKTEST_API AManagerActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AManagerActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	TSharedPtr<class SOverlay> CreateWindow(const FVector2D& windowSize, const FVector2D& windowPos, const FText& tilte);
	void SetWidget(UWidget* setWidget, TSharedPtr<class SOverlay> overlay);
	
	UFUNCTION(BlueprintCallable)
	void SetCaptureEnabled(bool bEnabled);

	FVector GetCellCenterWorld(int32 Col, int32 Row, float DistanceInFront) const;

	UFUNCTION(BlueprintCallable)
	void SetCellTarget(int32 CellIndex, AActor* NewTarget);

	UFUNCTION(BlueprintCallable)
	void TestSnapShot(const FSnapshotRequest& Request);   // 1回1セルぶんを積む

	//void EnqueueSnapshotDrawAndReadback(int32 CellIndex);        // 引数追加

	//void SaveSnapshotPNG();

	void ProcessSnapshotQueues();                          // 毎Tick：差し替え進行＋撮影発行
	void EnqueueAtlasReadback(const TArray<FSnapshotRequest>& ShotsThisFrame); // 全体1Readback
	void SaveFromAtlas(FSnapshotReadbackJob& Job);         // 完了後：全体から各セル切り出し保存

	UPROPERTY(EditAnywhere, Category = "Layout")
	FCellLayout m_layout;     // 追加（エディタで余白等を設定）

	TSubclassOf<UUserWidget> m_mtRtWidgetClass;
	TSubclassOf<UUserWidget> m_rt10WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Capture")
	TObjectPtr<USkeletalMesh> m_testMesh = nullptr; // エディタでマネキン等を割当

	UPROPERTY(EditAnywhere, Category = "Capture")
	AActor* m_captureActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Capture")
	AActor* m_captureActor2 = nullptr;

	TArray<TSharedPtr<SWindow>> m_windows; // 生成したウィンドウをまとめて保持

	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> m_followerComponents;

	USceneCaptureComponent2D* m_sceneCapture2D = NULL;
	UTextureRenderTarget2D* m_masterRenderTarget = NULL;

	// セルごとのキュー（添字＝セル番号）
	TArray<TArray<FSnapshotRequest>> m_cellQueues;

	// 並行で走っているReadbackジョブ群
	TArray<TSharedPtr<FSnapshotReadbackJob>> m_readbackJobs;

	// ヘッダ：セルごとの「本来狙っているアクタ」を保持
	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> m_cellDefaultTargets; // BeginPlayで SetNum(セル数)

	uint64 m_snapRequestFrame = 0;
	int32  m_snapCellIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Capture")
	TObjectPtr<UTexture2D> m_markTexture = nullptr;   // ×マーク等（エディタで割当。無くても線だけ描く）

	UPROPERTY(EditAnywhere, Category = "Capture")
	FString m_snapshotOutputPath = TEXT("H:\\HDriveUE5Projects\\Lancher5.3\\DrawLineReadBackTest\\TrajectoryReadback.png");
};
