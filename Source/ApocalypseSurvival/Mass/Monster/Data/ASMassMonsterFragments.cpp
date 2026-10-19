#include "Mass/Monster/Data/ASMassMonsterFragments.h"

void FASMassMonsterConfigFragment::Validate()
{
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	MoveSpeed = FMath::Max(0.0f, MoveSpeed);
	HitRadius = FMath::Max(0.0f, HitRadius);
	AttackPower = FMath::Max(0.0f, AttackPower);
	AttackRange = FMath::Max(0.0f, AttackRange);
	AttackCooldown = FMath::Max(0.01f, AttackCooldown);
	RangedProjectileSpeed = FMath::Max(1.0f, RangedProjectileSpeed);
	RangedProjectileMaxRange = FMath::Max(AttackRange, RangedProjectileMaxRange);
	RangedProjectileSpawnHeight = FMath::Max(0.0f, RangedProjectileSpawnHeight);
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

