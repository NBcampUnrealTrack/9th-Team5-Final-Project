#include "Core/Combat/ASCombatTargetSubsystem.h"

#include <GAS/ASAbilityTags.h>
#include "Core/Combat/ASCombatDiagnostics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassSimulationSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "Mass/Monster/Subsystems/ASMassMonsterTargetQuerySubsystem.h"
#include "MapSystem/Vision/ASVisionFunctionLibrary.h"
#include "Skills/ASSkillTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASCombatTargetSubsystem)

namespace
{
	bool IsActorTargetAllowed(const AActor* Target, const FASCombatTargetQuery& Query)
	{
		if (Query.bHitAllTeams)
		{
			return IsValid(Target)
				&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));
		}
		return UASCombatTargetSubsystem::IsValidActorTarget(Target, Query.Instigator, Query.bCanHitPlayers);
	}

	bool IsAliveActorTarget(const AActor* Target)
	{
		const UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));
		return TargetASC
			&& !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
			&& !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Downed)
			&& !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead);
	}
}

bool UASCombatTargetSubsystem::DeferUntilMassIdle(FTimerHandle& PendingTimer, FTimerDelegate Callback)
{
	UMassEntitySubsystem* Entities = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!Entities || !Entities->GetEntityManager().IsProcessing()) return false;
	if (!GetWorld()->GetTimerManager().IsTimerActive(PendingTimer))
	{
		PendingTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(MoveTemp(Callback));
		ASCombatDiagnostics::Record(this, TEXT("Combat.DeferredUntilMassIdle"));
	}
	return true;
}

void UASCombatTargetSubsystem::AddMassTickPrerequisites(FTickFunction& TickFunction)
{
	TickFunction.TickGroup = TG_PostUpdateWork;
	TickFunction.EndTickGroup = TG_LastDemotable;
	if (UMassSimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UMassSimulationSubsystem>())
	{
		FMassProcessingPhaseManager& Phases = Simulation->GetMutablePhaseManager();
		for (uint8 Index = 0; Index < static_cast<uint8>(EMassProcessingPhase::MAX); ++Index)
		{
			TickFunction.AddPrerequisite(Simulation, Phases.GetProcessingPhaseTickFunction(static_cast<EMassProcessingPhase>(Index)));
		}
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
	MassNetworkId = 0;
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
	if (!TargetASC
		|| TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Downed)
		|| TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead)
		|| TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Invulnerable))
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

int32 UASCombatTargetSubsystem::ApplyEffectSpecsToActor(
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Target,
	const FASCombatImpactContext& ImpactContext)
{
	const int32 AppliedSpecCount = ApplyEffectSpecsToActor(EffectSpecs, Target);
	if (AppliedSpecCount > 0)
	{
		if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(Target))
		{
			Monster->ApplyImpactReaction(ImpactContext);
			Monster->ApplyImpactKnockback(ImpactContext);
		}
		else if (AASPlayerCharacter* Player = Cast<AASPlayerCharacter>(Target))
		{
			Player->ApplyImpactReaction(ImpactContext);
		}
	}
	return AppliedSpecCount;
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecToActor(
	const FGameplayEffectSpecHandle& EffectSpec,
	AActor* Target,
	const FASCombatImpactContext& ImpactContext)
{
	return ApplyEffectSpecsToActor(TConstArrayView<FGameplayEffectSpecHandle>(&EffectSpec, 1), Target, ImpactContext);
}

TArray<AActor*> UASCombatTargetSubsystem::FindActorTargetsInRadius(
	const FVector& Center,
	const float Radius,
	const AActor* Instigator,
	const bool bCanHitPlayers) const
{
	FASCombatTargetQuery Query;
	Query.SearchOrigin = Center;
	Query.SearchRadius = Radius;
	Query.Instigator = Instigator;
	Query.Domain = EASCombatTargetDomain::ActorOnly;
	Query.bCanHitPlayers = bCanHitPlayers;
	Query.bRequireAlive = false;
	return FindActorTargets(Query);
}

