#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ASCheatManager.generated.h"

/** 테스트용 콘솔 명령의 진입점. 서버 실행 요청은 소유 PlayerController가 전달한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** 자신이 조종하는 플레이어의 피해 무적을 켜거나 끈다. */
	UFUNCTION(Exec)
	void DebugGod();
};
