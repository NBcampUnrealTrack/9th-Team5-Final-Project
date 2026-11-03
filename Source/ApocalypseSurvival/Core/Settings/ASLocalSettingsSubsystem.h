#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASLocalSettingsSubsystem.generated.h"

class UInputMappingContext;
class UASLocalSettingsSaveGame;
class USoundMix;

UCLASS()
class APOCALYPSESURVIVAL_API UASLocalSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	UInputMappingContext* GetRuntimeMappingContext(UInputMappingContext* Source);
	FKey GetMappedKey(const UInputMappingContext* Source, int32 MappingIndex) const;
	bool SetMappedKey(UInputMappingContext* Source, int32 MappingIndex, FKey Key, FText& Error);
	float GetMasterVolume() const;
	void SetMasterVolume(float Volume);
	float GetBGMVolume() const;
	void SetBGMVolume(float Volume);
	float GetSFXVolume() const;
	void SetSFXVolume(float Volume);
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool IsRightClickAttackEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetRightClickAttackEnabled(bool bEnabled);
	void ResetMappedKeys(UInputMappingContext* Source);

private:
	FString GetMappingId(const UInputMappingContext* Source, int32 MappingIndex) const;
	void RebuildMappingContext(UInputMappingContext* Source, UInputMappingContext* Runtime);
	void ApplyMasterVolume();
	void HandleMapLoaded(UWorld* World);
	void SaveSettings();
	FString GetSaveSlot() const;

	UPROPERTY(Transient)
	TObjectPtr<UASLocalSettingsSaveGame> Settings;
	UPROPERTY(Transient)
	TMap<TObjectPtr<UInputMappingContext>, TObjectPtr<UInputMappingContext>> RuntimeContexts;
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> VolumeMix;
	TWeakObjectPtr<UWorld> AudioWorld;
};
