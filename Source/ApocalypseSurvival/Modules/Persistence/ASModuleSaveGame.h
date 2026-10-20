#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Modules/Data/ASModuleTypes.h"
#include "Quests/Persistence/ASQuestSaveData.h"
#include "ASModuleSaveGame.generated.h"

/** Personal profile. Version 3 adds quests; version 2 module profiles remain readable. */
UCLASS()
class APOCALYPSESURVIVAL_API UASModuleSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 3;
	static constexpr int32 LegacyModuleVersion = 2;

	UPROPERTY(SaveGame)
	int32 DataVersion = CurrentVersion;

	UPROPERTY(SaveGame)
	TArray<FName> UnlockedModuleIds;

	UPROPERTY(SaveGame)
	TArray<FASModuleInstanceData> OwnedModules;

	UPROPERTY(SaveGame)
	TArray<FGuid> EquippedModuleInstanceIds;

	UPROPERTY(SaveGame)
	FASQuestSaveData QuestProgress;
};
