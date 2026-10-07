#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ASSkillBarViewModel.generated.h"

class UASSkillSlotViewModel;

/** HUD에 고정 배치되는 Q, W, E, R 스킬 슬롯 네 개를 소유한다. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASSkillBarViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** QWER 슬롯 ViewModel을 만들고 각 슬롯의 단축키를 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Bar")
	void InitializeSlots();

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetQSlot() const { return QSlot; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetWSlot() const { return WSlot; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetESlot() const { return ESlot; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetRSlot() const { return RSlot; }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> QSlot = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> WSlot = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> ESlot = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> RSlot = nullptr;
};
