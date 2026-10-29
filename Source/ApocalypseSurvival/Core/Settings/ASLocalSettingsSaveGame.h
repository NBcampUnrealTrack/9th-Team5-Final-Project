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
	
	/** 몬스터 우클릭 시 공격 여부. false면 우클릭은 항상 이동만 한다. */
	UPROPERTY()
	bool bRightClickAttackEnabled = true;
};
