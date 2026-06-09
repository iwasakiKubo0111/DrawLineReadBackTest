// Fill out your copyright notice in the Description page of Project Settings.


#include "RT_DisplayWidget.h"
#include "Engine/TextureRenderTarget2D.h"

void URT_DisplayWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void URT_DisplayWidget::BindRenderTarget(UTextureRenderTarget2D* renderTarget)
{
    if (!m_rtImage1 || !renderTarget) return;

    FSlateBrush Brush;
    Brush.SetResourceObject(renderTarget);                       // RT をブラシのリソースに
    Brush.ImageSize = FVector2D(renderTarget->SizeX, renderTarget->SizeY);
    m_rtImage1->SetBrush(Brush);
}

void URT_DisplayWidget::BindCell(UTextureRenderTarget2D* renderTarget, int32 CellIndex, int32 Cols, int32 Rows)
{
    if (!m_rtImage1 || !m_cellMaterial) return;

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(m_cellMaterial, this);
    MID->SetTextureParameterValue(TEXT("RT"), renderTarget);

    const int32 Col = CellIndex % Cols;
    const int32 Row = CellIndex / Cols;

    // セル1枚ぶんに縮小し、対象セルの左上へオフセット
    MID->SetVectorParameterValue(TEXT("UVScale"),
        FLinearColor(1.f / Cols, 1.f / Rows, 0.f, 0.f));
    MID->SetVectorParameterValue(TEXT("UVOffset"),
        FLinearColor((float)Col / Cols, (float)Row / Rows, 0.f, 0.f));

    m_rtImage1->SetBrushFromMaterial(MID);
}