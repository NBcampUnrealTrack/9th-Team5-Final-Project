#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASMonsterPatternAbility.h"
#include "ASMonsterComboStepAbility.generated.h"

/**
 * 범용 콤보의 한 단계를 담당하는 패턴 Ability 기반 클래스.
 * CompletionEventTag가 유효하면 정상 종료 시 Combo GA에 완료 이벤트를 알린다.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterComboStepAbility : public UASMonsterPatternAbility
{
	GENERATED_BODY()

protected:
	/** PatternAbility의 기본 단발 실행을 콤보 완료 이벤트가 있는 종료 경로로 바꾼다. */
	virtual bool ExecutePatternEffect() override;

	/** 정상 완료 시 Avatar ASC에 전송할 이벤트. 비우면 단독 패턴처럼 종료한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Combo", meta = (Categories = "Event"))
	FGameplayTag CompletionEventTag;

	/** 정상 완료/취소를 정리하고, 정상 완료인 경우에만 Combo 완료 이벤트를 보낸다. */
	void FinishComboStep(bool bWasCancelled);
};
