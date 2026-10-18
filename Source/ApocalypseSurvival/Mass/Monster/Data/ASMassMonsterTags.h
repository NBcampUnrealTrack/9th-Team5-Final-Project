#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ASMassMonsterTags.generated.h"

/** 죽은 Entity를 감지·이동·공격 Query에서 제외한다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterDeadTag : public FMassTag
{
	GENERATED_BODY()
};

/** 사망 연출이나 보상 처리가 끝난 뒤 제거할 Entity를 표시한다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterPendingDestroyTag : public FMassTag
{
	GENERATED_BODY()
};

/** NavMesh 경로를 새로 요청해야 하는 Entity를 표시한다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterNeedsPathTag : public FMassTag
{
	GENERATED_BODY()
};