TArray<AActor*> UASCombatTargetSubsystem::FindActorTargets(const FASCombatTargetQuery& Query) const
{
	TArray<AActor*> Targets;
	if (Query.Domain == EASCombatTargetDomain::MassOnly)
	{
		return Targets;
	}

	const UWorld* World = GetWorld();
	if (!World || Query.SearchRadius < 0.0f)
	{
		return Targets;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASCombatFindTargets), false,
		Query.bHitAllTeams ? nullptr : Query.Instigator);
	const FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
	if (Query.Shape == EASCombatTargetShape::Segment && !Query.SearchOrigin.Equals(Query.EndLocation))
	{
		TArray<FHitResult> Hits;
		World->SweepMultiByObjectType(Hits, Query.SearchOrigin, Query.EndLocation, FQuat::Identity,
			ObjectParams, FCollisionShape::MakeSphere(Query.SearchRadius), Params);
		for (const FHitResult& Hit : Hits)
		{
			AActor* Actor = Hit.GetActor();
			if ((!Query.ExplicitActor || Actor == Query.ExplicitActor)
				&& IsActorTargetAllowed(Actor, Query)
				&& (!Query.bRequireAlive || IsAliveActorTarget(Actor))
				&& (!Query.bRequireVisibleToLocalTeam || UASVisionFunctionLibrary::IsActorVisibleToLocalTeam(Actor))
				&& MatchesQueryShape(Query, Actor->GetActorLocation(), 0.0f))
			{
				Targets.AddUnique(Actor);
			}
		}
	}
	else
	{
		TArray<FOverlapResult> Overlaps;
		const bool bIsBox = Query.Shape == EASCombatTargetShape::Box;
		const FQuat QueryRotation = bIsBox ? Query.Direction.GetSafeNormal2D().Rotation().Quaternion() : FQuat::Identity;
		const FCollisionShape QueryShape = bIsBox
			? FCollisionShape::MakeBox(Query.BoxHalfExtent)
			: FCollisionShape::MakeSphere(Query.SearchRadius);
		World->OverlapMultiByObjectType(Overlaps, Query.SearchOrigin, QueryRotation, ObjectParams, QueryShape, Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Actor = Overlap.GetActor();
			if ((!Query.ExplicitActor || Actor == Query.ExplicitActor)
				&& IsActorTargetAllowed(Actor, Query)
				&& (!Query.bRequireAlive || IsAliveActorTarget(Actor))
				&& (!Query.bRequireVisibleToLocalTeam || UASVisionFunctionLibrary::IsActorVisibleToLocalTeam(Actor))
				&& MatchesQueryShape(Query, Actor->GetActorLocation(), 0.0f))
			{
				Targets.AddUnique(Actor);
			}
		}
	}

	Targets.Sort([this, &Query](const AActor& Left, const AActor& Right)
	{
		return GetQuerySortKey(Query, Left.GetActorLocation()) < GetQuerySortKey(Query, Right.GetActorLocation());
	});
	return Targets;
}

