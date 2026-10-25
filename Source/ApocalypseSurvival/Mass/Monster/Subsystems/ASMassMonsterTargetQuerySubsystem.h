#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMassMonsterTargetQuerySubsystem.generated.h"

class UMassEntitySubsystem;

/**
 * 일반 공격과 공격 이동이 사용할 Mass 몬스터의 읽기 전용 타겟 조회 계층.
 *
 * 서버에서는 권한 있는 UASMassMonsterSubsystem의 관리 Entity를 조회하고,
 * 클라이언트에서는 Mass Client Bubble이 만든 복제 Entity를 조회한다.
 * 반환하는 TargetId는 현재 World 안에서만 유효한 FMassEntityHandle 번호이며,
 * RPC 또는 복제 데이터로 전송하면 안 된다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterTargetQuerySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * SearchOrigin 반경 안의 살아 있는 몬스터 중 PreferredLocation에 가장 가까운 대상을 반환한다.
	 * 클라이언트에서는 Client Bubble이 복제한 Entity만 조회한다.
	 */
	bool FindBestAliveMonsterTarget(const FVector& SearchOrigin, float SearchRadius,
		const FVector& PreferredLocation, bool bRequireVisibleToLocalTeam,
		uint64& OutTargetId, FVector& OutLocation, float& OutHitRadius) const;

	/** 현재 World에서만 유효한 TargetId를 최신 위치와 생존 상태로 갱신한다. */
	bool GetAliveMonsterTarget(uint64 TargetId, bool bRequireVisibleToLocalTeam,
		FVector& OutLocation, float& OutHitRadius) const;

	/** 현재 World의 로컬 Entity ID에서 서버·클라이언트 공통 Mass replication ID를 얻는다. */
	bool GetMassNetworkId(uint64 TargetId, uint32& OutMassNetworkId) const;

	/** 서버 전용. Mass replication ID를 살아 있는 권한 Entity로 해석한다. */
	bool GetAliveMonsterTargetByNetworkId(uint32 MassNetworkId, uint64& OutTargetId,
		FVector& OutLocation, float& OutHitRadius) const;

private:
	bool FindBestClientReplicaTarget(const FVector& SearchOrigin, float SearchRadius,
		const FVector& PreferredLocation, bool bRequireVisibleToLocalTeam,
		uint64& OutTargetId, FVector& OutLocation, float& OutHitRadius) const;

	bool GetAliveClientReplicaTarget(uint64 TargetId, bool bRequireVisibleToLocalTeam,
		FVector& OutLocation, float& OutHitRadius) const;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
};
