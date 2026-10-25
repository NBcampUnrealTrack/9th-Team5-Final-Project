#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASCombatTargetSubsystem.generated.h"

class AActor;

/** 타겟 검색에 포함할 전투 백엔드 범위. */
enum class EASCombatTargetDomain : uint8
{
	ActorOnly,
	MassOnly,
	All
};

/** 여러 후보 중 최종 타겟을 고르는 거리 기준. */
enum class EASCombatTargetSortPolicy : uint8
{
	/** SearchOrigin(일반적으로 공격자)에서 가장 가까운 대상을 고른다. */
	NearestToSearchOrigin,
	/** PriorityLocation(예: 사거리 안 클릭 지점)에서 가장 가까운 대상을 고른다. */
	NearestToPriorityLocation
};

/** 범위 공격의 공간 판정 형태. */
enum class EASCombatTargetShape : uint8
{
	Radius,
	Cone,
	Segment
};

/** 타겟 검색 규칙. 호출부는 백엔드가 아닌 검색 의도만 지정한다. */
struct APOCALYPSESURVIVAL_API FASCombatTargetQuery
{
	/** Radius/Cone의 중심, Segment의 시작 위치. */
	FVector SearchOrigin = FVector::ZeroVector;
	/** Cone의 방향. */
	FVector Direction = FVector::ForwardVector;
	/** Segment의 끝 위치. */
	FVector EndLocation = FVector::ZeroVector;
	FVector PriorityLocation = FVector::ZeroVector;
	/** Radius/Cone의 반경, Segment의 Sweep 반경. */
	float SearchRadius = 0.0f;
	/** Cone의 전체 각도. 360이면 원형과 같다. */
	float AngleDegrees = 360.0f;
	const AActor* Instigator = nullptr;
	/** 지정 공격에서만 사용하는 Actor 고정 대상. 설정 시 Mass는 검색하지 않는다. */
	const AActor* ExplicitActor = nullptr;
	EASCombatTargetDomain Domain = EASCombatTargetDomain::All;
	EASCombatTargetSortPolicy SortPolicy = EASCombatTargetSortPolicy::NearestToSearchOrigin;
	EASCombatTargetShape Shape = EASCombatTargetShape::Radius;
	/** 0이면 제한 없음. */
	int32 MaxTargets = 0;
	bool bCanHitPlayers = false;
	/** Include the instigator, players, friendly/neutral actors and Mass monsters together. Overrides affiliation filters only. */
	bool bHitAllTeams = false;
	bool bRequireAlive = true;
	bool bRequireVisibleToLocalTeam = false;
};

/** Actor와 Mass Entity를 호출부에 같은 형태로 전달하는 단일 타겟 참조. */
struct APOCALYPSESURVIVAL_API FASCombatTargetRef
{
	bool IsValid() const;
	bool IsActor() const;
	bool IsMass() const;
	AActor* GetActor() const;
	uint64 GetMassTargetId() const { return MassTargetId; }
	uint32 GetMassNetworkId() const { return MassNetworkId; }
	const FVector& GetLocation() const { return Location; }
	float GetHitRadius() const { return HitRadius; }
	void Reset();

private:
	TWeakObjectPtr<AActor> Actor;
	uint64 MassTargetId = 0;
	/** 서버와 Client Bubble 양쪽에서 동일한 Mass replication ID. */
	uint32 MassNetworkId = 0;
	FVector Location = FVector::ZeroVector;
	float HitRadius = 0.0f;

	friend class UASCombatTargetSubsystem;
};

/** 투사체처럼 여러 프레임에 걸쳐 같은 대상을 한 번만 맞혀야 할 때 사용하는 Backend 비종속 기록. */
struct APOCALYPSESURVIVAL_API FASCombatTargetHitSet
{
	bool ContainsActor(const AActor* Actor) const;
	bool AddActor(AActor* Actor);
	int32 Num() const;
	void Reset();

private:
	TSet<TWeakObjectPtr<AActor>> Actors;
	TSet<int32> NonActorTargetIds;

	friend class UASCombatTargetSubsystem;
};

/** 공통 범위 공격 결과. Blueprint 호환을 위해 Actor 목록은 그대로 보존한다. */
struct APOCALYPSESURVIVAL_API FASCombatApplyResult
{
	TArray<AActor*> ActorTargets;
	int32 NonActorHitCount = 0;

