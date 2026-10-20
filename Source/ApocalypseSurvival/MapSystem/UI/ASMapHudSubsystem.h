#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMapHudSubsystem.generated.h"

class UUserWidget;

/** 구역이 있는 게임 월드에서 로컬 플레이어의 미니맵과 구역 HUD를 표시한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMapHudSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void TryCreateWidgets();

	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> MinimapWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> ZoneHudWidget;

	FTimerHandle WidgetTimer;
};
