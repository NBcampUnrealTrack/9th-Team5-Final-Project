#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Modules/Data/ASModuleTypes.h"
#include "ASModuleSaveGame.generated.h"

/** Persistent equipment instances and the lobby's selected instance IDs. */
UCLASS()
class APOCALYPSESURVIVAL_API UASModuleSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 DataVersion = 2;

	UPROPERTY(SaveGame)
	TArray<FName> UnlockedModuleIds;

	UPROPERTY(SaveGame)
	TArray<FASModuleInstanceData> OwnedModules;

	UPROPERTY(SaveGame)
	TArray<FGuid> EquippedModuleInstanceIds;
};