void UASCombatTargetSubsystem::FindTargetsAlongSegment(
	const FASCombatTargetQuery& Query, TArray<FASCombatTargetRef>& OutTargets) const
{
	OutTargets.Reset();
	if (Query.Shape != EASCombatTargetShape::Segment || Query.SearchRadius < 0.0f)
	{
		return;
	}

	if (Query.Domain != EASCombatTargetDomain::MassOnly)
	{
		for (AActor* Actor : FindActorTargets(Query))
		{
			FASCombatTargetRef& Target = OutTargets.AddDefaulted_GetRef();
			Target.Actor = Actor;
			Target.Location = Actor->GetActorLocation();
		}
	}

	if (Query.Domain != EASCombatTargetDomain::ActorOnly)
	{
		if (const UASMassMonsterSubsystem* MassMonsters = GetWorld()
			? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>() : nullptr)
		{
			TArray<FASMassMonsterTargetSnapshot> MassTargets;
			MassMonsters->FindAliveMonsterTargetsAlongSegment(
				Query.SearchOrigin, Query.EndLocation, Query.SearchRadius, MassTargets);
			for (const FASMassMonsterTargetSnapshot& MassTarget : MassTargets)
			{
				FASCombatTargetRef& Target = OutTargets.AddDefaulted_GetRef();
				Target.MassTargetId = MassTarget.TargetId;
				Target.MassNetworkId = MassTarget.NetworkId;
				Target.Location = MassTarget.Location;
				Target.HitRadius = MassTarget.HitRadius;
			}
		}
	}

	OutTargets.Sort([this, &Query](const FASCombatTargetRef& Left, const FASCombatTargetRef& Right)
	{
		return GetQuerySortKey(Query, Left.GetLocation()) < GetQuerySortKey(Query, Right.GetLocation());
	});
	if (Query.MaxTargets > 0 && OutTargets.Num() > Query.MaxTargets)
	{
		OutTargets.SetNum(Query.MaxTargets);
	}
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

	const FVector& SortOrigin = Query.SortPolicy == EASCombatTargetSortPolicy::NearestToPriorityLocation
		? Query.PriorityLocation
		: Query.SearchOrigin;
	float BestSortDistanceSquared = TNumericLimits<float>::Max();
	float BestOriginDistanceSquared = TNumericLimits<float>::Max();
	auto IsBetterCandidate = [&Query, &SortOrigin, &BestSortDistanceSquared, &BestOriginDistanceSquared](const FVector& Location)
	{
		const float SortDistanceSquared = FVector::DistSquared2D(Location, SortOrigin);
		const float OriginDistanceSquared = FVector::DistSquared2D(Location, Query.SearchOrigin);
		return SortDistanceSquared < BestSortDistanceSquared
			|| (FMath::IsNearlyEqual(SortDistanceSquared, BestSortDistanceSquared)
				&& OriginDistanceSquared < BestOriginDistanceSquared);
	};

	if (Query.Domain != EASCombatTargetDomain::MassOnly)
	{
		FASCombatTargetQuery ActorQuery = Query;
		ActorQuery.Shape = EASCombatTargetShape::Radius;
		const TArray<AActor*> ActorTargets = FindActorTargets(ActorQuery);
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
			BestSortDistanceSquared = FVector::DistSquared2D(Location, SortOrigin);
			BestOriginDistanceSquared = FVector::DistSquared2D(Location, Query.SearchOrigin);
		}
	}

	if (Query.Domain != EASCombatTargetDomain::ActorOnly)
	{
		const UASMassMonsterTargetQuerySubsystem* MassTargets =
			World->GetSubsystem<UASMassMonsterTargetQuerySubsystem>();
		uint64 MassTargetId = 0;
		FVector MassLocation;
		float MassHitRadius = 0.0f;
		if (MassTargets && MassTargets->FindBestAliveMonsterTarget(
			Query.SearchOrigin, Query.SearchRadius, SortOrigin, Query.bRequireVisibleToLocalTeam,
			MassTargetId, MassLocation, MassHitRadius)
			&& IsBetterCandidate(MassLocation))
		{
			OutTarget.Reset();
			OutTarget.MassTargetId = MassTargetId;
			MassTargets->GetMassNetworkId(MassTargetId, OutTarget.MassNetworkId);
			OutTarget.Location = MassLocation;
			OutTarget.HitRadius = MassHitRadius;
		}
	}

	return OutTarget.IsValid();
}

