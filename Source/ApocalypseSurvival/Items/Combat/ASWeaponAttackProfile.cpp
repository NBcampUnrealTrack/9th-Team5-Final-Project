#include "Items/Combat/ASWeaponAttackProfile.h"

#include "Items/Combat/ASWeaponAttackExecution.h"
#include "Skills/ASSkillTags.h"

#if WITH_EDITOR
#include "Items/Combat/ASRifleBurstSequence.h"
#include "Items/Combat/ASShotgunAttackSequence.h"
#include "Items/Combat/ASSniperPiercingProjectile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Misc/DataValidation.h"
#endif

UASMeleeAttackProfile::UASMeleeAttackProfile()
{
	ExecutionClass = UASMeleeAttackExecution::StaticClass();
	TargetMode = EASAttackTargetMode::Area;
	ImpactPreset = EASImpactPreset::Standard;
	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}

UASRangedAttackProfile::UASRangedAttackProfile()
{
	ExecutionClass = UASRangedAttackExecution::StaticClass();
	TargetMode = EASAttackTargetMode::Single;
	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}

UASRifleBurstAttackProfile::UASRifleBurstAttackProfile()
{
	ExecutionClass = UASRifleBurstAttackExecution::StaticClass();
	TargetMode = EASAttackTargetMode::Single;
	ImpactPreset = EASImpactPreset::Rapid;
	AmmoPerShot = 3;
	ProjectilePierceCount = 0;
}

UASShotgunAttackProfile::UASShotgunAttackProfile()
{
	ExecutionClass = UASShotgunAttackExecution::StaticClass();
	TargetMode = EASAttackTargetMode::Single;
	ImpactPreset = EASImpactPreset::None;
	AmmoPerShot = 1;
	ProjectilePierceCount = 0;
}

UASSniperAttackProfile::UASSniperAttackProfile()
{
	ExecutionClass = UASSniperAttackExecution::StaticClass();
	TargetMode = EASAttackTargetMode::Single;
	ImpactPreset = EASImpactPreset::Heavy;
	AmmoPerShot = 1;
	ProjectilePierceCount = 0;
}

#if WITH_EDITOR
namespace ASWeaponAttackProfileValidation
{
	void AddError(FDataValidationContext& Context, bool& bValid, const TCHAR* Message)
	{
		Context.AddError(FText::FromString(Message));
		bValid = false;
	}

	EDataValidationResult Finish(const EDataValidationResult ParentResult, const bool bLocalValid)
	{
		return ParentResult == EDataValidationResult::Invalid || !bLocalValid
			? EDataValidationResult::Invalid
			: EDataValidationResult::Valid;
	}
}

