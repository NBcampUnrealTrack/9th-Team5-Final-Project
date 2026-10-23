#include "Mass/Monster/Data/ASMassMonsterFragments.h"

float FASMassMonsterThreatFragment::GetEffectiveThreat(
	const FASTargetHandle Target,
	const double CurrentTimeSeconds,
	const float DecayPerSecond) const
{
	if (!Target.IsValid())
	{
		return 0.0f;
	}

	for (const FASMassMonsterThreatEntry& Entry : Entries)
	{
		if (Entry.Target != Target)
		{
			continue;
		}

		const double ElapsedSeconds = FMath::Max(0.0, CurrentTimeSeconds - Entry.LastUpdateTimeSeconds);
		return FMath::Max(0.0f, Entry.StoredThreat - FMath::Max(0.0f, DecayPerSecond) * ElapsedSeconds);
	}

	return 0.0f;
}

void FASMassMonsterThreatFragment::AddThreat(
	const FASTargetHandle Target,
	const float ThreatAmount,
	const double CurrentTimeSeconds,
	const float DecayPerSecond)
{
	if (!Target.IsValid() || ThreatAmount <= 0.0f)
	{
		return;
	}

	FASMassMonsterThreatEntry* Destination = nullptr;
	for (FASMassMonsterThreatEntry& Entry : Entries)
	{
		if (Entry.Target == Target)
		{
			Destination = &Entry;
			break;
		}
		if (!Destination && !Entry.Target.IsValid())
		{
			Destination = &Entry;
		}
	}

	if (!Destination)
	{
		Destination = &Entries[0];
		float LowestThreat = GetEffectiveThreat(Destination->Target, CurrentTimeSeconds, DecayPerSecond);
		for (FASMassMonsterThreatEntry& Entry : Entries)
		{
			const float EffectiveThreat = GetEffectiveThreat(Entry.Target, CurrentTimeSeconds, DecayPerSecond);
			if (EffectiveThreat < LowestThreat)
			{
				Destination = &Entry;
				LowestThreat = EffectiveThreat;
			}
		}
	}

	const float ExistingThreat = Destination->Target == Target
		? GetEffectiveThreat(Target, CurrentTimeSeconds, DecayPerSecond)
		: 0.0f;
	Destination->Target = Target;
	Destination->StoredThreat = FMath::Max(ExistingThreat, ThreatAmount);
	Destination->LastUpdateTimeSeconds = CurrentTimeSeconds;
}

void FASMassMonsterThreatFragment::Reset()
{
	for (FASMassMonsterThreatEntry& Entry : Entries)
	{
		Entry = FASMassMonsterThreatEntry();
	}
}

void FASMassMonsterConfigFragment::Validate()
{
	ExperienceReward = FMath::Max(0, ExperienceReward);
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	MoveSpeed = FMath::Max(0.0f, MoveSpeed);
	HitRadius = FMath::Max(0.0f, HitRadius);
	AttackPower = FMath::Max(0.0f, AttackPower);
	AttackRange = FMath::Max(0.0f, AttackRange);
	AttackCooldown = FMath::Max(0.01f, AttackCooldown);
	FallbackMeleeAttackRange = FMath::Max(0.0f, FallbackMeleeAttackRange);
	FallbackMeleeAttackPower = FMath::Max(0.0f, FallbackMeleeAttackPower);
	FallbackMeleeAttackCooldown = FMath::Max(0.01f, FallbackMeleeAttackCooldown);
	RangedProjectileSpeed = FMath::Max(1.0f, RangedProjectileSpeed);
	RangedProjectileMaxRange = FMath::Max(AttackRange, RangedProjectileMaxRange);
	RangedProjectileSpawnHeight = FMath::Max(0.0f, RangedProjectileSpawnHeight);
	ChargeWindupDuration = FMath::Max(0.0f, ChargeWindupDuration);
	ChargeDistance = FMath::Max(1.0f, ChargeDistance);
	ChargeDuration = FMath::Max(0.01f, ChargeDuration);
	ChargeHitRadius = FMath::Max(1.0f, ChargeHitRadius);
	SuicideWindupDuration = FMath::Max(0.0f, SuicideWindupDuration);
	SuicideRadius = FMath::Max(1.0f, SuicideRadius);
	SummonCount = FMath::Max(1, SummonCount);
	MaxSummonActivations = FMath::Max(1, MaxSummonActivations);
	SummonMinRadius = FMath::Max(0.0f, SummonMinRadius);
	SummonMaxRadius = FMath::Max(SummonMinRadius, SummonMaxRadius);
	SummonWindupDuration = FMath::Max(0.0f, SummonWindupDuration);
	HasteRadius = FMath::Max(1.0f, HasteRadius);
	HasteMoveSpeedMultiplier = FMath::Max(1.0f, HasteMoveSpeedMultiplier);
	HasteDuration = FMath::Max(0.0f, HasteDuration);
	HasteWindupDuration = FMath::Max(0.0f, HasteWindupDuration);
	DetectionRadius = FMath::Max(0.0f, DetectionRadius);
	LoseTargetRadius = FMath::Max(DetectionRadius, LoseTargetRadius);
	AssistAggroRadius = FMath::Max(0.0f, AssistAggroRadius);
	DirectHitAggro = FMath::Max(0.0f, DirectHitAggro);
	AggroDecayPerSecond = FMath::Max(0.0f, AggroDecayPerSecond);
	ProximityScoreMax = FMath::Max(0.0f, ProximityScoreMax);
	ProximityHalfScoreDistance = FMath::Max(1.0f, ProximityHalfScoreDistance);
	TargetSwitchScoreMargin = FMath::Max(0.0f, TargetSwitchScoreMargin);
	MinTargetLockSeconds = FMath::Max(0.0f, MinTargetLockSeconds);
	TargetRefreshInterval = FMath::Max(0.01f, TargetRefreshInterval);
	PathRefreshInterval = FMath::Max(0.01f, PathRefreshInterval);
	TargetMoveRepathDistance = FMath::Max(0.0f, TargetMoveRepathDistance);
	BlockingTargetSearchRadius = FMath::Max(0.0f, BlockingTargetSearchRadius);
	BlockingTargetMinForwardDot = FMath::Clamp(BlockingTargetMinForwardDot, -1.0f, 1.0f);
	BlockingTargetStuckSeconds = FMath::Max(0.0f, BlockingTargetStuckSeconds);
	BlockingTargetMinProgressDistance = FMath::Max(1.0f, BlockingTargetMinProgressDistance);
	DeathDespawnDelay = FMath::Max(0.0f, DeathDespawnDelay);
}