bool UASCombatTargetSubsystem::GetMassTargetByNetworkId(
	const uint32 MassNetworkId,
	FASCombatTargetRef& OutTarget) const
{
	OutTarget.Reset();
	const UWorld* World = GetWorld();
	if (!World || MassNetworkId == 0)
	{
		return false;
	}

	const UASMassMonsterTargetQuerySubsystem* MassTargets =
		World->GetSubsystem<UASMassMonsterTargetQuerySubsystem>();
	uint64 MassTargetId = 0;
	FVector Location;
	float HitRadius = 0.0f;
	if (!MassTargets || !MassTargets->GetAliveMonsterTargetByNetworkId(
		MassNetworkId, MassTargetId, Location, HitRadius))
	{
		return false;
	}

	OutTarget.MassTargetId = MassTargetId;
	OutTarget.MassNetworkId = MassNetworkId;
	OutTarget.Location = Location;
	OutTarget.HitRadius = HitRadius;
	return true;
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
			&& IsActorTargetAllowed(Actor, Query)
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
		const UASMassMonsterTargetQuerySubsystem* MassTargets =
			World->GetSubsystem<UASMassMonsterTargetQuerySubsystem>();
		if (Query.Domain != EASCombatTargetDomain::ActorOnly && MassTargets
			&& MassTargets->GetAliveMonsterTarget(InOutTarget.MassTargetId,
				Query.bRequireVisibleToLocalTeam, InOutTarget.Location, InOutTarget.HitRadius))
		{
			return true;
		}
	}

	InOutTarget.Reset();
	return false;
}

FASCombatApplyResult UASCombatTargetSubsystem::ResolveAndApplyBestTarget(
	const FASCombatTargetQuery& Query,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs)
{
	return ResolveAndApplyBestTarget(Query, EffectSpecs, FASCombatImpactContext());
}

FASCombatApplyResult UASCombatTargetSubsystem::ResolveAndApplyBestTarget(
	const FASCombatTargetQuery& Query,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	const FASCombatImpactContext& ImpactContext)
{
	FASCombatApplyResult Result;
	FASCombatTargetRef BestTarget;
	if (!FindBestTarget(Query, BestTarget))
	{
		return Result;
	}

	if (AActor* Actor = BestTarget.GetActor())
	{
		if (ApplyEffectSpecsToActor(EffectSpecs, Actor, ImpactContext) > 0)
		{
			Result.ActorTargets.Add(Actor);
		}
		return Result;
	}

	const float Damage = CalculateDamageMagnitude(EffectSpecs);
	UASMassMonsterSubsystem* MassMonsters = GetWorld()
		? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>()
		: nullptr;
	if (MassMonsters && Damage > 0.0f)
	{
		Result.NonActorHitCount = MassMonsters->ApplyDamageToTarget(
			BestTarget.GetMassTargetId(), Damage, const_cast<AActor*>(Query.Instigator), ImpactContext);
		if (Result.NonActorHitCount > 0)
		{
			MassMonsters->ApplyImpactKnockbackToTarget(BestTarget.GetMassTargetId(), ImpactContext);
		}
	}
	return Result;
}

FASCombatApplyResult UASCombatTargetSubsystem::ApplyEffectSpecsToTarget(
	const FASCombatTargetRef& Target,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Instigator)
{
	return ApplyEffectSpecsToTarget(Target, EffectSpecs, Instigator, FASCombatImpactContext());
}

FASCombatApplyResult UASCombatTargetSubsystem::ApplyEffectSpecsToTarget(
	const FASCombatTargetRef& Target,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Instigator,
	const FASCombatImpactContext& ImpactContext)
{
	ASCombatDiagnostics::Record(this, TEXT("Target.Apply"));
	FASCombatApplyResult Result;
	FASCombatTargetRef RefreshedTarget = Target;
	FASCombatTargetQuery ValidationQuery;
	ValidationQuery.Instigator = Instigator;
	ValidationQuery.MaxTargets = 1;
	if (!RefreshTarget(ValidationQuery, RefreshedTarget))
	{
		ASCombatDiagnostics::Record(this, TEXT("Target.Rejected RefreshFailed"));
		return Result;
	}

	if (AActor* Actor = RefreshedTarget.GetActor())
	{
		if (ApplyEffectSpecsToActor(EffectSpecs, Actor, ImpactContext) > 0)
		{
			Result.ActorTargets.Add(Actor);
		}
		return Result;
	}

	const float Damage = CalculateDamageMagnitude(EffectSpecs);
	UASMassMonsterSubsystem* MassMonsters = GetWorld()
		? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>()
		: nullptr;
	if (MassMonsters && Damage > 0.0f)
	{
		Result.NonActorHitCount = MassMonsters->ApplyDamageToTarget(
			RefreshedTarget.GetMassTargetId(), Damage, Instigator, ImpactContext);
		if (Result.NonActorHitCount > 0)
		{
			MassMonsters->ApplyImpactKnockbackToTarget(
				RefreshedTarget.GetMassTargetId(), ImpactContext);
		}
	}
	if (ASCombatDiagnostics::IsEnabled())
	{
		ASCombatDiagnostics::Record(this, FString::Printf(TEXT("Target.MassResult Damage=%.3f Hits=%d"), Damage, Result.NonActorHitCount));
	}
	return Result;
}

