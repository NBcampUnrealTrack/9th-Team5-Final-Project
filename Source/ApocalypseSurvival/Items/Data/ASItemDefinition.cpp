#include "Items/Data/ASItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "GameplayEffect.h"
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
		if (PrimaryAttackAbility.IsNull()) Context.AddWarning(FText::FromString(TEXT("PrimaryAttackAbility is empty: equipping will not supply a normal attack.")));
		else if (!PrimaryAttackAbility.LoadSynchronous()) Error(TEXT("PrimaryAttackAbility cannot be loaded."));
		if (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.0f) Error(TEXT("BaseDamage must be finite and nonnegative."));
		if (!FMath::IsFinite(AttackInterval) || AttackInterval < 0.01f) Error(TEXT("AttackInterval must be finite and at least 0.01 seconds."));
		if (!FMath::IsFinite(AttackRange) || AttackRange < 0.0f) Error(TEXT("AttackRange must be finite and nonnegative."));
		if (!DamageEffectClass.IsNull() && !DamageEffectClass.LoadSynchronous()) Error(TEXT("DamageEffectClass cannot be loaded."));
		if (!EquippedActorClass.IsNull() && !EquippedActorClass.LoadSynchronous()) Error(TEXT("EquippedActorClass cannot be loaded."));
		if (bUsesAmmo)
		{
			if (WeaponType != EASWeaponType::Ranged) Error(TEXT("Only ranged weapons can use ammo."));
			if (!IsValid(AmmoDefinition) || AmmoDefinition == this || AmmoDefinition->ItemType != EASItemType::Ammo) Error(TEXT("AmmoDefinition must reference an Ammo item."));
			if (AmmoPerShot < 1) Error(TEXT("AmmoPerShot must be positive."));
		}
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
