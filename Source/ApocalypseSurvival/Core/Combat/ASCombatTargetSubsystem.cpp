#include "Core/Combat/ASCombatTargetSubsystem.h"

#include <GAS/ASAbilityTags.h>

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "MapSystem/Vision/ASVisionFunctionLibrary.h"
#include "Skills/ASSkillTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASCombatTargetSubsystem)

namespace
{
	bool IsAliveActorTarget(const AActor* Target)
	{
		const UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));
		return TargetASC
			&& !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
			&& !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead);
	}
}

bool FASCombatTargetHitSet::ContainsActor(const AActor* Actor) const
{
	return IsValid(Actor) && Actors.Contains(Actor);
}

bool FASCombatTargetRef::IsValid() const
{
	return Actor.IsValid() || MassTargetId != 0;
}

bool FASCombatTargetRef::IsActor() const
{
	return Actor.IsValid();
}

bool FASCombatTargetRef::IsMass() const
{
	return MassTargetId != 0;
}

AActor* FASCombatTargetRef::GetActor() const
{
	return Actor.Get();
}

void FASCombatTargetRef::Reset()
{
	Actor.Reset();
	MassTargetId = 0;
	Location = FVector::ZeroVector;
	HitRadius = 0.0f;
}

bool FASCombatTargetHitSet::AddActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const int32 PreviousNum = Actors.Num();
	Actors.Add(Actor);
	return Actors.Num() > PreviousNum;
}

int32 FASCombatTargetHitSet::Num() const
{
	return Actors.Num() + NonActorTargetIds.Num();
}

void FASCombatTargetHitSet::Reset()
{
	Actors.Reset();
	NonActorTargetIds.Reset();
}

bool UASCombatTargetSubsystem::IsValidActorTarget(
	const AActor* Target,
	const AActor* Instigator,
	const bool bCanHitPlayers)
{
	if (!IsValid(Target) || Target == Instigator)
	{
		return false;
	}

	if (!bCanHitPlayers)
	{
		if (const APawn* Pawn = Cast<APawn>(Target); Pawn && Pawn->IsPlayerControlled())
		{
			return false;
		}
	}

	if (const IGenericTeamAgentInterface* InstigatorTeam = Cast<IGenericTeamAgentInterface>(Instigator))
	{
		if (InstigatorTeam->GetTeamAttitudeTowards(*Target) != ETeamAttitude::Hostile)
		{
			return false;
		}
	}

	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target)) != nullptr;
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecsToActor(
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Target)
{
	if (!IsValid(Target))
	{
		return 0;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!TargetASC || TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Invulnerable))
	{
		return 0;
	}

	int32 AppliedSpecCount = 0;
	for (const FGameplayEffectSpecHandle& EffectSpec : EffectSpecs)
	{
		if (!EffectSpec.IsValid())
		{
			continue;
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
		++AppliedSpecCount;
	}
	return AppliedSpecCount;
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecToActor(
	const FGameplayEffectSpecHandle& EffectSpec,
	AActor* Target)
{
	return ApplyEffectSpecsToActor(TConstArrayView<FGameplayEffectSpecHandle>(&EffectSpec, 1), Target);
}

TArray<AActor*> UASCombatTargetSubsystem::FindActorTargetsInRadius(
	const FVector& Center,
	const float Radius,
	const AActor* Instigator,
	const bool bCanHitPlayers) const
{
	TArray<AActor*> Targets;
	const UWorld* World = GetWorld();
	if (!World || Radius <= 0.0f)
	{
		return Targets;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASCombatFindTargets), false, Instigator);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (IsValidActorTarget(Actor, Instigator, bCanHitPlayers))
		{
			Targets.AddUnique(Actor);
		}
	}
	return Targets;
}

bool UASCombatTargetSubsystem::FindBestTarget(
	const FASCombatTargetQuery& Query,
	FASCombatTargetRef& OutTarget) const
{
	OutTarget.Reset();
	const UWorld* World = GetWorld();
	if (!World || Query.SearchRadius < 0.0f)
	{
		return false;
	}

	float BestPreferredDistanceSquared = TNumericLimits<float>::Max();
	float BestOriginDistanceSquared = TNumericLimits<float>::Max();
	auto IsBetterCandidate = [&Query, &BestPreferredDistanceSquared, &BestOriginDistanceSquared](const FVector& Location)
	{
		const float PreferredDistanceSquared = FVector::DistSquared2D(Location, Query.PreferredLocation);
		const float OriginDistanceSquared = FVector::DistSquared2D(Location, Query.SearchOrigin);
		return PreferredDistanceSquared < BestPreferredDistanceSquared
			|| (FMath::IsNearlyEqual(PreferredDistanceSquared, BestPreferredDistanceSquared)
				&& OriginDistanceSquared < BestOriginDistanceSquared);
	};

	if (Query.Domain != EASCombatTargetDomain::MassOnly)
	{
		const TArray<AActor*> ActorTargets = FindActorTargetsInRadius(
			Query.SearchOrigin, Query.SearchRadius, Query.Instigator, Query.bCanHitPlayers);
		for (AActor* Actor : ActorTargets)
		{
			if (Query.bRequireAlive && !IsAliveActorTarget(Actor))
			{
				continue;
			}

			if (Query.bRequireVisibleToLocalTeam
				&& !UASVisionFunctionLibrary::IsActorVisibleToLocalTeam(Actor))
			{
				continue;
			}

			const FVector Location = Actor->GetActorLocation();
			if (!IsBetterCandidate(Location))
			{
				continue;
			}

			OutTarget.Reset();
			OutTarget.Actor = Actor;
			OutTarget.Location = Location;
			BestPreferredDistanceSquared = FVector::DistSquared2D(Location, Query.PreferredLocation);
			BestOriginDistanceSquared = FVector::DistSquared2D(Location, Query.SearchOrigin);
		}
	}

	if (Query.Domain != EASCombatTargetDomain::ActorOnly)
	{
		const UASMassMonsterSubsystem* MassMonsters = World->GetSubsystem<UASMassMonsterSubsystem>();
		uint64 MassTargetId = 0;
		FVector MassLocation;
		float MassHitRadius = 0.0f;
		if (MassMonsters && MassMonsters->FindBestAliveMonsterTarget(
			Query.SearchOrigin, Query.SearchRadius, Query.PreferredLocation,
			MassTargetId, MassLocation, MassHitRadius)
			&& IsBetterCandidate(MassLocation))
		{
			OutTarget.Reset();
			OutTarget.MassTargetId = MassTargetId;
			OutTarget.Location = MassLocation;
			OutTarget.HitRadius = MassHitRadius;
		}
	}

	return OutTarget.IsValid();
}

