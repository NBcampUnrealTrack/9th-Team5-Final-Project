#pragma once

#include "CoreMinimal.h"
#include "MassReplicationTrait.h"
#include "ASMassMonsterReplicationTrait.generated.h"

/**
 * 일반 몬스터 Entity Config에 프로젝트 전용 Client Bubble과 Replicator를 연결한다.
 * MassReplicationTrait의 LOD·갱신 주기 설정은 에셋별로 계속 조정할 수 있다.
 */
UCLASS(meta = (DisplayName = "AS Mass Monster Replication"))
class APOCALYPSESURVIVAL_API UASMassMonsterReplicationTrait : public UMassReplicationTrait
{
	GENERATED_BODY()

public:
	UASMassMonsterReplicationTrait();

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override;
};
