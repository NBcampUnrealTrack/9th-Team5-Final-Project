#include "Mass/Monster/Processors/ASMassMonsterBehaviorProcessor.h"

#include "ApocalypseSurvival.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterBehaviorProcessor)

namespace ASMassMonsterBehavior
{
	struct FTargetCandidate
	{
		FASTargetSnapshot Snapshot;
		float Score = -TNumericLimits<float>::Max();
		float EffectiveThreat = 0.0f;
	};

	static float MakeRefreshInterval(const float BaseInterval, const FMassEntityHandle Entity)
	{
		const uint32 Bucket = GetTypeHash(Entity.Index) % 31u;
		const float Scale = 0.85f + (static_cast<float>(Bucket) / 30.0f) * 0.3f;
		return FMath::Max(0.01f, BaseInterval * Scale);
	}

	static float MakeInitialRefreshDelay(const float BaseInterval, const FMassEntityHandle Entity)
	{
		const uint32 Bucket = GetTypeHash(Entity.Index) % 32u;
		return FMath::Max(0.0f, BaseInterval * (static_cast<float>(Bucket) / 31.0f));
	}

	static void ClearTarget(FASMassMonsterTargetFragment& Target, const float NextRefreshDelay)
	{
		Target.Reset();
		Target.bRefreshScheduleInitialized = true;
		Target.RefreshTimeRemaining = FMath::Max(0.01f, NextRefreshDelay);
	}

	static bool IsUsablePrimaryTarget(const FASTargetSnapshot& Snapshot)
	{
		return Snapshot.Handle.IsValid()
			&& Snapshot.bAlive
			&& Snapshot.bAttackable
			&& Snapshot.Actor.IsValid()
			&& ASHasAllTargetCapabilities(Snapshot.Capabilities, EASMonsterTargetCapability::PrimaryTarget);
	}

	static void AddCandidate(TArray<FASTargetSnapshot>& Candidates,
		const FASTargetSnapshot& Snapshot)
	{
		if (!IsUsablePrimaryTarget(Snapshot))
		{
			return;
		}

		for (FASTargetSnapshot& Existing : Candidates)
		{
			if (Existing.Handle == Snapshot.Handle)
			{
				Existing = Snapshot;
				return;
			}
		}

		Candidates.Add(Snapshot);
	}

	static float CalculateProximityScore(const FVector& EntityLocation, const FVector& TargetLocation,
		const FASMassMonsterConfigFragment& Config)
	{
		if (Config.ProximityScoreMax <= 0.0f)
		{
			return 0.0f;
		}

		const float Distance = FVector::Dist2D(EntityLocation, TargetLocation);
		return Config.ProximityScoreMax / (1.0f + Distance / Config.ProximityHalfScoreDistance);
	}

	static bool IsTargetSwitchDeferred(const FASMassMonsterCombatFragment& Combat)
	{
		return Combat.AttackState == EASMassMonsterAttackState::Windup
			|| Combat.AttackState == EASMassMonsterAttackState::Executing;
	}

	static const TCHAR* GetSelectionReason(const FTargetCandidate& Candidate)
	{
		return Candidate.EffectiveThreat > 0.0f ? TEXT("ThreatScore") : TEXT("ProximityScore");
	}

	static void SetTarget(const UWorld& World, const FMassEntityHandle Entity,
		FASMassMonsterTargetFragment& Target, const FTargetCandidate& Candidate,
		const FASMassMonsterConfigFragment& Config)
	{
		const FASTargetHandle PreviousTarget = Target.PrimaryTarget;
		Target.PrimaryTarget = Candidate.Snapshot.Handle;
		Target.CachedTargetLocation = Candidate.Snapshot.Location;
		if (PreviousTarget == Target.PrimaryTarget)
		{
			return;
		}

		Target.TargetLockTimeRemaining = Config.MinTargetLockSeconds;
#if !UE_BUILD_SHIPPING
		UE_LOG(LogApocalypseSurvival, Log,
			TEXT("[MassMonsterTarget] Entity=%d Old=%d:%d New=%d:%d Actor=%s Reason=%s NetMode=%d Score=%.2f Threat=%.2f"),
			Entity.Index, PreviousTarget.Id, PreviousTarget.Generation,
			Target.PrimaryTarget.Id, Target.PrimaryTarget.Generation, *GetNameSafe(Candidate.Snapshot.Actor.Get()), GetSelectionReason(Candidate),
			static_cast<int32>(World.GetNetMode()), Candidate.Score, Candidate.EffectiveThreat);
#endif
	}

