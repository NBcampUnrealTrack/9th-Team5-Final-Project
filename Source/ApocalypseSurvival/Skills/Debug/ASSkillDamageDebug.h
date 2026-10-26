#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"

//스킬 데미지 디버그 (콘솔: AS.Debug.SkillDamage 1 켜기 / 0 끄기)
//GE_Damage Spec 안의 Data.Damage 값을 꺼내서, 맞은 Actor 머리 위와 Mass 판정 위치에 숫자를 띄우고 출력 로그에 남김
//Actor는 표시값에서 대상의 피해 감소(DamageReduction)가 추가로 적용됨. Mass는 표시값 그대로 들어감
namespace ASSkillDamageDebug
{
	APOCALYPSESURVIVAL_API bool IsEnabled();

	//Spec 목록에 들어 있는 1회 데미지 합계 (Data.Damage)
	APOCALYPSESURVIVAL_API float GetDamageFromSpecs(TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs);

	//ResolveAndApply 결과 표시 (서버에서 호출). MassLabelLocation = Mass 맞은 수를 띄울 위치
	APOCALYPSESURVIVAL_API void Report(const UObject* Source, TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
		const FASCombatApplyResult& Result, const FVector& MassLabelLocation);
}
