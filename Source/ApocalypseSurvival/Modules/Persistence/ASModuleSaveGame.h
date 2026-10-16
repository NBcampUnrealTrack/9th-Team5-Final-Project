#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ASModuleSaveGame.generated.h"

/** Fixed module unlocks; no live UObject references or GAS handles are serialized. */
UCLASS()
class APOCALYPSESURVIVAL_API UASModuleSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 DataVersion = 1;

	UPROPERTY(SaveGame)
	TArray<FName> OwnedModuleIds;

	/** Index is the lobby slot. NAME_None means an empty slot. */
	UPROPERTY(SaveGame)
	TArray<FName> EquippedModuleIds;
};
