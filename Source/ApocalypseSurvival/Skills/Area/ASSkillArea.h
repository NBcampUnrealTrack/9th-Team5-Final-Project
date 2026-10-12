#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "ASSkillArea.generated.h"

class AASSkillAreaActor;

/**
 * 일정 시간 유지되는 범위(AASSkillAreaActor)를 만드는 스킬
 * 데미지 1회 = CalculateDamage(), 횟수 = Duration / TickInterval
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASSkillArea : public UASSkillAbility
{
	GENERATED_BODY()

public:
	UASSkillArea();

protected:
	virtual void OnSkillEffect_Implementation() override;
	virtual void ApplySkillStats(const FASSkillStatsRow& Stats) override;
	virtual FASSkillIndicatorSettings GetIndicatorSettings_Implementation() const override;

	//생성할 범위 액터 (이펙트를 넣은 BP 자식 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area")
	TSubclassOf<AASSkillAreaActor> AreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area", meta = (ClampMin = "0.05", Units = "s"))
	float Duration = 3.0f;

	//데미지 간격 (0.5 = 초당 2번)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area", meta = (ClampMin = "0.05", Units = "s"))
	float TickInterval = 0.5f;

	//생성하자마자 첫 데미지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area")
	bool bDamageOnSpawn = true;

	//true = 시전자를 따라다님, false = 그 자리에 남음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area")
	bool bAttachToOwner = true;

	//그 자리에 남을 때 생성 위치를 앞으로 옮기는 거리 (조준 방식이 방향형일 때만 사용, 위치형은 목표 지점에 생성)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area", meta = (Units = "cm", EditCondition = "!bAttachToOwner"))
	float ForwardOffset = 0.0f;
};
