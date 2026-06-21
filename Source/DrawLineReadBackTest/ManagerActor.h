// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RHIGPUReadback.h"
#include "GameFramework/Actor.h"
#include <Components/SceneCaptureComponent2D.h>
#include "Engine/TextureRenderTarget2D.h"
#include <Camera/CameraComponent.h>
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

	// 着弾点（リーダーアクタのローカル座標）。複数当たりに備え配列
	UPROPERTY(BlueprintReadWrite) TArray<FVector> LocalHitPoints;

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

// 1点の記録（時刻＋セルローカルピクセル）
struct FTrailPoint
{
	float Time = 0.f;          // ワールド時刻（GetWorld()->GetTimeSeconds()）
	FVector2D Pixel;           // 記録時のセルローカルピクセル
};

// 軌跡保存の予約（着弾時に積み、2秒後に発火）
struct FTrailSaveRequest
{
	float HitTime = 0.f;       // この着弾の時刻（色分けの基準）
	int32 CellIndex = 0;
	FString OutputPath;        // 軌跡画像の保存先
	bool bFired = false;
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

	// アクタからバウンズ寸法（cm, 直径相当）を取得。近傍パネル基準にしたくなったらここを差し替える
	float GetTargetBoundsExtentSize(AActor* Target) const;

	// セル収めスケールを計算してフォロワーへ適用
	void ApplyFitScale(int32 CellIndex, AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Capture")
	static FVector WorldHitToActorLocal(AActor* LeaderActor, const FVector& WorldHit)
	{
		// 着弾検知時：リーダーの「メッシュコンポーネント」ローカルへ
		USkeletalMeshComponent* LeaderMesh = LeaderActor->FindComponentByClass<USkeletalMeshComponent>();
		const FVector LocalHit = LeaderMesh
			? LeaderMesh->GetComponentTransform().InverseTransformPosition(WorldHit)
			: WorldHit;
		return LocalHit;
	}

	// 着弾点 → セルローカルピクセル変換
	FVector2D CellLocalPixelFromLocalHit(int32 CellIndex, const FVector& LocalHit) const;

	void LoadMarkTextureToCPU();   // 起動時に一度だけ呼ぶ

	static void BlitMarkScaledCPU(TArray<FColor>& Dst, int32 DW, int32 DH, const TArray<FColor>& Mark, int32 MW, int32 MH, FIntPoint Center, int32 DrawSize);   // DrawSize<=0 なら等倍

	void UpdateFollowerRotations(); // 毎フレーム：各フォロワーの回転をカメラ基準で更新

	void PlaceFollowerCentered(int32 CellIndex);

	void CacheCenterOffsetLocal(int32 CellIndex);

	void RecordTrailPoint(const FVector2D& Pixel);     // 毎フレーム（テストでは着弾時）に呼ぶ
	void RequestTrailSave(int32 CellIndex, const FString& Path); // 着弾時に予約
	void ProcessTrailSaveQueue();                      // 毎Tick：2秒経過したら発火

	// 対象プレイヤーカメラ（BPから設定）。とりあえず1台。将来プレイヤー人数分なら配列化。
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture")
	UCameraComponent* m_playerCamera = nullptr;

	// セルごとの「向き合わせ対象アクタ」（＝そのセルに映している実アクタ＝リーダー）
	// リアルタイム表示中はこれを見て毎フレーム回転を合わせる
	UPROPERTY()
	TArray<TObjectPtr<AActor>> m_cellRotationTargets;

	// 全体スケール調整用（後から見た目を一括で詰めたいとき用）
	UPROPERTY(EditAnywhere, Category = "Capture")
	float m_cellFillRatio = 0.8f;   // セルに対してバウンズ最大辺をどれだけ占めるか（余白確保）

	UPROPERTY(EditAnywhere, Category = "Capture")
	float m_scaleMultiplier = 1.0f; // さらに全体を微調整したいときの係数

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

	// マーク画像（着弾点）のリテラルパス。エディタで差し替えたいなら EditAnywhere のままでOK
	UPROPERTY(EditAnywhere, Category = "Capture")
	FString m_markTexturePath = TEXT("/Game/texture1.texture1");

	// 着弾マークの描画サイズ（px、1辺）。0以下なら元画像サイズで描く
	UPROPERTY(EditAnywhere, Category = "Capture")
	int32 m_markDrawSize = 32;

	// 起動時にCPUへ展開したマーク画像（ワーカースレッドで参照する）
	TArray<FColor> m_markPixels;
	int32 m_markW = 0;
	int32 m_markH = 0;

	TArray<FVector> m_cellCenterLocal; // BeginPlayで SetNum(セル数)

	TArray<FTrailPoint> m_trailPoints;          // 狙い点の履歴（時刻順）
	TArray<FTrailSaveRequest> m_trailSaveQueue; // 2秒後発火待ち

	UPROPERTY(EditAnywhere, Category = "Trail")
	float m_trailBeforeSec = 3.0f;   // 着弾より前 何秒ぶん描くか（青）

	UPROPERTY(EditAnywhere, Category = "Trail")
	float m_trailAfterSec = 2.0f;    // 着弾より後 何秒ぶん描くか（赤）

	UPROPERTY(EditAnywhere, Category = "Trail")
	int32 m_trailThickness = 3;      // 線の太さ（px）

	int32 m_shotCounter = 0;

	UPROPERTY(EditAnywhere, Category = "Capture")
	FString m_outputDir = TEXT("H:\\HDriveUE5Projects\\Lancher5.3\\DrawLineReadBackTest\\");
};
