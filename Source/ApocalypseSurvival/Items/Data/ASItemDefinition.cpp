#include "Items/Data/ASItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "GameplayEffect.h"
#include "Items/Combat/ASWeaponAttackExecution.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/GAS/ASItemUseAbility.h"

EDataValidationResult UASItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;
	auto Error = [&Context, &bValid](const TCHAR* Message)
	{
		Context.AddError(FText::FromString(Message));
		bValid = false;
	};
	if (ItemId.IsNone()) Error(TEXT("ItemId must be set."));
	if (MaxStackSize < 1) Error(TEXT("MaxStackSize must be positive."));
	if (ItemType == EASItemType::Weapon)
	{
		if (MaxStackSize != 1) Error(TEXT("Weapons must have MaxStackSize=1 to keep equipped instances independent."));
		if (WeaponType == EASWeaponType::None) Error(TEXT("WeaponType must be Melee or Ranged."));
		if (!IsValid(AttackProfile))
		{
			Error(TEXT("Weapons require an AttackProfile."));
		}
		else if (!AttackProfile->ExecutionClass)
		{
			Error(TEXT("AttackProfile requires an ExecutionClass."));
		}
		else if (WeaponType == EASWeaponType::Melee && Cast<UASRangedAttackProfile>(AttackProfile))
		{
			Error(TEXT("Melee weapons cannot use a RangedAttackProfile."));
		}
		else if (WeaponType == EASWeaponType::Ranged && Cast<UASMeleeAttackProfile>(AttackProfile))
		{
			Error(TEXT("Ranged weapons cannot use a MeleeAttackProfile."));
		}
		if (const UASMeleeAttackProfile* MeleeProfile = Cast<UASMeleeAttackProfile>(AttackProfile))
		{
			if (MeleeProfile->ExecutionClass
				&& !MeleeProfile->ExecutionClass->IsChildOf(UASMeleeAttackExecution::StaticClass()))
			{
				Error(TEXT("MeleeAttackProfile ExecutionClass must derive from UASMeleeAttackExecution."));
			}
			if (!FMath::IsFinite(MeleeProfile->StaminaCost) || MeleeProfile->StaminaCost <= 0.0f)
			{
				Error(TEXT("MeleeAttackProfile requires a positive finite StaminaCost."));
			}
			if (!MeleeProfile->StaminaCostEffect)
			{
				Error(TEXT("MeleeAttackProfile requires a StaminaCostEffect."));
			}
			if (!FMath::IsFinite(MeleeProfile->HitRadius) || MeleeProfile->HitRadius < 0.0f
				|| !FMath::IsFinite(MeleeProfile->HitboxOffset) || MeleeProfile->HitboxOffset < 0.0f)
			{
				Error(TEXT("MeleeAttackProfile hit dimensions must be finite and nonnegative."));
			}
		}
		if (const UASRangedAttackProfile* RangedProfile = Cast<UASRangedAttackProfile>(AttackProfile))
		{
			if (RangedProfile->ExecutionClass
				&& !RangedProfile->ExecutionClass->IsChildOf(UASRangedAttackExecution::StaticClass()))
			{
				Error(TEXT("RangedAttackProfile ExecutionClass must derive from UASRangedAttackExecution."));
			}
			if (!IsValid(RangedProfile->AmmoDefinition)
				|| RangedProfile->AmmoDefinition == this
				|| RangedProfile->AmmoDefinition->ItemType != EASItemType::Ammo)
			{
				Error(TEXT("RangedAttackProfile requires an Ammo item definition."));
			}
			if (RangedProfile->AmmoPerShot < 1)
			{
				Error(TEXT("RangedAttackProfile AmmoPerShot must be positive."));
			}
			if (!FMath::IsFinite(RangedProfile->TraceRadius) || RangedProfile->TraceRadius < 0.0f
				|| !FMath::IsFinite(RangedProfile->AreaRadius) || RangedProfile->AreaRadius < 0.0f)
			{
				Error(TEXT("RangedAttackProfile trace values are invalid."));
			}
			if (RangedProfile->TargetMode == EASAttackTargetMode::Area && RangedProfile->AreaRadius <= 0.0f)
			{
				Error(TEXT("Area ranged attacks require a positive AreaRadius."));
			}
			if (!RangedProfile->bUseAttackMoveTargeting)
			{
				if (!RangedProfile->ProjectileClass)
				{
					Error(TEXT("Directional projectile attacks require a ProjectileClass."));
				}
				if (!FMath::IsFinite(RangedProfile->ProjectileSpeed) || RangedProfile->ProjectileSpeed <= 0.0f)
				{
					Error(TEXT("Directional projectile attacks require a positive finite ProjectileSpeed."));
				}
			}
		}
		if (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.0f) Error(TEXT("BaseDamage must be finite and nonnegative."));
		if (!FMath::IsFinite(AttackInterval) || AttackInterval < 0.01f) Error(TEXT("AttackInterval must be finite and at least 0.01 seconds."));
		if (!FMath::IsFinite(AttackRange) || AttackRange < 0.0f) Error(TEXT("AttackRange must be finite and nonnegative."));
		if (!DamageEffectClass.IsNull() && !DamageEffectClass.LoadSynchronous()) Error(TEXT("DamageEffectClass cannot be loaded."));
		if (!EquippedActorClass.IsNull() && !EquippedActorClass.LoadSynchronous()) Error(TEXT("EquippedActorClass cannot be loaded."));
	}
	if (ItemType == EASItemType::Consumable)
	{
		if (UseQuantity < 1 || UseQuantity > MaxStackSize) Error(TEXT("UseQuantity must be between 1 and MaxStackSize."));
		for (const auto& Magnitude : EffectMagnitudes)
		{
			if (!Magnitude.Key.IsValid()) Error(TEXT("EffectMagnitudes requires valid gameplay tags."));
			if (!FMath::IsFinite(Magnitude.Value)) Error(TEXT("EffectMagnitudes values must be finite."));
		}
		if (!UseAbility.IsNull())
		{
			UClass* AbilityClass = UseAbility.LoadSynchronous();
			if (!AbilityClass || !AbilityClass->IsChildOf(UASItemUseAbility::StaticClass())) Error(TEXT("UseAbility must derive from ASItemUseAbility."));
		}
		else if (UseEffects.IsEmpty())
			Error(TEXT("Default consumables require at least one UseEffect."));
		for (const auto& Effect : UseEffects)
		{
			if (!Effect.LoadSynchronous()) Error(TEXT("UseEffects contains an empty or missing class."));
		}
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
