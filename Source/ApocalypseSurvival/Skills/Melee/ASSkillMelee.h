#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "ASSkillMelee.generated.h"

/**
 * 타격 타이밍마다 원 / 부채꼴 범위를 한 번 판정하는 스킬
 * Angle 360 = 원, 그보다 작으면 부채꼴
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASSkillMelee : public UASSkillAbility
{
	GENERATED_BODY()

public:
	UASSkillMelee();

protected:
	virtual void OnSkillEffect_Implementation() override;
	virtual void ApplySkillStats(const FASSkillStatsRow& Stats) override;
	virtual FASSkillIndicatorSettings GetIndicatorSettings_Implementation() const override;

	// Actor 전용 레거시 훅. 효과 적용은 UASCombatTargetSubsystem이 담당한다.
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill|Melee", meta = (DeprecatedFunction, DeprecationMessage = "Use the unified combat target path. This event is no longer called by UASSkillMelee."))
	void OnTargetsHit(const TArray<AActor*>& Targets);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Melee", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 200.0f;

	//판정 중심을 앞으로 옮기는 거리 (0 = 나 기준)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Melee", meta = (Units = "cm"))
	float ForwardOffset = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Melee", meta = (ClampMin = "1.0", ClampMax = "360.0", Units = "deg"))
	float Angle = 360.0f;
};
