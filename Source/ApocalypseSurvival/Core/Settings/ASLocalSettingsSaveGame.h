#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "ASLocalSettingsSaveGame.generated.h"

UCLASS()
class APOCALYPSESURVIVAL_API UASLocalSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float MasterVolume = 1.0f;

	UPROPERTY()
	TMap<FString, FKey> KeyOverrides;
};