	static void GatherCandidates(const UASMonsterTargetSubsystem& TargetSubsystem,
		const FVector& EntityLocation, const FASMassMonsterTargetFragment& Target,
		FASMassMonsterThreatFragment& Threats, const FASMassMonsterHomeFragment& Home,
		const FASMassMonsterConfigFragment& Config, const bool bNightAssault,
		const double CurrentTime, TArray<FASTargetSnapshot>& OutCandidates)
	{
		OutCandidates.Reset();
		const float CandidateSearchRadius = bNightAssault ? Config.DetectionRadius : Home.DetectionRadius;
		TargetSubsystem.FindTargetsInRange(EntityLocation, CandidateSearchRadius,
			EASMonsterTargetCapability::PrimaryTarget, OutCandidates);
		if (bNightAssault)
		{
			TArray<FASTargetSnapshot> GlobalCandidates;
			TargetSubsystem.GetGlobalCandidateTargets(GlobalCandidates);
			for (const FASTargetSnapshot& Snapshot : GlobalCandidates)
			{
				AddCandidate(OutCandidates, Snapshot);
			}
		}

		FASTargetSnapshot CurrentSnapshot;
		if (Target.HasTarget() && TargetSubsystem.ResolveTarget(Target.PrimaryTarget, CurrentSnapshot)
			&& IsUsablePrimaryTarget(CurrentSnapshot))
		{
			const bool bCurrentHasThreat = Threats.GetEffectiveThreat(
				Target.PrimaryTarget, CurrentTime, Config.AggroDecayPerSecond) > 0.0f;
			const bool bCurrentInRange = bNightAssault
				|| FVector::DistSquared2D(EntityLocation, CurrentSnapshot.Location) <= FMath::Square(Home.LoseTargetRadius);
			if (bCurrentInRange || bCurrentHasThreat)
			{
				AddCandidate(OutCandidates, CurrentSnapshot);
			}
		}

		for (FASMassMonsterThreatEntry& Entry : Threats.Entries)
		{
			if (!Entry.Target.IsValid())
			{
				continue;
			}

			const float EffectiveThreat = Threats.GetEffectiveThreat(
				Entry.Target, CurrentTime, Config.AggroDecayPerSecond);
			if (EffectiveThreat <= 0.0f)
			{
				Entry = FASMassMonsterThreatEntry();
				continue;
			}

			FASTargetSnapshot ThreatSnapshot;
			if (!TargetSubsystem.ResolveTarget(Entry.Target, ThreatSnapshot)
				|| !IsUsablePrimaryTarget(ThreatSnapshot))
			{
				Entry = FASMassMonsterThreatEntry();
				continue;
			}

			AddCandidate(OutCandidates, ThreatSnapshot);
		}
	}

