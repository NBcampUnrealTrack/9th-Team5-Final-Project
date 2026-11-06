#include "Items/Data/ASItemDefinition.h"

float UASItemDefinition::GetEffectMagnitude(const FGameplayTag Tag, const float DefaultValue) const
{
	const float* Value = EffectMagnitudes.Find(Tag);
	return Value ? *Value : DefaultValue;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "GameplayEffect.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/GAS/ASItemUseAbility.h"
#include "Items/GAS/ASGrenadeUseAbility.h"
#include "Items/GAS/ASBarricadeUseAbility.h"
#include "Items/Data/ASItemTags.h"

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
	if (!IsValidLootRarity(Rarity)) Error(TEXT("Rarity must be Uncommon, Rare, Epic, or Legendary."));
	if (MaxStackSize < 1) Error(TEXT("MaxStackSize must be positive."));
	if (ItemType == EASItemType::Bag)
	{
		if (MaxStackSize != 1) Error(TEXT("Bags must have MaxStackSize=1."));
		if (BagAdditionalSlots < 0 || BagAdditionalSlots > 256) Error(TEXT("BagAdditionalSlots must be between 0 and 256."));
	}
	if (ItemType == EASItemType::Weapon)
	{
		if (MaxStackSize != 1) Error(TEXT("Weapons must have MaxStackSize=1 to keep equipped instances independent."));
		if (WeaponType == EASWeaponType::None) Error(TEXT("WeaponType must be Melee or Ranged."));
		if (!IsValid(AttackProfile.LoadSynchronous()))
		{
			Error(TEXT("Weapons require an AttackProfile."));
		}
		else if (AttackProfile.LoadSynchronous()->ValidateForItem(*this, Context) == EDataValidationResult::Invalid)
		{
			bValid = false;
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
		const UASItemUseAbility* AbilityDefaults = GetDefault<UASItemUseAbility>();
		if (!UseAbility.IsNull())
		{
			UClass* AbilityClass = UseAbility.LoadSynchronous();
			if (!AbilityClass || !AbilityClass->IsChildOf(UASItemUseAbility::StaticClass())
				|| AbilityClass->HasAnyClassFlags(CLASS_Abstract))
			{
				Error(TEXT("UseAbility must be a non-abstract subclass of ASItemUseAbility."));
				AbilityDefaults = nullptr;
			}
			else
			{
				AbilityDefaults = AbilityClass->GetDefaultObject<UASItemUseAbility>();
			}
		}
		if (AbilityDefaults && AbilityDefaults->RequiresUseEffects() && UseEffects.IsEmpty())
			Error(TEXT("The selected UseAbility requires at least one UseEffect. Custom actions must override RequiresUseEffects to return false."));
		if (AbilityDefaults && AbilityDefaults->IsA<UASGrenadeUseAbility>())
		{
			const float ThrowRange = GetEffectMagnitude(ASGameplayTags::Data_Item_ThrowRange, ASItemMagnitudeDefaults::ThrowRange);
			const float ExplosionRadius = GetEffectMagnitude(ASGameplayTags::Data_Item_ExplosionRadius, ASItemMagnitudeDefaults::ExplosionRadius);
			if (!FMath::IsFinite(ThrowRange) || ThrowRange <= 0.0f)
				Error(TEXT("EffectMagnitudes: Data.Item.ThrowRange must be positive and finite (cm)."));
			if (!FMath::IsFinite(ExplosionRadius) || ExplosionRadius <= 0.0f)
				Error(TEXT("EffectMagnitudes: Data.Item.ExplosionRadius must be positive and finite (cm)."));
		}
		if (AbilityDefaults && AbilityDefaults->IsA<UASBarricadeUseAbility>())
		{
			const float Range = GetEffectMagnitude(ASGameplayTags::Data_Item_PlacementRange, ASItemMagnitudeDefaults::PlacementRange);
			const float Health = GetEffectMagnitude(ASGameplayTags::Data_Item_BarricadeHealth, ASItemMagnitudeDefaults::BarricadeHealth);
			const float Lifetime = GetEffectMagnitude(ASGameplayTags::Data_Item_BarricadeLifetime, ASItemMagnitudeDefaults::BarricadeLifetime);
			if (!FMath::IsFinite(Range) || Range <= 0.0f)
				Error(TEXT("EffectMagnitudes: Data.Item.PlacementRange must be positive and finite (cm)."));
			if (!FMath::IsFinite(Health) || Health <= 0.0f)
				Error(TEXT("EffectMagnitudes: Data.Item.BarricadeHealth must be positive and finite."));
			if (!FMath::IsFinite(Lifetime) || Lifetime < 0.0f)
				Error(TEXT("EffectMagnitudes: Data.Item.BarricadeLifetime must be zero or positive (seconds)."));
		}
		for (const auto& Effect : UseEffects)
		{
			if (!Effect.LoadSynchronous()) Error(TEXT("UseEffects contains an empty or missing class."));
		}
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
