#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Map/Core/ASMapTypes.h"
#include "Styling/SlateColor.h"
#include "ASZoneHudViewModel.generated.h"

/**
 * 현재 구역 HUD 뷰모델: 구역 이름과 위험도.
 *
 * UASMapViewModelSubsystem이 구역 액터와 공유 MapState로
 * 채우고, 전역 뷰모델 컬렉션에 "ZoneHud"로 등록한다.
 * 위젯을 표시 함수에 바인딩하거나 사용자 정의 표시를 위해 Info에 바인딩한다.
 */
UCLASS(BlueprintType, DisplayName = "AS Zone HUD Viewmodel")
class APOCALYPSESURVIAL_API UASZoneHudViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	static const FName GlobalName;

	/** 구역 이름, 위험도 또는 색상이 바뀔 때만 알린다. */
	void SetInfo(const FASZoneHudInfo& InInfo);

	const FASZoneHudInfo& GetInfo() const { return Info; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "AS|Map")
	FText GetZoneNameText() const;

	UFUNCTION(BlueprintPure, FieldNotify, Category = "AS|Map")
	FText GetDangerText() const;

	UFUNCTION(BlueprintPure, FieldNotify, Category = "AS|Map")
	FSlateColor GetDangerTextColor() const;

	/** 위험도 색상. 구역 밖에서는 투명하다. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "AS|Map")
	FLinearColor GetDangerBackgroundColor() const;

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "AS|Map", meta = (AllowPrivateAccess = "true"))
	FASZoneHudInfo Info;
};