	static void RefreshPrimaryTarget(const UWorld& World, const FMassEntityHandle Entity,
		const UASMonsterTargetSubsystem& TargetSubsystem, const FVector& EntityLocation,
		FASMassMonsterTargetFragment& Target, FASMassMonsterThreatFragment& Threats,
		const FASMassMonsterCombatFragment& Combat, const FASMassMonsterHomeFragment& Home,
		const FASMassMonsterConfigFragment& Config, const bool bNightAssault, const double CurrentTime)
	{
		TArray<FASTargetSnapshot> Candidates;
		GatherCandidates(TargetSubsystem, EntityLocation, Target, Threats, Home, Config, bNightAssault,
			CurrentTime, Candidates);

		FTargetCandidate BestCandidate;
		FTargetCandidate CurrentCandidate;
		bool bHasBestCandidate = false;
		bool bHasCurrentCandidate = false;
		for (const FASTargetSnapshot& Snapshot : Candidates)
		{
			FTargetCandidate Candidate;
			Candidate.Snapshot = Snapshot;
			Candidate.EffectiveThreat = Threats.GetEffectiveThreat(
				Snapshot.Handle, CurrentTime, Config.AggroDecayPerSecond);
			Candidate.Score = Snapshot.BaseScore
				+ CalculateProximityScore(EntityLocation, Snapshot.Location, Config)
				+ Candidate.EffectiveThreat;

			const bool bHigherScore = !bHasBestCandidate || Candidate.Score > BestCandidate.Score;
			const bool bBreaksTie = bHasBestCandidate && FMath::IsNearlyEqual(Candidate.Score, BestCandidate.Score)
				&& Candidate.Snapshot.Handle.Id < BestCandidate.Snapshot.Handle.Id;
			if (bHigherScore || bBreaksTie)
			{
				BestCandidate = Candidate;
				bHasBestCandidate = true;
			}

			if (Snapshot.Handle == Target.PrimaryTarget)
			{
				CurrentCandidate = Candidate;
				bHasCurrentCandidate = true;
			}
		}

		if (!bHasBestCandidate)
		{
			ClearTarget(Target, Config.TargetRefreshInterval);
			return;
		}

		if (!bHasCurrentCandidate)
		{
			SetTarget(World, Entity, Target, BestCandidate, Config);
			return;
		}

		Target.CachedTargetLocation = CurrentCandidate.Snapshot.Location;
		const bool bHasBetterCandidate = BestCandidate.Snapshot.Handle != Target.PrimaryTarget
			&& BestCandidate.Score >= CurrentCandidate.Score + Config.TargetSwitchScoreMargin;
		if (bHasBetterCandidate && Target.TargetLockTimeRemaining <= 0.0f && !IsTargetSwitchDeferred(Combat))
		{
			SetTarget(World, Entity, Target, BestCandidate, Config);
		}
	}

	static void SetCombatApproachState(
		const FVector& EntityLocation,
		const FASMassMonsterConfigFragment& Config,
		const FASMassMonsterCombatFragment& Combat,
		const FASMassMonsterTargetFragment& Target,
		FASMassMonsterBehaviorFragment& Behavior)
	{
		const float DistanceSquared = FVector::DistSquared2D(EntityLocation, Target.CachedTargetLocation);
		// HitRadius는 근접 유닛의 중심점-대상 거리 보정용이다.
		// 원거리/돌진 유닛에 더하면 사거리보다 멀리서 Attack 상태가 되어 이동을 멈출 수 있다.
		const float EffectiveAttackRange = Combat.bUseFallbackMelee
			? Config.FallbackMeleeAttackRange
			: Config.AttackPattern == EASMassMonsterAttackPattern::Melee
				? Config.AttackRange + Config.HitRadius
				: Config.AttackRange;
		Behavior.State = DistanceSquared <= FMath::Square(EffectiveAttackRange)
			? EASMassMonsterState::Attack
			: EASMassMonsterState::Chase;
	}
}

UASMassMonsterBehaviorProcessor::UASMassMonsterBehaviorProcessor()
	: EntityQuery(*this)
{
	// 본맵과 기능 테스트 맵이 같은 행동 전환 경로를 쓰도록 자동 등록을 명시한다.
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	bRequiresGameThreadExecution = true;
}

void UASMassMonsterBehaviorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterCombatFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterHomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterBehaviorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterThreatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FASMassMonsterConfigFragment>(EMassFragmentPresence::All);
}

void UASMassMonsterBehaviorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassMonsterBehaviorProcessor);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassMonsterBehaviorProcessor);

	UWorld* World = GetWorld();
	UASMonsterTargetSubsystem* TargetSubsystem = World ? World->GetSubsystem<UASMonsterTargetSubsystem>() : nullptr;
	if (!TargetSubsystem)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const double CurrentTime = World->GetTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [World, TargetSubsystem, DeltaTime, CurrentTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FASMassMonsterHealthFragment> Healths = Context.GetMutableFragmentView<FASMassMonsterHealthFragment>();
		const TConstArrayView<FASMassMonsterCombatFragment> Combats = Context.GetFragmentView<FASMassMonsterCombatFragment>();
		const TConstArrayView<FASMassMonsterHomeFragment> Homes = Context.GetFragmentView<FASMassMonsterHomeFragment>();
		const TArrayView<FASMassMonsterBehaviorFragment> Behaviors = Context.GetMutableFragmentView<FASMassMonsterBehaviorFragment>();
		const TArrayView<FASMassMonsterTargetFragment> Targets = Context.GetMutableFragmentView<FASMassMonsterTargetFragment>();
		const TArrayView<FASMassMonsterThreatFragment> Threats = Context.GetMutableFragmentView<FASMassMonsterThreatFragment>();
		const FASMassMonsterConfigFragment& Config = Context.GetConstSharedFragment<FASMassMonsterConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterBehaviorFragment& Behavior = Behaviors[EntityIt];
			FASMassMonsterTargetFragment& Target = Targets[EntityIt];
			FASMassMonsterThreatFragment& Threat = Threats[EntityIt];
			const FASMassMonsterHomeFragment& Home = Homes[EntityIt];
			const FVector EntityLocation = Transforms[EntityIt].GetTransform().GetLocation();
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);

			if (Healths[EntityIt].CurrentHealth <= 0.0f)
			{
				Behavior.State = EASMassMonsterState::Dead;
				Target.Reset();
				Threat.Reset();
				continue;
			}

			Target.TargetLockTimeRemaining = FMath::Max(0.0f, Target.TargetLockTimeRemaining - DeltaTime);
			Target.RefreshTimeRemaining = FMath::Max(0.0f, Target.RefreshTimeRemaining - DeltaTime);
			if (!Target.bRefreshScheduleInitialized)
			{
				Target.bRefreshScheduleInitialized = true;
				Target.RefreshTimeRemaining = ASMassMonsterBehavior::MakeInitialRefreshDelay(Config.TargetRefreshInterval, Entity);
			}

			const float RefreshInterval = ASMassMonsterBehavior::MakeRefreshInterval(Config.TargetRefreshInterval, Entity);
			const bool bRefreshTarget = Target.RefreshTimeRemaining <= 0.0f;
			if (bRefreshTarget)
			{
				Target.RefreshTimeRemaining = RefreshInterval;
			}

			if (Behavior.WorldBehavior == EASMassMonsterWorldBehavior::NightAssault)
			{
				if (bRefreshTarget)
				{
					ASMassMonsterBehavior::RefreshPrimaryTarget(*World, Entity, *TargetSubsystem,
						EntityLocation, Target, Threat, Combats[EntityIt], Home, Config, true, CurrentTime);
				}

				if (!Target.HasTarget())
				{
					Behavior.State = EASMassMonsterState::Idle;
					continue;
				}

				ASMassMonsterBehavior::SetCombatApproachState(EntityLocation, Config, Combats[EntityIt], Target, Behavior);
				continue;
			}

			const float DistanceFromHomeSquared = FVector::DistSquared2D(EntityLocation, Home.HomeLocation);
			const bool bOutsideLeash = DistanceFromHomeSquared > FMath::Square(Home.LeashRadius);
			const bool bAtHome = DistanceFromHomeSquared <= FMath::Square(Home.ReturnAcceptanceRadius);

			if (Behavior.State == EASMassMonsterState::ReturnHome)
			{
				FASMassMonsterHealthFragment& Health = Healths[EntityIt];
				if (Home.ReturnHealthPercentPerSecond > 0.0f && Health.CurrentHealth < Config.MaxHealth)
				{
					Health.CurrentHealth = FMath::Min(Config.MaxHealth,
						Health.CurrentHealth + Config.MaxHealth * Home.ReturnHealthPercentPerSecond * DeltaTime);
				}

				if (bAtHome)
				{
					Behavior.State = EASMassMonsterState::Idle;
				}
				Target.Reset();
				Threat.Reset();
				Target.bRefreshScheduleInitialized = true;
				continue;
			}

			if (bOutsideLeash)
			{
				Behavior.State = EASMassMonsterState::ReturnHome;
				ASMassMonsterBehavior::ClearTarget(Target, RefreshInterval);
				Threat.Reset();
				continue;
			}

			if (bRefreshTarget)
			{
				ASMassMonsterBehavior::RefreshPrimaryTarget(*World, Entity, *TargetSubsystem,
					EntityLocation, Target, Threat, Combats[EntityIt], Home, Config, false, CurrentTime);
			}

			if (!Target.HasTarget())
			{
				Behavior.State = bAtHome ? EASMassMonsterState::Idle : EASMassMonsterState::ReturnHome;
				continue;
			}

			ASMassMonsterBehavior::SetCombatApproachState(EntityLocation, Config, Combats[EntityIt], Target, Behavior);
		}
	});
}
