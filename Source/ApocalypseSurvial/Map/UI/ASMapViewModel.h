#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"
#include "ASMapViewModel.generated.h"

/**
 * 전역 뷰모델 컬렉션에 "Map"으로 등록되는 로컬 맵 UI 상태.
 * 확장 맵의 Visibility를 GetExpandedMapVisibility에 바인딩한다.
 */
UCLASS(BlueprintType, DisplayName = "AS Map Viewmodel")
class APOCALYPSESURVIAL_API UASMapViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	static const FName GlobalName;

	bool IsExpandedMapOpen() const { return bExpandedMapOpen; }
	void SetExpandedMapOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "AS|Map")
	void ToggleExpandedMap() { SetExpandedMapOpen(!bExpandedMapOpen); }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "AS|Map")
	ESlateVisibility GetExpandedMapVisibility() const;

private:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = "IsExpandedMapOpen", Setter = "SetExpandedMapOpen", Category = "AS|Map", meta = (AllowPrivateAccess = "true"))
	bool bExpandedMapOpen = false;
};
