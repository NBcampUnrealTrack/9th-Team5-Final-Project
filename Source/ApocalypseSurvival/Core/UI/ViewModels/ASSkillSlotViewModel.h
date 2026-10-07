#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ASSkillSlotViewModel.generated.h"

class UTexture2D;

/** QWER 스킬 바의 한 칸을 표현하는 MVVM 데이터 모델이다. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASSkillSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void InitializeSlot(const FText& InKeyBindText);

	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetSkillIcon(UTexture2D* InSkillIcon);

	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetCooldownPercent(float InCooldownPercent);

	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetStackCount(int32 InStackCount);

	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetEnabled(bool bInIsEnabled);

	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetOnCooldown(bool bInIsOnCooldown);

	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetHasSkill(bool bInHasSkill);

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	UTexture2D* GetSkillIcon() const { return SkillIcon; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	FText GetKeyBindText() const { return KeyBindText; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	float GetCooldownPercent() const { return CooldownPercent; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	int32 GetStackCount() const { return StackCount; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	bool GetIsEnabled() const { return bIsEnabled; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	bool GetIsOnCooldown() const { return bIsOnCooldown; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	bool GetHasSkill() const { return bHasSkill; }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Slot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> SkillIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Slot", meta = (AllowPrivateAccess = "true"))
	FText KeyBindText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Slot", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float CooldownPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Slot", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 StackCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Slot", meta = (AllowPrivateAccess = "true"))
	bool bIsEnabled = true;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Slot", meta = (AllowPrivateAccess = "true"))
	bool bIsOnCooldown = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Slot", meta = (AllowPrivateAccess = "true"))
	bool bHasSkill = false;
};
