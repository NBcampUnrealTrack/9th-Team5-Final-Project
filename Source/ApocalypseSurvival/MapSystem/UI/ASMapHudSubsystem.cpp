#include "MapSystem/UI/ASMapHudSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MapSystem/Zone/Core/ASMapDefinition.h"
#include "MapSystem/Zone/Core/ASMapFunctionLibrary.h"
#include "TimerManager.h"
#include "Widgets/Layout/Anchors.h"

bool UASMapHudSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UASMapHudSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_DedicatedServer || !UASMapFunctionLibrary::LevelHasMapZones(&InWorld))
	{
		return;
	}

	// 로컬 플레이어와 컨트롤러, 맵 정의가 준비된 뒤 한 번만 생성한다.
	InWorld.GetTimerManager().SetTimer(WidgetTimer, this, &UASMapHudSubsystem::TryCreateWidgets, 0.25f, true);
}

void UASMapHudSubsystem::TryCreateWidgets()
{
	constexpr float MinimapSize = 256.0f;
	constexpr float Margin = 16.0f;

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->GetLocalPlayer())
	{
		return;
	}
	const UASMapDefinition* MapDefinition = UASMapFunctionLibrary::GetMapDefinition(World);
	if (!MapDefinition)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(WidgetTimer);

	if (UClass* MinimapClass = MapDefinition->MinimapWidgetClass.LoadSynchronous())
	{
		UUserWidget* Widget = CreateWidget<UUserWidget>(PlayerController, MinimapClass);
		if (Widget)
		{
			MinimapWidget = Widget;
			Widget->AddToPlayerScreen();
			Widget->SetPositionInViewport(FVector2D(-Margin, Margin), false);
			Widget->SetDesiredSizeInViewport(FVector2D(MinimapSize, MinimapSize));
			// 위치와 크기 설정이 앵커를 초기화하므로 마지막에 배치 기준을 적용한다.
			Widget->SetAnchorsInViewport(FAnchors(1, 0));
			Widget->SetAlignmentInViewport(FVector2D(1, 0));
		}
	}

	if (UClass* ZoneHudClass = MapDefinition->ZoneHudWidgetClass.LoadSynchronous())
	{
		UUserWidget* Widget = CreateWidget<UUserWidget>(PlayerController, ZoneHudClass);
		if (Widget)
		{
			ZoneHudWidget = Widget;
			Widget->AddToPlayerScreen();
			// 팀 HUD 좌상단 패널과 겹치지 않도록 미니맵 아래에 둔다.
			Widget->SetPositionInViewport(FVector2D(-Margin, Margin + MinimapSize + 8), false);
			Widget->SetAnchorsInViewport(FAnchors(1, 0));
			Widget->SetAlignmentInViewport(FVector2D(1, 0));
		}
	}
}

void UASMapHudSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WidgetTimer);
	}
	if (UUserWidget* Widget = MinimapWidget.Get())
	{
		Widget->RemoveFromParent();
	}
	if (UUserWidget* Widget = ZoneHudWidget.Get())
	{
		Widget->RemoveFromParent();
	}
	MinimapWidget.Reset();
	ZoneHudWidget.Reset();
	Super::Deinitialize();
}
