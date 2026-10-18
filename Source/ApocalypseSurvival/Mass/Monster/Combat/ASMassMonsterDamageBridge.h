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

	/** ASC를 보유하지 않는 Mass Entity의 서버 피해를 Actor의 ASC에 전달한다. */
	APOCALYPSESURVIVAL_API EApplyResult ApplyToActor(AActor* TargetActor, float Damage, const FVector& SourceLocation);
}
