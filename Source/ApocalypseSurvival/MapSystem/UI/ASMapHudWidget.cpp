#include "MapSystem/UI/ASMapHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MapSystem/UI/ASMapViewModel.h"
#include "MapSystem/UI/ASMapViewModelSubsystem.h"
#include "MapSystem/Zone/Core/ASMapFunctionLibrary.h"

TSharedRef<SWidget> UASMapHudWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Root;

		if (UClass* MinimapClass = MinimapWidgetClass.LoadSynchronous())
		{
			if (UUserWidget* Minimap = WidgetTree->ConstructWidget<UUserWidget>(MinimapClass))
			{
				// 미니맵 자체 설정은 유지하고 바깥 상자로 크기만 고정한다.
				USizeBox* MinimapSize = WidgetTree->ConstructWidget<USizeBox>();
				MinimapSize->SetWidthOverride(256.0f);
				MinimapSize->SetHeightOverride(256.0f);
				MinimapSize->SetContent(Minimap);
				Root->AddChildToVerticalBox(MinimapSize)->SetHorizontalAlignment(HAlign_Right);
			}
		}

		if (UClass* ZoneHudClass = ZoneHudWidgetClass.LoadSynchronous())
		{
			if (UUserWidget* ZoneHud = WidgetTree->ConstructWidget<UUserWidget>(ZoneHudClass))
			{
				UVerticalBoxSlot* ZoneHudSlot = Root->AddChildToVerticalBox(ZoneHud);
				ZoneHudSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
				ZoneHudSlot->SetHorizontalAlignment(HAlign_Right);
			}
		}
	}
	return Super::RebuildWidget();
}

void UASMapHudWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 디자이너에서는 구역이 없어도 배치를 확인할 수 있게 표시한다.
	const UWorld* World = GetWorld();
	const bool bHideHud = !IsDesignTime() && World && World->IsGameWorld()
		&& !UASMapFunctionLibrary::LevelHasMapZones(this);
	SetVisibility(bHideHud ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!bHideHud && !IsDesignTime() && OwningPlayer && OwningPlayer->IsLocalPlayerController() && !ExpandedMapWidget)
	{
		if (UClass* ExpandedMapClass = ExpandedMapWidgetClass.LoadSynchronous())
		{
			ExpandedMapWidget = CreateWidget<UUserWidget>(OwningPlayer, ExpandedMapClass);
			if (ExpandedMapWidget)
			{
				// 팀 HUD(뷰포트 레이어, 인벤토리 바 10)보다 위, 게임 메뉴(20)보다 아래에 그린다.
				ExpandedMapWidget->AddToViewport(15);
				// 위치·크기 설정은 앵커를 좌상단으로 되돌리므로 늘이는 앵커만 지정한다.
				ExpandedMapWidget->SetAnchorsInViewport(FAnchors(0.1f, 0.1f, 0.9f, 0.9f));
				ExpandedMapWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UASMapHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ExpandedMapWidget)
	{
		// 뷰모델이 HUD보다 늦게 생성될 수 있어 매 틱 조회한다.
		const UASMapViewModelSubsystem* Subsystem = UASMapViewModelSubsystem::Get(this);
		const UASMapViewModel* ViewModel = Subsystem ? Subsystem->GetMapViewModel() : nullptr;
		const ESlateVisibility TargetVisibility = ViewModel ? ViewModel->GetExpandedMapVisibility() : ESlateVisibility::Collapsed;
		if (ExpandedMapWidget->GetVisibility() != TargetVisibility)
		{
			ExpandedMapWidget->SetVisibility(TargetVisibility);
		}
	}
}

void UASMapHudWidget::NativeDestruct()
{
	if (ExpandedMapWidget)
	{
		ExpandedMapWidget->RemoveFromParent();
		ExpandedMapWidget = nullptr;
	}

	Super::NativeDestruct();
}
