#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "ASSkillBuff.generated.h"

class UGameplayEffect;

UCLASS()
class APOCALYPSESURVIVAL_API UASSkillBuff : public UASSkillAbility
{
	GENERATED_BODY()
	
public:
	UASSkillBuff();
	
	virtual void OnSkillEffect_Implementation() override;
	
protected:
	virtual void ApplySkillStats(const FASSkillStatsRow& Stats) override;
	
	//적용할 버프 스킬 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Buff")
	TArray<TSubclassOf<UGameplayEffect>> BuffEffects;
	
	//지속시간 (0이면 GE에 적힌 값으로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Buff")
	float BuffDuration = 0.0f;
	
private:
	void ApplyBuffEffect(TSubclassOf<UGameplayEffect> EffectClass);
};