bool UASCombatTargetSubsystem::ApplyImpactKnockbackToTarget(
	const FASCombatTargetRef& Target, const FASCombatImpactContext& ImpactContext)
{
	if (ImpactContext.KnockbackDistance <= 0.0f)
	{
		return false;
	}

	FASCombatTargetRef RefreshedTarget = Target;
	FASCombatTargetQuery ValidationQuery;
	if (!RefreshTarget(ValidationQuery, RefreshedTarget))
	{
		return false;
	}

	if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(RefreshedTarget.GetActor()))
	{
		Monster->ApplyImpactKnockback(ImpactContext);
		return true;
	}

	if (!RefreshedTarget.IsMass())
	{
		return false;
	}
	if (UASMassMonsterSubsystem* MassMonsters = GetWorld()
		? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>() : nullptr)
	{
		return MassMonsters->ApplyImpactKnockbackToTarget(
			RefreshedTarget.GetMassTargetId(), ImpactContext);
	}
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
	FASCombatTargetQuery Query;
	Query.SearchOrigin = Center;
	Query.SearchRadius = Radius;
	Query.Instigator = Instigator;
	Query.bCanHitPlayers = bCanHitPlayers;
	return ResolveAndApply(Query, EffectSpecs);
}

int32 UASCombatTargetSubsystem::ApplyEffectSpecsToNonActorTargetsInRadius(
	const FVector& Center,
	const float Radius,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	AActor* Instigator,
	const int32 MaxTargets)
{
	FASCombatTargetQuery Query;
	Query.SearchOrigin = Center;
	Query.SearchRadius = Radius;
	Query.Instigator = Instigator;
	Query.Domain = EASCombatTargetDomain::MassOnly;
	Query.MaxTargets = MaxTargets;
	return ResolveAndApply(Query, EffectSpecs).NonActorHitCount;
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
	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Cone;
	Query.SearchOrigin = Center;
	Query.Direction = AimDirection;
	Query.SearchRadius = Radius;
	Query.AngleDegrees = AngleDegrees;
	Query.Instigator = Instigator;
	Query.Domain = EASCombatTargetDomain::MassOnly;
	Query.MaxTargets = MaxTargets;
	return ResolveAndApply(Query, EffectSpecs).NonActorHitCount;
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
	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Segment;
	Query.SearchOrigin = Start;
	Query.EndLocation = End;
	Query.SearchRadius = SweepRadius;
	Query.Instigator = Instigator;
	Query.Domain = EASCombatTargetDomain::MassOnly;
	Query.MaxTargets = MaxTargets;
	return ResolveAndApply(Query, EffectSpecs, &InOutHitTargets).NonActorHitCount;
}

FASCombatApplyResult UASCombatTargetSubsystem::ResolveAndApply(
	const FASCombatTargetQuery& Query,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	FASCombatTargetHitSet* InOutHitTargets)
{
	return ResolveAndApply(Query, EffectSpecs, FASCombatImpactContext(), InOutHitTargets);
}

