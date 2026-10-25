#pragma once

#include "CoreMinimal.h"

class AActor;

namespace ASMassMonsterDamage
{
	enum class EApplyResult : uint8
	{
		Applied,
		Blocked,
		InvalidTarget
	};

	/**
	 * 서버 Mass Entity의 피해를 대상 Actor에 전달한다. ASC가 있으면 기존 GameplayEffect를 우선 적용하고,
	 * ASC가 없으면 IASMonsterDamageable 인터페이스로 전달한다.
	 */
	APOCALYPSESURVIVAL_API EApplyResult ApplyToActor(AActor* TargetActor, float Damage,
		const FVector& SourceLocation, const FVector& HitLocation = FVector::ZeroVector, AActor* DamageCauser = nullptr);
}
