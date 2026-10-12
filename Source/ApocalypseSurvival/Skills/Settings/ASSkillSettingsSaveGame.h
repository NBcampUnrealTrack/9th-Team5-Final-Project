#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ASSkillSettingsSaveGame.generated.h"

//스킬 조작 설정 저장용 (로컬 플레이어별)
UCLASS()
class APOCALYPSESURVIVAL_API UASSkillSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	//사거리 밖을 지정하면 걸어가서 시전 (false = 사거리 끝에 시전)
	UPROPERTY()
	bool bWalkToCastWhenOutOfRange = true;
};
