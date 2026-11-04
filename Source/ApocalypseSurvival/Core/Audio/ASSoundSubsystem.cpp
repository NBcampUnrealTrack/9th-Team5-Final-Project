#include "Core/Audio/ASSoundSubsystem.h"

#include "Components/AudioComponent.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/ConstructorHelpers.h"

UASSoundSubsystem::UASSoundSubsystem()
{
	static ConstructorHelpers::FObjectFinder<USoundClass> MusicClass(TEXT("/Game/ApocalypseSurvival/Core/Audio/SC_BGM.SC_BGM"));
	MusicSoundClass = MusicClass.Object;
	static ConstructorHelpers::FObjectFinder<USoundClass> EffectsClass(TEXT("/Game/ApocalypseSurvival/Core/Audio/SC_SFX.SC_SFX"));
	EffectsSoundClass = EffectsClass.Object;
	static ConstructorHelpers::FObjectFinder<USoundConcurrency> UIGroup(TEXT("/Game/ApocalypseSurvival/Core/Audio/SCC_UI.SCC_UI"));
	UIConcurrency = UIGroup.Object;
	static ConstructorHelpers::FObjectFinder<USoundConcurrency> WorldGroup(TEXT("/Game/ApocalypseSurvival/Core/Audio/SCC_WorldSFX.SCC_WorldSFX"));
	WorldConcurrency = WorldGroup.Object;
}

bool UASSoundSubsystem::CanPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

void UASSoundSubsystem::Deinitialize()
{
	StopMusic(0.0f);
	Super::Deinitialize();
}

void UASSoundSubsystem::PlayMusic(USoundBase* Sound, const float FadeInDuration)
{
	if (!Sound || !CanPlayAudio()) return;
	if (IsValid(MusicComponent) && MusicComponent->Sound == Sound && MusicComponent->IsPlaying()
		&& MusicComponent->GetPlayState() != EAudioComponentPlayState::FadingOut) return;
	StopMusic(0.0f);
	// Explicit ownership lets music survive map travel and still be stopped on shutdown.
	MusicComponent = UGameplayStatics::CreateSound2D(this, Sound, 1.0f, 1.0f, 0.0f, nullptr, true, false);
	if (IsValid(MusicComponent))
	{
		MusicComponent->SoundClassOverride = MusicSoundClass;
		MusicComponent->FadeIn(FMath::Max(0.0f, FadeInDuration), 1.0f);
	}
}

void UASSoundSubsystem::StopMusic(const float FadeOutDuration)
{
	if (IsValid(MusicComponent))
	{
		if (FadeOutDuration > 0.0f && MusicComponent->IsPlaying())
		{
			MusicComponent->FadeOut(FadeOutDuration, 0.0f);
			return;
		}
		else
		{
			MusicComponent->Stop();
			MusicComponent->DestroyComponent();
		}
	}
	MusicComponent = nullptr;
}

void UASSoundSubsystem::PlayUISound(USoundBase* Sound, const float Volume, USoundConcurrency* Concurrency)
{
	if (!Sound || !CanPlayAudio()) return;
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(this, Sound, FMath::Max(0.0f, Volume),
		1.0f, 0.0f, Concurrency ? Concurrency : UIConcurrency.Get());
	if (Component)
	{
		Component->SoundClassOverride = EffectsSoundClass;
		Component->Play();
	}
}

void UASSoundSubsystem::PlayWorldSound(USoundBase* Sound, const FVector Location, const float Volume,
	const float Pitch, USoundAttenuation* Attenuation, USoundConcurrency* Concurrency,
	const FRotator Rotation, const float StartTime, const AActor* OwningActor, const UInitialActiveSoundParams* InitialParams)
{
	if (!Sound || !CanPlayAudio()) return;
	FAudioDevice::FCreateComponentParams Params(GetWorld(), const_cast<AActor*>(OwningActor));
	Params.SetLocation(Location);
	Params.AttenuationSettings = Attenuation;
	Params.bPlay = false;
	Params.bAutoDestroy = true;
	Params.bStopWhenOwnerDestroyed = false;
	Params.ConcurrencySet.Add(Concurrency ? Concurrency : WorldConcurrency.Get());
	UAudioComponent* Component = FAudioDevice::CreateComponent(Sound, Params);
	if (Component)
	{
		Component->SoundClassOverride = EffectsSoundClass;
		Component->SetWorldRotation(Rotation);
		Component->SetVolumeMultiplier(FMath::Max(0.0f, Volume));
		Component->SetPitchMultiplier(Pitch);
		if (InitialParams) Component->InstanceParameters = InitialParams->AudioParams;
		Component->Play(StartTime);
	}
}

void UASSoundSubsystem::PlayManagedSoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound,
	const FVector Location, const FRotator Rotation, const float VolumeMultiplier, const float PitchMultiplier,
	const float StartTime, USoundAttenuation* AttenuationSettings, USoundConcurrency* ConcurrencySettings,
	const AActor* OwningActor, const UInitialActiveSoundParams* InitialParams)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UASSoundSubsystem* Audio = GameInstance ? GameInstance->GetSubsystem<UASSoundSubsystem>() : nullptr)
	{
		Audio->PlayWorldSound(Sound, Location, VolumeMultiplier, PitchMultiplier, AttenuationSettings,
			ConcurrencySettings, Rotation, StartTime, OwningActor, InitialParams);
	}
}
