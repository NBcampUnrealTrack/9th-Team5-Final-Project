#include "MapSystem/UI/ASMapHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
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
}