FASCombatApplyResult UASCombatTargetSubsystem::ResolveAndApply(
	const FASCombatTargetQuery& Query,
	const TConstArrayView<FGameplayEffectSpecHandle> EffectSpecs,
	const FASCombatImpactContext& ImpactContext,
	FASCombatTargetHitSet* InOutHitTargets)
{
	if (ASCombatDiagnostics::IsEnabled())
	{
		ASCombatDiagnostics::Record(this, FString::Printf(TEXT("Query.Begin Shape=%d Domain=%d Origin=%s End=%s Aim=%s Radius=%.2f Angle=%.2f Specs=%d Damage=%.3f"),
			static_cast<int32>(Query.Shape), static_cast<int32>(Query.Domain), *Query.SearchOrigin.ToString(),
			*Query.EndLocation.ToString(), *Query.Direction.ToString(), Query.SearchRadius, Query.AngleDegrees,
			EffectSpecs.Num(), CalculateDamageMagnitude(EffectSpecs)));
	}
	FASCombatApplyResult Result;
	if (Query.SearchRadius < 0.0f)
	{
		return Result;
	}

	if (Query.Domain != EASCombatTargetDomain::MassOnly)
	{
		for (AActor* Actor : FindActorTargets(Query))
		{
			if (InOutHitTargets && InOutHitTargets->ContainsActor(Actor))
			{
				continue;
			}
			if (Query.MaxTargets > 0 && Result.GetTotalHitCount() >= Query.MaxTargets)
			{
				break;
			}

			if (ApplyEffectSpecsToActor(EffectSpecs, Actor, ImpactContext) > 0)
			{
				Result.ActorTargets.Add(Actor);
				if (InOutHitTargets)
				{
					InOutHitTargets->AddActor(Actor);
				}
			}
		}
	}

	// 현재 Mass 몬스터는 플레이어의 적이다. 무차별 공격을 제외하면 기존 플레이어 공격 경로에는 포함하지 않는다.
	if (Query.Domain == EASCombatTargetDomain::ActorOnly || Query.ExplicitActor
		|| (Query.bCanHitPlayers && !Query.bHitAllTeams))
	{
		return Result;
	}

	const float Damage = CalculateDamageMagnitude(EffectSpecs);
	UASMassMonsterSubsystem* MassMonsters = GetWorld() ? GetWorld()->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	if (!MassMonsters || Damage <= 0.0f)
	{
		ASCombatDiagnostics::Record(this, TEXT("Query.MassRejected MissingSubsystemOrDamage"));
		return Result;
	}

	const int32 RemainingTargets = Query.MaxTargets > 0
		? FMath::Max(0, Query.MaxTargets - Result.ActorTargets.Num())
		: 0;
	if (Query.MaxTargets > 0 && RemainingTargets <= 0)
	{
		return Result;
	}

	switch (Query.Shape)
	{
	case EASCombatTargetShape::Radius:
		Result.NonActorHitCount = MassMonsters->ApplyDamageInRadiusWithImpact(
			Query.SearchOrigin, Query.SearchRadius, Damage, const_cast<AActor*>(Query.Instigator), RemainingTargets,
			ImpactContext);
		break;
	case EASCombatTargetShape::Cone:
		Result.NonActorHitCount = MassMonsters->ApplyDamageInConeWithImpact(
			Query.SearchOrigin, Query.Direction, Query.SearchRadius, Query.AngleDegrees,
			Damage, const_cast<AActor*>(Query.Instigator), RemainingTargets, ImpactContext);
		break;
	case EASCombatTargetShape::Segment:
		if (InOutHitTargets)
		{
			Result.NonActorHitCount = MassMonsters->ApplyDamageAlongSegmentUnique(
				Query.SearchOrigin, Query.EndLocation, Query.SearchRadius, Damage,
				const_cast<AActor*>(Query.Instigator), InOutHitTargets->NonActorTargetIds, RemainingTargets,
				ImpactContext);
		}
		else
		{
			Result.NonActorHitCount = MassMonsters->ApplyDamageAlongSegmentWithImpact(
				Query.SearchOrigin, Query.EndLocation, Query.SearchRadius, Damage,
				const_cast<AActor*>(Query.Instigator), RemainingTargets, ImpactContext);
		}
		break;
	default:
		break;
	}

	if (ASCombatDiagnostics::IsEnabled())
	{
		ASCombatDiagnostics::Record(this, FString::Printf(TEXT("Query.Result ActorHits=%d MassHits=%d"),
			Result.ActorTargets.Num(), Result.NonActorHitCount));
	}
	return Result;
}

