// Test harness for DrawTrajectoryAndRequest

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RHIGPUReadback.h"
#include "TrajectoryReadbackTester.generated.h"

class UTextureRenderTarget2D;
class UTexture2D;

/**
 * DrawTrajectoryAndRequest（線＋PNGマークをRTに描いてReadback）の動作確認用Actor。
 * レベルに配置してPlayすると、BeginPlayで描画と読み戻しを開始し、
 * Tickで IsReady を待ってPNGをローカル保存する。
 *
 * 必要モジュール（*.Build.cs の PublicDependencyModuleNames に追加）:
 *   "RHI", "RenderCore"   ※ "Core","CoreUObject","Engine" は既定で入っている前提
 */
UCLASS()
class ATrajectoryReadbackTester : public AActor
{
	GENERATED_BODY()

public:
	ATrajectoryReadbackTester();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	// ---- #5 既存の関数（テストでは自前で持たせている。実プロジェクト側に既にあるなら差し替え可）----
	void DrawTrajectoryAndRequest(
		UTextureRenderTarget2D* RT,
		const TArray<FVector2D>& InPoints,
		UTexture2D* InMarkTexture,
		const TArray<FVector2D>& InMarkPoints);

	// ---- #7 ピクセル受け取り → PNG保存 ----
	void ReadbackAndSavePNG();

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> MarkTexture = nullptr;

	// Readback 本体（FRHIGPUTextureReadback はコンストラクタで名前が必要）
	FRHIGPUTextureReadback Readback{ TEXT("TrajectoryReadback") };

	bool bWaitingForReadback = false;
	bool bDone = false;

	int32 RTWidth = 512;
	int32 RTHeight = 512;
};
