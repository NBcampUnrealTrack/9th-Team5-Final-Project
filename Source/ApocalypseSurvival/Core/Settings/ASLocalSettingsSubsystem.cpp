#include "Core/Settings/ASLocalSettingsSubsystem.h"

#include "Core/Settings/ASLocalSettingsSaveGame.h"
#include "Core/Audio/ASSoundSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void UASLocalSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGameplayStatics::DoesSaveGameExist(GetSaveSlot(), 0))
	{
		Settings = Cast<UASLocalSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlot(), 0));
	}
	if (!Settings) Settings = NewObject<UASLocalSettingsSaveGame>(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandleMapLoaded);
}

void UASLocalSettingsSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::Deinitialize();
}

FString UASLocalSettingsSubsystem::GetSaveSlot() const
{
	return FString::Printf(TEXT("LocalSettings_%d"), FMath::Max(0, GetLocalPlayer()->GetControllerId()));
}

void UASLocalSettingsSubsystem::SaveSettings()
{
	if (!UGameplayStatics::SaveGameToSlot(Settings, GetSaveSlot(), 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Settings] Failed to save local settings."));
	}
}

void UASLocalSettingsSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	ApplyMasterVolume();
}

void UASLocalSettingsSubsystem::HandleMapLoaded(UWorld* World)
{
	if (World && World == GetLocalPlayer()->GetWorld()) ApplyMasterVolume();
}

float UASLocalSettingsSubsystem::GetMasterVolume() const
{
	return Settings ? Settings->MasterVolume : 1.0f;
}

void UASLocalSettingsSubsystem::SetMasterVolume(const float Volume)
{
	Settings->MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyMasterVolume();
	SaveSettings();
}

float UASLocalSettingsSubsystem::GetBGMVolume() const
{
	return Settings ? Settings->BGMVolume : 1.0f;
}

void UASLocalSettingsSubsystem::SetBGMVolume(const float Volume)
{
	Settings->BGMVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyMasterVolume();
	SaveSettings();
}

float UASLocalSettingsSubsystem::GetSFXVolume() const
{
	return Settings ? Settings->SFXVolume : 1.0f;
}

void UASLocalSettingsSubsystem::SetSFXVolume(const float Volume)
{
	Settings->SFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyMasterVolume();
	SaveSettings();
}

bool UASLocalSettingsSubsystem::IsRightClickAttackEnabled() const
{
	return Settings ? Settings->bRightClickAttackEnabled : true;
}

void UASLocalSettingsSubsystem::SetRightClickAttackEnabled(const bool bEnabled)
{
	if (!Settings) return;
	Settings->bRightClickAttackEnabled = bEnabled;
	SaveSettings();
}

void UASLocalSettingsSubsystem::ApplyMasterVolume()
{
	UWorld* World = GetLocalPlayer()->GetWorld();
	if (!World || !Settings) return;
	USoundClass* MasterClass = Cast<USoundClass>(GetDefault<UAudioSettings>()->DefaultSoundClassName.TryLoad());
	if (!MasterClass) return;
	if (!VolumeMix)
	{
		VolumeMix = NewObject<USoundMix>(this);
		VolumeMix->Duration = -1.0f;
		VolumeMix->FadeInTime = 0.0f;
	}
	UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, MasterClass, Settings->MasterVolume, 1.0f, 0.0f, true);
	if (UASSoundSubsystem* Audio = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UASSoundSubsystem>())
	{
		if (USoundClass* MusicClass = Audio->GetMusicSoundClass())
		{
			// BGM has its own root class so engine assets remain untouched. Include master volume explicitly.
			UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, MusicClass,
				Settings->MasterVolume * Settings->BGMVolume, 1.0f, 0.0f, true);
		}
		if (USoundClass* EffectsClass = Audio->GetEffectsSoundClass())
		{
			UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, EffectsClass,
				Settings->MasterVolume * Settings->SFXVolume, 1.0f, 0.0f, true);
		}
	}
	if (AudioWorld != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, VolumeMix);
		AudioWorld = World;
	}
}

FString UASLocalSettingsSubsystem::GetMappingId(const UInputMappingContext* Source, const int32 MappingIndex) const
{
	if (!Source || !Source->GetMappings().IsValidIndex(MappingIndex)) return FString();
	const FEnhancedActionKeyMapping& Mapping = Source->GetMappings()[MappingIndex];
	return Source->GetPathName() + TEXT("|") + GetPathNameSafe(Mapping.Action) + TEXT("|") + Mapping.Key.GetFName().ToString();
}

FKey UASLocalSettingsSubsystem::GetMappedKey(const UInputMappingContext* Source, const int32 MappingIndex) const
{
	if (!Source || !Source->GetMappings().IsValidIndex(MappingIndex)) return FKey();
	const FKey* Override = Settings->KeyOverrides.Find(GetMappingId(Source, MappingIndex));
	return Override ? *Override : Source->GetMappings()[MappingIndex].Key;
}

void UASLocalSettingsSubsystem::RebuildMappingContext(UInputMappingContext* Source, UInputMappingContext* Runtime)
{
	Runtime->UnmapAll();
	for (int32 Index = 0; Index < Source->GetMappings().Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Original = Source->GetMappings()[Index];
		FEnhancedActionKeyMapping& Mapping = Runtime->MapKey(Original.Action, GetMappedKey(Source, Index));
		Mapping = Original;
		Mapping.Key = GetMappedKey(Source, Index);
	}
}

UInputMappingContext* UASLocalSettingsSubsystem::GetRuntimeMappingContext(UInputMappingContext* Source)
{
	if (!Source) return nullptr;
	if (const TObjectPtr<UInputMappingContext>* Existing = RuntimeContexts.Find(Source)) return Existing->Get();
	UInputMappingContext* Runtime = DuplicateObject<UInputMappingContext>(Source, this);
	RuntimeContexts.Add(Source, Runtime);
	RebuildMappingContext(Source, Runtime);
	return Runtime;
}

bool UASLocalSettingsSubsystem::SetMappedKey(UInputMappingContext* Source, const int32 MappingIndex, const FKey Key, FText& Error)
{
	if (!Source || !Source->GetMappings().IsValidIndex(MappingIndex) || !Key.IsValid() || Key.IsGamepadKey())
	{
		Error = FText::FromString(TEXT("유효한 키보드 또는 마우스 입력을 선택해 주세요."));
		return false;
	}
	Settings->KeyOverrides.Add(GetMappingId(Source, MappingIndex), Key);
	if (const TObjectPtr<UInputMappingContext>* Runtime = RuntimeContexts.Find(Source)) RebuildMappingContext(Source, Runtime->Get());
	if (UEnhancedInputLocalPlayerSubsystem* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		Input->RequestRebuildControlMappings();
	}
	SaveSettings();
	OnKeyMappingsChanged.Broadcast();
	return true;
}

void UASLocalSettingsSubsystem::ResetMappedKeys(UInputMappingContext* Source)
{
	if (!Source) return;
	for (int32 Index = 0; Index < Source->GetMappings().Num(); ++Index) Settings->KeyOverrides.Remove(GetMappingId(Source, Index));
	if (const TObjectPtr<UInputMappingContext>* Runtime = RuntimeContexts.Find(Source)) RebuildMappingContext(Source, Runtime->Get());
	if (UEnhancedInputLocalPlayerSubsystem* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) Input->RequestRebuildControlMappings();
	SaveSettings();
	OnKeyMappingsChanged.Broadcast();
}
