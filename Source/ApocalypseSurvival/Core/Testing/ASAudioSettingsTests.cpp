#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Audio/ASSoundSubsystem.h"
#include "Core/Settings/ASLocalSettingsSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASBGMSettingsTest,
	"ApocalypseSurvival.Audio.BGMSettingsPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASBGMSettingsTest::RunTest(const FString& Parameters)
{
	UASLocalSettingsSaveGame* Settings = NewObject<UASLocalSettingsSaveGame>();
	TestEqual(TEXT("New saves default BGM volume to full"), Settings->BGMVolume, 1.0f);
	TestEqual(TEXT("New saves default SFX volume to full"), Settings->SFXVolume, 1.0f);
	Settings->MasterVolume = 0.5f;
	Settings->BGMVolume = 0.3f;
	Settings->SFXVolume = 0.4f;
	TArray<uint8> Data;
	if (!TestTrue(TEXT("Serialize audio settings"), UGameplayStatics::SaveGameToMemory(Settings, Data))) return false;
	const UASLocalSettingsSaveGame* Loaded = Cast<UASLocalSettingsSaveGame>(UGameplayStatics::LoadGameFromMemory(Data));
	if (!TestNotNull(TEXT("Restore saved settings"), Loaded)) return false;
	TestEqual(TEXT("BGM volume is restored independently"), Loaded->BGMVolume, 0.3f);
	TestEqual(TEXT("SFX volume is restored independently"), Loaded->SFXVolume, 0.4f);
	TestEqual(TEXT("Master volume is preserved"), Loaded->MasterVolume, 0.5f);
	USoundClass* MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/ApocalypseSurvival/Core/Audio/SC_BGM.SC_BGM"));
	if (TestNotNull(TEXT("BGM sound class is available"), MusicClass))
	{
		TestNull(TEXT("BGM root uses explicit combined master/BGM volume"), MusicClass->ParentClass.Get());
		TestEqual(TEXT("Manager references the packaged BGM class"),
			GetDefault<UASSoundSubsystem>()->GetMusicSoundClass(), MusicClass);
	}
	USoundClass* EffectsClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/ApocalypseSurvival/Core/Audio/SC_SFX.SC_SFX"));
	if (TestNotNull(TEXT("SFX sound class is available"), EffectsClass))
	{
		TestNull(TEXT("SFX root uses explicit combined master/SFX volume"), EffectsClass->ParentClass.Get());
		TestEqual(TEXT("Manager references the packaged SFX class"),
			GetDefault<UASSoundSubsystem>()->GetEffectsSoundClass(), EffectsClass);
	}
	for (const auto& Entry : { TPair<const TCHAR*, int32>(TEXT("SCC_UI"), 4), TPair<const TCHAR*, int32>(TEXT("SCC_WorldSFX"), 16) })
	{
		const FString Path = FString::Printf(TEXT("/Game/ApocalypseSurvival/Core/Audio/%s.%s"), Entry.Key, Entry.Key);
		USoundConcurrency* Rule = LoadObject<USoundConcurrency>(nullptr, *Path);
		if (!TestNotNull(TEXT("Concurrency profile loads"), Rule)) continue;
		TestEqual(TEXT("Shared group voice limit"), static_cast<int32>(Rule->Concurrency.MaxCount), Entry.Value);
		TestFalse(TEXT("Limit applies across actors"), Rule->Concurrency.bLimitToOwner);
		TestEqual(TEXT("Oldest sound is replaced"), Rule->Concurrency.ResolutionRule, EMaxConcurrentResolutionRule::StopOldest);
		TestEqual(TEXT("Voice stealing fades briefly"), Rule->Concurrency.VoiceStealReleaseTime, 0.05f);
	}
	return true;
}

#endif
