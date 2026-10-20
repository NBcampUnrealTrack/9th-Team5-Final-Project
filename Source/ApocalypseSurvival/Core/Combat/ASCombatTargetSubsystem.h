#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASCombatTargetSubsystem.generated.h"

class AActor;

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

	TArray<AActor*> FindActorTargetsInRadius(const FVector& Center, float Radius,
		const AActor* Instigator, bool bCanHitPlayers) const;

	int32 ApplyEffectSpecsToActors(TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
		TConstArrayView<AActor*> Targets) const;

	FASCombatApplyResult ApplyEffectSpecsInRadius(const FVector& Center, float Radius,
		TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs, AActor* Instigator, bool bCanHitPlayers);

	int32 ApplyEffectSpecsToNonActorTargetsInRadius(const FVector& Center, float Radius,
		TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs, AActor* Instigator, int32 MaxTargets = 0);

	int32 ApplyEffectSpecsToNonActorTargetsInCone(const FVector& Center, const FVector& AimDirection,
		float Radius, float AngleDegrees, TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
		AActor* Instigator, int32 MaxTargets = 0);

	int32 ApplyEffectSpecsToNonActorTargetsAlongSegmentUnique(const FVector& Start, const FVector& End,
		float SweepRadius, TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs, AActor* Instigator,
		FASCombatTargetHitSet& InOutHitTargets, int32 MaxTargets = 1);

private:
	static float CalculateDamageMagnitude(TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs);
};
