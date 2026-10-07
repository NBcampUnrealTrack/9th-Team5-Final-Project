#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ASPlayerStatusViewModel.generated.h"

class UTexture2D;

/** 플레이어 초상화, 레벨, 체력 및 마나 표시용 MVVM 데이터 모델이다. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASPlayerStatusViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD|Player Status")
	void SetHealth(float InCurrentHealth, float InMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "HUD|Player Status")
	void SetMana(float InCurrentMana, float InMaxMana);

	UFUNCTION(BlueprintCallable, Category = "HUD|Player Status")
	void SetPlayerLevel(int32 InPlayerLevel);

	UFUNCTION(BlueprintCallable, Category = "HUD|Player Status")
	void SetPortraitTexture(UTexture2D* InPortraitTexture);

	UFUNCTION(BlueprintPure, Category = "HUD|Player Status")
	float GetHealthPercent() const { return HealthPercent; }

	UFUNCTION(BlueprintPure, Category = "HUD|Player Status")
	FText GetHealthText() const { return HealthText; }

	UFUNCTION(BlueprintPure, Category = "HUD|Player Status")
	float GetManaPercent() const { return ManaPercent; }

	UFUNCTION(BlueprintPure, Category = "HUD|Player Status")
	FText GetManaText() const { return ManaText; }

	UFUNCTION(BlueprintPure, Category = "HUD|Player Status")
	int32 GetPlayerLevel() const { return PlayerLevel; }

	UFUNCTION(BlueprintPure, Category = "HUD|Player Status")
	UTexture2D* GetPortraitTexture() const { return PortraitTexture; }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Player Status", meta = (AllowPrivateAccess = "true"))
	float HealthPercent = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Player Status", meta = (AllowPrivateAccess = "true"))
	FText HealthText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Player Status", meta = (AllowPrivateAccess = "true"))
	float ManaPercent = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Player Status", meta = (AllowPrivateAccess = "true"))
	FText ManaText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Player Status", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 PlayerLevel = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Player Status", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> PortraitTexture = nullptr;
};