	int32 GetTotalHitCount() const { return ActorTargets.Num() + NonActorHitCount; }
	bool HasAnyHit() const { return GetTotalHitCount() > 0; }
};

/**
 * 스킬과 무기가 Actor/GAS, Mass Entity 등 실제 대상 Backend를 직접 알지 않게 하는 공통 진입점.
 * 현재 Non-Actor Backend는 Mass 몬스터이며, 이후 다른 Backend가 생겨도 이 Subsystem 내부에서만 확장한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASCombatTargetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool IsValidActorTarget(const AActor* Target, const AActor* Instigator, bool bCanHitPlayers);
	static int32 ApplyEffectSpecsToActor(TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs, AActor* Target);
	static int32 ApplyEffectSpecToActor(const FGameplayEffectSpecHandle& EffectSpec, AActor* Target);

	/** Query의 공간·생존·진영 규칙으로 Actor 후보만 찾는다. */
	TArray<AActor*> FindActorTargets(const FASCombatTargetQuery& Query) const;

	/**
	 * Query로 Actor/Mass를 함께 판정하고 EffectSpec을 적용한다.
	 * 투사체·돌진처럼 여러 프레임에 걸쳐 같은 대상을 한 번만 맞혀야 하면 InOutHitTargets를 전달한다.
	 */
	FASCombatApplyResult ResolveAndApply(const FASCombatTargetQuery& Query,
		TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
		FASCombatTargetHitSet* InOutHitTargets = nullptr);

	/** Query 정렬 정책으로 고른 단일 Actor/Mass 타깃 하나에만 EffectSpec을 적용한다. */
	FASCombatApplyResult ResolveAndApplyBestTarget(const FASCombatTargetQuery& Query,
		TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs);

	UE_DEPRECATED(5.8, "Use FindActorTargets with FASCombatTargetQuery instead.")
	TArray<AActor*> FindActorTargetsInRadius(const FVector& Center, float Radius,
		const AActor* Instigator, bool bCanHitPlayers) const;

	/** Query 정책으로 Actor/Mass 후보를 모두 비교해 가장 적합한 타겟 하나를 반환한다. */
	bool FindBestTarget(const FASCombatTargetQuery& Query, FASCombatTargetRef& OutTarget) const;

	/** 이전에 반환한 타겟의 유효성, 생존 상태와 최신 위치를 갱신한다. */
	bool RefreshTarget(const FASCombatTargetQuery& Query, FASCombatTargetRef& InOutTarget) const;

	/** 서버가 클라이언트가 전달한 Mass replication ID를 현재 서버 Entity로 검증할 때 사용한다. */
	bool GetMassTargetByNetworkId(uint32 MassNetworkId, FASCombatTargetRef& OutTarget) const;

	int32 ApplyEffectSpecsToActors(TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
		TConstArrayView<AActor*> Targets) const;

	UE_DEPRECATED(5.8, "Use ResolveAndApply with FASCombatTargetQuery instead.")
	FASCombatApplyResult ApplyEffectSpecsInRadius(const FVector& Center, float Radius,
		TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs, AActor* Instigator, bool bCanHitPlayers);

	UE_DEPRECATED(5.8, "Use ResolveAndApply with FASCombatTargetQuery instead.")
	int32 ApplyEffectSpecsToNonActorTargetsInRadius(const FVector& Center, float Radius,
		TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs, AActor* Instigator, int32 MaxTargets = 0);

	UE_DEPRECATED(5.8, "Use ResolveAndApply with FASCombatTargetQuery instead.")
	int32 ApplyEffectSpecsToNonActorTargetsInCone(const FVector& Center, const FVector& AimDirection,
		float Radius, float AngleDegrees, TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
		AActor* Instigator, int32 MaxTargets = 0);

	UE_DEPRECATED(5.8, "Use ResolveAndApply with FASCombatTargetQuery instead.")
	int32 ApplyEffectSpecsToNonActorTargetsAlongSegmentUnique(const FVector& Start, const FVector& End,
		float SweepRadius, TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs, AActor* Instigator,
		FASCombatTargetHitSet& InOutHitTargets, int32 MaxTargets = 1);

private:
	bool MatchesQueryShape(const FASCombatTargetQuery& Query, const FVector& Location, float HitRadius) const;
	float GetQuerySortKey(const FASCombatTargetQuery& Query, const FVector& Location) const;
	static float CalculateDamageMagnitude(TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs);
};
