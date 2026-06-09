// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/Image.h>
#include "RT_DisplayWidget.generated.h"

/**
 * 
 */
UCLASS()
class DRAWLINEREADBACKTEST_API URT_DisplayWidget : public UUserWidget
{
	GENERATED_BODY()
	
	virtual void NativeConstruct() override;

public:
	void BindRenderTarget(UTextureRenderTarget2D* RT);

	void BindCell(UTextureRenderTarget2D* RT, int32 CellIndex, int32 Cols, int32 Rows);

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage1 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UMaterialInterface> m_cellMaterial = nullptr;
};
