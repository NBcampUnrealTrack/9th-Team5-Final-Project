#include "Skills/Settings/ASSkillSettingsSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Skills/Settings/ASSkillSettingsSaveGame.h"

namespace ASSkillSettingsPrivate
{
	const FString SlotName = TEXT("SkillSettings");
	constexpr int32 UserIndex = 0;
}

void UASSkillSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//저장된 값이 있으면 불러옴. 없으면 기본값(걸어가서 시전)
	if (UGameplayStatics::DoesSaveGameExist(ASSkillSettingsPrivate::SlotName, ASSkillSettingsPrivate::UserIndex))
	{
		if (const UASSkillSettingsSaveGame* SaveGame = Cast<UASSkillSettingsSaveGame>(
			UGameplayStatics::LoadGameFromSlot(ASSkillSettingsPrivate::SlotName, ASSkillSettingsPrivate::UserIndex)))
		{
			bWalkToCastWhenOutOfRange = SaveGame->bWalkToCastWhenOutOfRange;
		}
	}
}

void UASSkillSettingsSubsystem::SetWalkToCastWhenOutOfRange(bool bEnabled)
{
	bWalkToCastWhenOutOfRange = bEnabled;
	SaveSettings();

	UE_LOG(LogTemp, Log, TEXT("[SkillSettings] 사거리 밖 시전: %s"), bEnabled ? TEXT("걸어가서 시전") : TEXT("사거리 끝에 시전"));
}

void UASSkillSettingsSubsystem::ToggleWalkToCastWhenOutOfRange()
{
	SetWalkToCastWhenOutOfRange(!bWalkToCastWhenOutOfRange);
}

UASSkillSettingsSubsystem* UASSkillSettingsSubsystem::Get(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UASSkillSettingsSubsystem>() : nullptr;
}

void UASSkillSettingsSubsystem::SaveSettings() const
{
	UASSkillSettingsSaveGame* SaveGame = Cast<UASSkillSettingsSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UASSkillSettingsSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return;
	}

	SaveGame->bWalkToCastWhenOutOfRange = bWalkToCastWhenOutOfRange;
	UGameplayStatics::SaveGameToSlot(SaveGame, ASSkillSettingsPrivate::SlotName, ASSkillSettingsPrivate::UserIndex);
}
