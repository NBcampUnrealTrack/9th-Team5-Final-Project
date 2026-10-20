#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASMapHudWidget.generated.h"

/** 팀 HUD(WBP_GameHUD)에 배치하는 미니맵·구역 HUD 묶음. 구역이 없는 레벨에서는 접힌다. */
UCLASS(Blueprintable, meta = (DisplayName = "AS Map HUD"))
class APOCALYPSESURVIVAL_API UASMapHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftClassPtr<UUserWidget> MinimapWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftClassPtr<UUserWidget> ZoneHudWidgetClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
};
