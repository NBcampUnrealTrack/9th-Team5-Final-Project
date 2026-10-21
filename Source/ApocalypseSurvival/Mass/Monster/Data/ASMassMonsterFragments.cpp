#include "Mass/Monster/Data/ASMassMonsterFragments.h"

void FASMassMonsterConfigFragment::Validate()
{
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
	TargetRefreshInterval = FMath::Max(0.01f, TargetRefreshInterval);
	PathRefreshInterval = FMath::Max(0.01f, PathRefreshInterval);
	TargetMoveRepathDistance = FMath::Max(0.0f, TargetMoveRepathDistance);
	DeathDespawnDelay = FMath::Max(0.0f, DeathDespawnDelay);
}