bool UASCombatTargetSubsystem::MatchesQueryShape(
	const FASCombatTargetQuery& Query,
	const FVector& Location,
	const float HitRadius) const
{
	const float SafeRadius = FMath::Max(0.0f, Query.SearchRadius) + FMath::Max(0.0f, HitRadius);
	switch (Query.Shape)
	{
	case EASCombatTargetShape::Radius:
		return FVector::DistSquared2D(Query.SearchOrigin, Location) <= FMath::Square(SafeRadius);
	case EASCombatTargetShape::Cone:
	{
		const FVector Offset = Location - Query.SearchOrigin;
		if (Offset.SizeSquared2D() > FMath::Square(SafeRadius))
		{
			return false;
		}
		const float SafeAngle = FMath::Clamp(Query.AngleDegrees, 1.0f, 360.0f);
		const FVector SafeDirection = Query.Direction.GetSafeNormal2D();
		if (SafeAngle >= 360.0f || SafeDirection.IsNearlyZero())
		{
			return true;
		}
		const FVector ToTarget = Offset.GetSafeNormal2D();
		return ToTarget.IsNearlyZero()
			|| FVector::DotProduct(SafeDirection, ToTarget) >= FMath::Cos(FMath::DegreesToRadians(SafeAngle * 0.5f));
	}
	case EASCombatTargetShape::Segment:
	{
		const FVector Start2D(Query.SearchOrigin.X, Query.SearchOrigin.Y, 0.0f);
		const FVector End2D(Query.EndLocation.X, Query.EndLocation.Y, 0.0f);
		const FVector Location2D(Location.X, Location.Y, 0.0f);
		return FMath::PointDistToSegmentSquared(Location2D, Start2D, End2D) <= FMath::Square(SafeRadius);
	}
	case EASCombatTargetShape::Box:
	{
		const FVector Forward = Query.Direction.GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			return false;
		}

		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		const FVector Offset = Location - Query.SearchOrigin;
		const float SafeHalfLength = FMath::Max(0.0f, Query.BoxHalfExtent.X) + FMath::Max(0.0f, HitRadius);
		const float SafeHalfWidth = FMath::Max(0.0f, Query.BoxHalfExtent.Y) + FMath::Max(0.0f, HitRadius);
		return FMath::Abs(FVector::DotProduct(Offset, Forward)) <= SafeHalfLength
			&& FMath::Abs(FVector::DotProduct(Offset, Right)) <= SafeHalfWidth;
	}
	default:
		return false;
	}
}

float UASCombatTargetSubsystem::GetQuerySortKey(
	const FASCombatTargetQuery& Query,
	const FVector& Location) const
{
	if (Query.Shape == EASCombatTargetShape::Segment)
	{
		const FVector Start2D(Query.SearchOrigin.X, Query.SearchOrigin.Y, 0.0f);
		const FVector End2D(Query.EndLocation.X, Query.EndLocation.Y, 0.0f);
		const FVector Location2D(Location.X, Location.Y, 0.0f);
		const FVector Segment = End2D - Start2D;
		const float SegmentLengthSquared = Segment.SizeSquared();
		return SegmentLengthSquared > SMALL_NUMBER
			? FMath::Clamp(FVector::DotProduct(Location2D - Start2D, Segment) / SegmentLengthSquared, 0.0f, 1.0f)
			: 0.0f;
	}

	const FVector& SortOrigin = Query.SortPolicy == EASCombatTargetSortPolicy::NearestToPriorityLocation
		? Query.PriorityLocation
		: Query.SearchOrigin;
	return FVector::DistSquared2D(Location, SortOrigin);
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
