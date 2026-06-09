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
#define DISPLAY_VERTICAL_CELLS 2
#define DISPLAY_HORIZONTAL_CELLS 5
#define RT_CELL_SIZE 512
#define MASTER_RT_WIDTH (RT_CELL_SIZE * DISPLAY_HORIZONTAL_CELLS) // 2560
#define MASTER_RT_HEIGHT (RT_CELL_SIZE * DISPLAY_VERTICAL_CELLS)   // 1024


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
	m_sceneCapture2D->OrthoWidth = MASTER_RT_WIDTH;
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

		//RTを作成
		m_masterRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			this,
			MASTER_RT_WIDTH,
			MASTER_RT_HEIGHT,
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
			rtDisplayWidget->BindCell(m_masterRenderTarget,DISPLAY_HORIZONTAL_CELLS, DISPLAY_VERTICAL_CELLS);//左下
			//rtDisplayWidget->BindCell(m_masterRenderTarget,1,1,1);
		}
	}

	// PoseableMeshComponentを配置
	{
		const int32 cellCount = DISPLAY_HORIZONTAL_CELLS * DISPLAY_VERTICAL_CELLS; // 10
		for (int32 i = 0; i < cellCount; i++)
		{
			const int32 Col = i % DISPLAY_HORIZONTAL_CELLS;
			const int32 Row = i / DISPLAY_HORIZONTAL_CELLS;

			UPoseableMeshComponent* pose = NewObject<UPoseableMeshComponent>(this);
			pose->RegisterComponent();
			pose->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			pose->SetWorldLocation(GetCellCenterWorld(Col, Row, DISPLAY_HORIZONTAL_CELLS, DISPLAY_VERTICAL_CELLS, 500.f));

			// ★テスト用にメッシュを割り当て（これが無いと何も描画されない）
			if (m_testMesh)
			{
				pose->SetSkinnedAssetAndUpdate(m_testMesh);
			}

			// メインビューには出さず、キャプチャにだけ映す
			//pose->SetVisibleInSceneCaptureOnly(true);
			// このキャプチャの ShowOnly 対象に登録（他は撮らない）
			//m_sceneCapture2D->ShowOnlyComponents.Add(pose);

			m_poseComponents.Add(pose);
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

FVector AManagerActor::GetCellCenterWorld(int32 Col, int32 Row, int32 Cols, int32 Rows, float DistanceInFront) const
{
	const FVector C = m_sceneCapture2D->GetComponentLocation();
	const FVector F = m_sceneCapture2D->GetForwardVector(); // 視線方向
	const FVector R = m_sceneCapture2D->GetRightVector();    // 画面の右
	const FVector U = m_sceneCapture2D->GetUpVector();       // 画面の上

	const float W = m_sceneCapture2D->OrthoWidth;                                   // 横の撮影幅(cm)
	const float H = W * ((float)MASTER_RT_HEIGHT / (float)MASTER_RT_WIDTH);          // 縦は RT アスペクトから

	// セル中心の正規化位置（-0.5〜+0.5）
	const float rightFrac = ((Col + 0.5f) / Cols) - 0.5f;   // 左→右で増加
	const float upFrac = 0.5f - ((Row + 0.5f) / Rows);// RT上は row0 が一番上 → +U が正

	return C + F * DistanceInFront + R * (rightFrac * W) + U * (upFrac * H);
}