bool UASCombatTargetSubsystem::RefreshTarget(
	const FASCombatTargetQuery& Query,
	FASCombatTargetRef& InOutTarget) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		InOutTarget.Reset();
		return false;
	}

	if (InOutTarget.IsActor())
	{
		AActor* Actor = InOutTarget.GetActor();
		if (Query.Domain != EASCombatTargetDomain::MassOnly
			&& IsValidActorTarget(Actor, Query.Instigator, Query.bCanHitPlayers)
			&& (!Query.bRequireAlive || IsAliveActorTarget(Actor))
			&& (!Query.bRequireVisibleToLocalTeam
				|| UASVisionFunctionLibrary::IsActorVisibleToLocalTeam(Actor)))
		{
			InOutTarget.Location = Actor->GetActorLocation();
			InOutTarget.HitRadius = 0.0f;
			return true;
		}
	}
	else if (InOutTarget.IsMass())
	{
		const UASMassMonsterSubsystem* MassMonsters = World->GetSubsystem<UASMassMonsterSubsystem>();
		if (Query.Domain != EASCombatTargetDomain::ActorOnly && MassMonsters
			&& MassMonsters->GetAliveMonsterTarget(
				InOutTarget.MassTargetId, InOutTarget.Location, InOutTarget.HitRadius))
		{
			return true;
		}
	}

	InOutTarget.Reset();
	return false;
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecsToActors(
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	const TConstArrayView<AActor*> Targets) const
{
	int32 AppliedTargetCount = 0;
	for (AActor* Target : Targets)
	{
		AppliedTargetCount += ApplyEffectSpecsToActor(EffectSpecs, Target) > 0 ? 1 : 0;
	}
	return AppliedTargetCount;
}

FASCombatApplyResult UASCombatTargetSubsystem::ApplyEffectSpecsInRadius(
	const FVector& Center,
	const float Radius,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Instigator,
	const bool bCanHitPlayers)
{
	FASCombatApplyResult Result;
	Result.ActorTargets = FindActorTargetsInRadius(Center, Radius, Instigator, bCanHitPlayers);
	ApplyEffectSpecsToActors(EffectSpecs, Result.ActorTargets);

	// 현재 bCanHitPlayers는 몬스터가 플레이어를 공격하는 경로다. 이 경우 적군인 Mass 몬스터는 제외한다.
	if (!bCanHitPlayers)
	{
		Result.NonActorHitCount = ApplyEffectSpecsToNonActorTargetsInRadius(
			Center, Radius, EffectSpecs, Instigator);
	}
	return Result;
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecsToNonActorTargetsInRadius(
	const FVector& Center,
	const float Radius,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Instigator,
	const int32 MaxTargets)
{
	const float Damage = CalculateDamageMagnitude(EffectSpecs);
	UASMassMonsterSubsystem* MassMonsters = GetWorld() ? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	return MassMonsters && Damage > 0.0f
		? MassMonsters->ApplyDamageInRadius(Center, Radius, Damage, Instigator, MaxTargets)
		: 0;
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecsToNonActorTargetsInCone(
	const FVector& Center,
	const FVector& AimDirection,
	const float Radius,
	const float AngleDegrees,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Instigator,
	const int32 MaxTargets)
{
	const float Damage = CalculateDamageMagnitude(EffectSpecs);
	UASMassMonsterSubsystem* MassMonsters = GetWorld() ? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	return MassMonsters && Damage > 0.0f
		? MassMonsters->ApplyDamageInCone(Center, AimDirection, Radius, AngleDegrees, Damage, Instigator, MaxTargets)
		: 0;
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecsToNonActorTargetsAlongSegmentUnique(
	const FVector& Start,
	const FVector& End,
	const float SweepRadius,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Instigator,
	FASCombatTargetHitSet& InOutHitTargets,
	const int32 MaxTargets)
{
	const float Damage = CalculateDamageMagnitude(EffectSpecs);
	UASMassMonsterSubsystem* MassMonsters = GetWorld() ? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	return MassMonsters && Damage > 0.0f
		? MassMonsters->ApplyDamageAlongSegmentUnique(Start, End, SweepRadius, Damage, Instigator,
			InOutHitTargets.NonActorTargetIds, MaxTargets)
		: 0;
}

float UASCombatTargetSubsystem::CalculateDamageMagnitude(
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs)
{
	float Damage = 0.0f;
	for (const FGameplayEffectSpecHandle& EffectSpec : EffectSpecs)
	{
		if (EffectSpec.IsValid())
		{
			Damage += FMath::Max(0.0f,
				-EffectSpec.Data->GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f));
		}
	}
	return Damage;
}
