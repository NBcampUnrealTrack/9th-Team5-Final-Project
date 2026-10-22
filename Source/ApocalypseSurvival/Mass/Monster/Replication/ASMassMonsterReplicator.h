#pragma once

#include "CoreMinimal.h"
#include "MassReplicationProcessor.h"
#include "ASMassMonsterReplicator.generated.h"

/** 서버 Mass 몬스터의 최소 표현 상태를 클라이언트별 Bubble Fast Array에 기록한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterReplicator : public UMassReplicatorBase
{
	GENERATED_BODY()

public:
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;
	virtual void ProcessClientReplication(FMassExecutionContext& Context,
		FMassReplicationContext& ReplicationContext) override;
};
