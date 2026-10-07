#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ASHUDViewModel.generated.h"

class UASPlayerStatusViewModel;
class UASSkillBarViewModel;

/** WBP_GameHUD가 바라보는 최상위 HUD MVVM 데이터 모델이다. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** 자식 ViewModel을 만들고 QWER 슬롯을 초기화한다. 여러 번 호출해도 안전하다. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void Initialize();

	UFUNCTION(BlueprintPure, Category = "HUD")
	UASPlayerStatusViewModel* GetPlayerStatusViewModel() const { return PlayerStatusViewModel; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	UASSkillBarViewModel* GetSkillBarViewModel() const { return SkillBarViewModel; }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASPlayerStatusViewModel> PlayerStatusViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillBarViewModel> SkillBarViewModel = nullptr;
};
