#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASSoundSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class USoundClass;
class UInitialActiveSoundParams;

// Playback is local. Networked events must reach each client before requesting sound.
UCLASS()
class APOCALYPSESURVIVAL_API UASSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UASSoundSubsystem();
	virtual void Deinitialize() override;
	USoundClass* GetMusicSoundClass() const { return MusicSoundClass; }
	USoundClass* GetEffectsSoundClass() const { return EffectsSoundClass; }

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayMusic(USoundBase* Sound, float FadeInDuration = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StopMusic(float FadeOutDuration = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayUISound(USoundBase* Sound, float Volume = 1.0f, USoundConcurrency* Concurrency = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayWorldSound(USoundBase* Sound, FVector Location, float Volume = 1.0f,
		float Pitch = 1.0f, USoundAttenuation* Attenuation = nullptr, USoundConcurrency* Concurrency = nullptr,
		FRotator Rotation = FRotator::ZeroRotator, float StartTime = 0.0f,
		const AActor* OwningActor = nullptr, const UInitialActiveSoundParams* InitialParams = nullptr);

	// Keeps existing Blueprint sound pins while resolving the local game instance's manager.
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3", UnsafeDuringActorConstruction = "true"))
	static void PlayManagedSoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound,
		FVector Location, FRotator Rotation, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f,
		float StartTime = 0.0f, USoundAttenuation* AttenuationSettings = nullptr,
		USoundConcurrency* ConcurrencySettings = nullptr, const AActor* OwningActor = nullptr,
		const UInitialActiveSoundParams* InitialParams = nullptr);

private:
	bool CanPlayAudio() const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	UPROPERTY()
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY()
	TObjectPtr<USoundClass> EffectsSoundClass;
	UPROPERTY()
	TObjectPtr<USoundConcurrency> UIConcurrency;
	UPROPERTY()
	TObjectPtr<USoundConcurrency> WorldConcurrency;
};