EDataValidationResult UASWeaponAttackProfile::ValidateForItem(
	const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const
{
	bool bValid = true;
	auto Error = [&Context, &bValid](const TCHAR* Message)
	{
		ASWeaponAttackProfileValidation::AddError(Context, bValid, Message);
	};

	if (!ExecutionClass.LoadSynchronous())
	{
		Error(TEXT("AttackProfile requires an ExecutionClass."));
	}
	if (!SupportsWeaponType(ItemDefinition.WeaponType))
	{
		Error(TEXT("AttackProfile does not support the ItemDefinition WeaponType."));
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

EDataValidationResult UASMeleeAttackProfile::ValidateForItem(
	const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::ValidateForItem(ItemDefinition, Context);
	bool bValid = true;
	auto Error = [&Context, &bValid](const TCHAR* Message)
	{
		ASWeaponAttackProfileValidation::AddError(Context, bValid, Message);
	};

	if (ExecutionClass.LoadSynchronous() && !ExecutionClass.LoadSynchronous()->IsChildOf(UASMeleeAttackExecution::StaticClass()))
	{
		Error(TEXT("MeleeAttackProfile ExecutionClass must derive from UASMeleeAttackExecution."));
	}
	if (!FMath::IsFinite(StaminaCost) || StaminaCost <= 0.0f)
	{
		Error(TEXT("MeleeAttackProfile requires a positive finite StaminaCost."));
	}
	if (!StaminaCostEffect.LoadSynchronous())
	{
		Error(TEXT("MeleeAttackProfile requires a StaminaCostEffect."));
	}
	if (!FMath::IsFinite(HitAngle) || HitAngle <= 0.0f || HitAngle > 360.0f)
	{
		Error(TEXT("MeleeAttackProfile HitAngle must be in (0, 360]."));
	}
	return ASWeaponAttackProfileValidation::Finish(ParentResult, bValid);
}

EDataValidationResult UASRangedAttackProfile::ValidateForItem(
	const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::ValidateForItem(ItemDefinition, Context);
	bool bValid = true;
	auto Error = [&Context, &bValid](const TCHAR* Message)
	{
		ASWeaponAttackProfileValidation::AddError(Context, bValid, Message);
	};

	if (ExecutionClass.LoadSynchronous() && !ExecutionClass.LoadSynchronous()->IsChildOf(UASRangedAttackExecution::StaticClass()))
	{
		Error(TEXT("RangedAttackProfile ExecutionClass must derive from UASRangedAttackExecution."));
	}
	if (!IsValid(AmmoDefinition.LoadSynchronous()) || AmmoDefinition.LoadSynchronous() == &ItemDefinition
		|| AmmoDefinition.LoadSynchronous()->ItemType != EASItemType::Ammo)
	{
		Error(TEXT("RangedAttackProfile requires an Ammo item definition."));
	}
	if (AmmoPerShot < 1)
	{
		Error(TEXT("RangedAttackProfile AmmoPerShot must be positive."));
	}
	if (!FMath::IsFinite(TraceRadius) || TraceRadius < 0.0f
		|| !FMath::IsFinite(AreaRadius) || AreaRadius < 0.0f)
	{
		Error(TEXT("RangedAttackProfile trace values are invalid."));
	}
	if (TargetMode == EASAttackTargetMode::Area && AreaRadius <= 0.0f)
	{
		Error(TEXT("Area ranged attacks require a positive AreaRadius."));
	}
	if (!bUseAttackMoveTargeting)
	{
		if (!ProjectileClass.LoadSynchronous() && !DirectionalProjectileClass.LoadSynchronous())
		{
			Error(TEXT("Directional projectile attacks require a ProjectileClass or DirectionalProjectileClass."));
		}
		if (!FMath::IsFinite(ProjectileSpeed) || ProjectileSpeed <= 0.0f)
		{
			Error(TEXT("Directional projectile attacks require a positive finite ProjectileSpeed."));
		}
	}
	if (DirectionalProjectileClass.LoadSynchronous() && (!FMath::IsFinite(ProjectileSpeed) || ProjectileSpeed <= 0.0f))
	{
		Error(TEXT("DirectionalProjectileClass requires a positive finite ProjectileSpeed."));
	}
	return ASWeaponAttackProfileValidation::Finish(ParentResult, bValid);
}

EDataValidationResult UASRifleBurstAttackProfile::ValidateForItem(
	const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::ValidateForItem(ItemDefinition, Context);
	bool bValid = true;
	auto Error = [&Context, &bValid](const TCHAR* Message)
	{
		ASWeaponAttackProfileValidation::AddError(Context, bValid, Message);
	};

	if (ExecutionClass.LoadSynchronous() && !ExecutionClass.LoadSynchronous()->IsChildOf(UASRifleBurstAttackExecution::StaticClass()))
	{
		Error(TEXT("RifleBurstAttackProfile ExecutionClass must derive from UASRifleBurstAttackExecution."));
	}
	if (BurstCount < 2)
	{
		Error(TEXT("RifleBurstAttackProfile BurstCount must be at least 2."));
	}
	if (!FMath::IsFinite(BurstShotInterval) || BurstShotInterval < 0.01f)
	{
		Error(TEXT("RifleBurstAttackProfile BurstShotInterval must be finite and at least 0.01 seconds."));
	}
	if (FocusedHitThreshold < 2 || FocusedHitThreshold > BurstCount)
	{
		Error(TEXT("RifleBurstAttackProfile FocusedHitThreshold must be between 2 and BurstCount."));
	}
	if (AmmoPerShot != BurstCount)
	{
		Error(TEXT("RifleBurstAttackProfile AmmoPerShot must equal BurstCount."));
	}
	if (ProjectilePierceCount != 0)
	{
		Error(TEXT("RifleBurstAttackProfile projectiles must not pierce."));
	}
	if (DirectionalProjectileClass.LoadSynchronous()
		&& !DirectionalProjectileClass.LoadSynchronous()->IsChildOf(AASRifleBurstDirectionalProjectile::StaticClass()))
	{
		Error(TEXT("Rifle DirectionalProjectileClass must derive from AASRifleBurstDirectionalProjectile."));
	}
	if (HomingProjectileClass.LoadSynchronous()
		&& !HomingProjectileClass.LoadSynchronous()->IsChildOf(AASRifleBurstHomingProjectile::StaticClass()))
	{
		Error(TEXT("Rifle HomingProjectileClass must derive from AASRifleBurstHomingProjectile."));
	}
	return ASWeaponAttackProfileValidation::Finish(ParentResult, bValid);
}

EDataValidationResult UASShotgunAttackProfile::ValidateForItem(
	const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::ValidateForItem(ItemDefinition, Context);
	bool bValid = true;
	auto Error = [&Context, &bValid](const TCHAR* Message)
	{
		ASWeaponAttackProfileValidation::AddError(Context, bValid, Message);
	};

	if (ExecutionClass.LoadSynchronous() && !ExecutionClass.LoadSynchronous()->IsChildOf(UASShotgunAttackExecution::StaticClass()))
	{
		Error(TEXT("ShotgunAttackProfile ExecutionClass must derive from UASShotgunAttackExecution."));
	}
	if (PelletCount < 1)
	{
		Error(TEXT("ShotgunAttackProfile PelletCount must be positive."));
	}
	if (!FMath::IsFinite(SpreadAngleDegrees) || SpreadAngleDegrees <= 0.0f || SpreadAngleDegrees > 180.0f)
	{
		Error(TEXT("ShotgunAttackProfile SpreadAngleDegrees must be in (0, 180]."));
	}
	if (!FMath::IsFinite(KnockbackDistance) || KnockbackDistance < 0.0f)
	{
		Error(TEXT("ShotgunAttackProfile KnockbackDistance must be finite and nonnegative."));
	}
	if (!PelletProjectileClass.LoadSynchronous()
		|| !PelletProjectileClass.LoadSynchronous()->IsChildOf(AASShotgunPelletProjectile::StaticClass()))
	{
		Error(TEXT("ShotgunAttackProfile PelletProjectileClass must derive from AASShotgunPelletProjectile."));
	}
	if (ProjectilePierceCount != 0)
	{
		Error(TEXT("ShotgunAttackProfile pellets must not pierce."));
	}
	return ASWeaponAttackProfileValidation::Finish(ParentResult, bValid);
}

EDataValidationResult UASSniperAttackProfile::ValidateForItem(
	const UASItemDefinition& ItemDefinition, FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::ValidateForItem(ItemDefinition, Context);
	bool bValid = true;
	auto Error = [&Context, &bValid](const TCHAR* Message)
	{
		ASWeaponAttackProfileValidation::AddError(Context, bValid, Message);
	};

	if (ExecutionClass.LoadSynchronous() && !ExecutionClass.LoadSynchronous()->IsChildOf(UASSniperAttackExecution::StaticClass()))
	{
		Error(TEXT("SniperAttackProfile ExecutionClass must derive from UASSniperAttackExecution."));
	}
	if (MaxPierceTargets < 1)
	{
		Error(TEXT("SniperAttackProfile MaxPierceTargets must be positive."));
	}
	if (PierceDamageMultipliers.Num() < MaxPierceTargets)
	{
		Error(TEXT("SniperAttackProfile requires one PierceDamageMultiplier per target."));
	}
	else
	{
		float PreviousMultiplier = 1.0f;
		for (int32 Index = 0; Index < MaxPierceTargets; ++Index)
		{
			const float Multiplier = PierceDamageMultipliers[Index];
			if (!FMath::IsFinite(Multiplier) || Multiplier < 0.0f || Multiplier > 1.0f
				|| (Index == 0 && !FMath::IsNearlyEqual(Multiplier, 1.0f))
				|| (Index > 0 && Multiplier > PreviousMultiplier))
			{
				Error(TEXT("SniperAttackProfile PierceDamageMultipliers must start at 1 and be nonincreasing within [0, 1]."));
				break;
			}
			PreviousMultiplier = Multiplier;
		}
	}
	if (!SniperProjectileClass.LoadSynchronous()
		|| !SniperProjectileClass.LoadSynchronous()->IsChildOf(AASSniperPiercingProjectile::StaticClass()))
	{
		Error(TEXT("SniperAttackProfile SniperProjectileClass must derive from AASSniperPiercingProjectile."));
	}
	return ASWeaponAttackProfileValidation::Finish(ParentResult, bValid);
}
#endif
