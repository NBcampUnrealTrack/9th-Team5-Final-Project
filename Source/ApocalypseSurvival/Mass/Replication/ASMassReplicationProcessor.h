#pragma once

#include "MassReplicationProcessor.h"
#include "ASMassReplicationProcessor.generated.h"

/**
 * UE 5.8 MassReplication의 동적 Query가 Processing Queue의 소유 Query 검증에 걸리는 문제를
 * 프로젝트 내부에서 몬스터 전용 소유 Query로 보정한다.
 *
 * MonsterReplicationQuery는 반드시 이 Processor의 고정 멤버로 유지해야 한다. Processing Queue는
 * Processor가 소유한 Query의 주소와 데이터 접근 요구사항을 실행 전에 등록한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassReplicationProcessor : public UMassReplicationProcessor
{
	GENERATED_BODY()

public:
	UASMassReplicationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/** UASMassMonsterReplicator에 고정 결합된 Queue 등록 Query. */
	FMassEntityQuery MonsterReplicationQuery;
};
