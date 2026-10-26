#pragma once

#include "CoreMinimal.h"
#include "Skills/Area/ASSkillArea.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "ASSkillCatchSling.generated.h"

/*
 * 재이 W (1차): 염동 장판 (2중 시전)
 * - 조준 중 커서 근처에 Actor 적(엘리트·보스)이 있으면 효과 원이 그 적에게 달라붙음 (bSnapToActorTarget)
 * - Actor 적에게 시전: 장판이 그 적을 따라다님, 적이 죽으면 그 자리에 남음
 * - 그 외(빈 땅, 일반 Mass 몬스터): 커서 위치에 고정 장판
 * - TickInterval마다 장판 안에 피해 + AdditionalEffects(슬로우 등, Actor만 적용)
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASSkillCatchSling : public UASSkillArea
{
	GENERATED_BODY()

public:
	UASSkillCatchSling();

protected:
	virtual void OnSkillEffect_Implementation() override;

	//서버에서 목표 지점 근처의 Actor 적을 다시 찾을 거리 (클라이언트에서 달라붙은 적이 조금 움직여도 찾도록)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|CatchSling", meta = (ClampMin = "0.0", Units = "cm"))
	float ServerSearchRadius = 100.0f;

	//true = 일반 Mass 몬스터에게도 달라붙어 따라다님 (조준 표시는 Actor에게만 붙음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|CatchSling")
	bool bFollowMassTargets = false;

private:
	//Location 근처에서 따라갈 적 하나
	bool FindFollowTarget(const FVector& Location, FASCombatTargetRef& OutTarget) const;
};
