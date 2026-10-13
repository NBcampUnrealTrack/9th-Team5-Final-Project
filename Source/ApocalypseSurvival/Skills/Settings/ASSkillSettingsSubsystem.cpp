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

			//시전 방식은 게임을 켤 때마다 일반 시전으로 시작 (옵션 UI가 생기면 저장값 불러오기로 변경)
			//if (SaveGame->CastMode <= static_cast<uint8>(EASSkillCastMode::NormalCast))
			//{
			//	CastMode = static_cast<EASSkillCastMode>(SaveGame->CastMode);
			//}
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

void UASSkillSettingsSubsystem::SetCastMode(EASSkillCastMode NewCastMode)
{
	CastMode = NewCastMode;
	SaveSettings();

	static const TCHAR* ModeNames[] = { TEXT("빠른 시전"), TEXT("범위 표시 후 빠른 시전"), TEXT("일반 시전") };
	UE_LOG(LogTemp, Log, TEXT("[SkillSettings] 시전 방식: %s"), ModeNames[static_cast<uint8>(CastMode)]);
}

void UASSkillSettingsSubsystem::CycleCastMode()
{
	const uint8 Next = (static_cast<uint8>(CastMode) + 1) % (static_cast<uint8>(EASSkillCastMode::NormalCast) + 1);
	SetCastMode(static_cast<EASSkillCastMode>(Next));
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
	SaveGame->CastMode = static_cast<uint8>(CastMode);
	UGameplayStatics::SaveGameToSlot(SaveGame, ASSkillSettingsPrivate::SlotName, ASSkillSettingsPrivate::UserIndex);
}
