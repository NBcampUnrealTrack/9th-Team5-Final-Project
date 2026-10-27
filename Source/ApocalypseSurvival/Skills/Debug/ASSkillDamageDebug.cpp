#include "Skills/Debug/ASSkillDamageDebug.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Skills/ASSkillTags.h"
#include "GameplayEffect.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSkillDamageDebug(
		TEXT("AS.Debug.SkillDamage"),
		0,
		TEXT("1 = 스킬 데미지를 화면(머리 위 숫자)과 출력 로그에 표시"),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarSkillDrawDebug(
		TEXT("AS.Debug.SkillDraw"),
		0,
		TEXT("1 = 패키징 빌드에서도 스킬 BP의 Draw Debug 선 표시 (에디터에서는 항상 표시)"),
		ECVF_Cheat);

	//숫자가 떠 있는 시간
	constexpr float LabelDuration = 1.5f;
}

bool ASSkillDamageDebug::IsEnabled()
{
	return CVarSkillDamageDebug.GetValueOnGameThread() > 0;
}

bool ASSkillDamageDebug::IsSkillDrawEnabled()
{
#if ENABLE_DRAW_DEBUG
	return GIsEditor || CVarSkillDrawDebug.GetValueOnGameThread() > 0;
#else
	return false;
#endif
}

float ASSkillDamageDebug::GetDamageFromSpecs(TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs)
{
	//전투 서브시스템이 Mass에 넣을 때와 같은 계산 (Health에 더하는 값이라 음수로 저장됨)
	float Damage = 0.0f;
	for (const FGameplayEffectSpecHandle& Spec : EffectSpecs)
	{
		if (Spec.IsValid())
		{
			Damage += FMath::Max(0.0f, -Spec.Data->GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f));
		}
	}
	return Damage;
}

void ASSkillDamageDebug::Report(const UObject* Source, TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	const FASCombatApplyResult& Result, const FVector& MassLabelLocation)
{
	if (!IsEnabled() || !Source || !Result.HasAnyHit())
	{
		return;
	}

	UWorld* World = Source->GetWorld();
	if (!World)
	{
		return;
	}

	const float Damage = GetDamageFromSpecs(EffectSpecs);
	const FString SourceName = Source->GetClass()->GetName();

	//Actor: 머리 위에 숫자
	for (const AActor* Target : Result.ActorTargets)
	{
		if (!Target)
		{
			continue;
		}

		const FVector LabelLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, Target->GetSimpleCollisionHalfHeight() + 30.0f);
		DrawDebugString(World, LabelLocation, FString::Printf(TEXT("%.0f"), Damage), nullptr, FColor::Red, LabelDuration, true, 1.4f);
	}

	//Mass: 판정 위치에 맞은 수 × 데미지
	if (Result.NonActorHitCount > 0)
	{
		DrawDebugString(World, MassLabelLocation + FVector(0.0f, 0.0f, 120.0f),
			FString::Printf(TEXT("Mass x%d  %.0f"), Result.NonActorHitCount, Damage), nullptr, FColor::Orange, LabelDuration, true, 1.4f);
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillDamage] %s: 1회 데미지 %.1f / Actor %d명 / Mass %d마리 (합계 %.1f)"),
		*SourceName, Damage, Result.ActorTargets.Num(), Result.NonActorHitCount, Damage * Result.GetTotalHitCount());
}
