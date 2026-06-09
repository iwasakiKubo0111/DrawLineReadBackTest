// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <Components/SceneCaptureComponent2D.h>
#include "Engine/TextureRenderTarget2D.h"
#include "ManagerActor.generated.h"

class UWidget;
class UPoseableMeshComponent;

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
	void SetCaptureEnabled(bool bEnabled);

	FVector GetCellCenterWorld(int32 Col, int32 Row, int32 Cols, int32 Rows, float DistanceInFront) const;

	TSubclassOf<UUserWidget> m_mtRtWidgetClass;
	TSubclassOf<UUserWidget> m_rt10WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Capture")
	TObjectPtr<USkeletalMesh> m_testMesh = nullptr; // エディタでマネキン等を割当

	TArray<TSharedPtr<SWindow>> m_windows; // 生成したウィンドウをまとめて保持

	UPROPERTY()
	TArray<TObjectPtr<UPoseableMeshComponent>> m_poseComponents;

	USceneCaptureComponent2D* m_sceneCapture2D = NULL;
	UTextureRenderTarget2D* m_masterRenderTarget = NULL;
};
