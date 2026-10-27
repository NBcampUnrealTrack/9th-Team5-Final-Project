#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ASCheatManager.generated.h"

class AASPlayerController;

/** 테스트용 콘솔 명령의 진입점. 서버 실행 요청은 소유 PlayerController가 전달한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** 자신이 조종하는 플레이어의 피해 무적을 켜거나 끈다. */
	UFUNCTION(Exec)
	void DebugGod();

	/** 지정한 무기 1개를 가방에 지급한다. 이름을 생략하면 지원하는 무기 목록을 표시한다. */
	UFUNCTION(Exec)
	void DebugGiveWeapon(FName WeaponName = NAME_None);

	/** PlayerController의 RPC가 호출하는 서버 전용 지급 처리. */
	static void GiveWeaponToPlayer(AASPlayerController* PlayerController, FName WeaponName);
};
