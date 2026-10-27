#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASMapHudWidget.generated.h"

/**
 * 팀 HUD(WBP_GameHUD)에 배치하는 미니맵·구역 HUD 묶음. 구역이 없는 레벨에서는 접힌다.
 * 확장 맵은 화면 중앙의 별도 레이어로 띄운다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "AS Map HUD"))
class APOCALYPSESURVIVAL_API UASMapHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftClassPtr<UUserWidget> MinimapWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftClassPtr<UUserWidget> ZoneHudWidgetClass;

	/** 화면 중앙 레이어에 띄우는 확장 맵. 팀 HUD 인스턴스에서 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftClassPtr<UUserWidget> ExpandedMapWidgetClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ExpandedMapWidget;
};
