#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Skills/Area/ASSkillArea.h"
#include "ASSkillRedline.generated.h"

class UGameplayEffect;

/*
 * 강태오 궁극기 레드라인
 * 시전자를 따라다니는 장판으로 주기적인 충격파 + 시전자 자기 버프 (지속시간 = 장판 Duration)
 * 무한 스태미나·반동은 SelfEffects의 GE에서 설정 (주기 GE, Additional Effects)
 * 나중: 기본공격 적중 시 충격파, 가까운 적 처치 시 지속시간 연장 (적중·처치 이벤트 필요)
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASSkillRedline : public UASSkillArea
{
	GENERATED_BODY()

public:
	UASSkillRedline();

protected:
	virtual void OnSkillEffect_Implementation() override;

	//시전 시 시전자 자신에게 걸 GE (지속시간은 장판 Duration으로 덮어씀)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Redline")
	TArray<TSubclassOf<UGameplayEffect>> SelfEffects;

	//버프 중 시전자에게 붙일 태그 (SelfEffects에 코드로 추가, GE에서 따로 설정할 필요 없음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Redline")
	FGameplayTag RedlineStateTag;

private:
	//GE 하나를 자신에게 적용 (ASSkillBuff와 같은 방식, 나중에 공통화 예정)
	void ApplySelfEffect(TSubclassOf<UGameplayEffect> EffectClass);
};
