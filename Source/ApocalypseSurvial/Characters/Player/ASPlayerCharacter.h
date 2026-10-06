#pragma once

#include "CoreMinimal.h"
#include "Characters/ASCharacterBase.h"
#include "ASPlayerCharacter.generated.h"

/**
 * 플레이어가 조종하는 캐릭터.
 *
 * 이동은 클릭 방식이다. AASPlayerController 가 명령을 내린 머신에서 길을 찾아 따라가고,
 * 그 결과는 일반 이동 입력처럼 이동 컴포넌트가 서버로 보낸다.
 *
 * 카메라는 이 캐릭터에 없다. 화면이 캐릭터와 따로 움직여야 해서
 * AASPlayerCameraManager 가 시점을 담당한다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIAL_API AASPlayerCharacter : public AASCharacterBase
{
	GENERATED_BODY()

public:
	AASPlayerCharacter();

	/** 서버: 빙의되는 시점에는 PlayerState 가 이미 있다. */
	virtual void PossessedBy(AController* NewController) override;

	/** 클라이언트: PlayerState 는 복제로 도착한다. */
	virtual void OnRep_PlayerState() override;

private:
	void InitAbilitySystemFromPlayerState();
};
