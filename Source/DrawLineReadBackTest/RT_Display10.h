// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/Image.h>
#include "RT_Display10.generated.h"

struct FCellLayout;

/**
 *
 */
UCLASS()
class DRAWLINEREADBACKTEST_API URT_Display10 : public UUserWidget
{
	GENERATED_BODY()

	virtual void NativeConstruct() override;

public:
	void BindRenderTarget(UTextureRenderTarget2D* RT);

	//void BindCell(UTextureRenderTarget2D* RT, int32 CellIndex, int32 Cols, int32 Rows);

	void BindCell(UTextureRenderTarget2D* RT, const FCellLayout& Layout);

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage1 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage2 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage3 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage4 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage5 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage6 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage7 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage8 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage9 = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* m_rtImage10 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UMaterialInterface> m_cellMaterial = nullptr;
};