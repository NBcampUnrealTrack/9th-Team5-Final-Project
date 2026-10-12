#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/Data/ASItemTypes.h"
#include "ASItemDefinition.generated.h"

class AActor;
class UGameplayAbility;
class UGameplayEffect;
class UStaticMesh;
class UTexture2D;
class FDataValidationContext;

/**
 * Static, designer-authored data shared by every instance of an item.
 * Quantity belongs to the inventory; equipped state belongs to the equipment component.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	/** Stable identifier. Do not derive gameplay logic from the asset name or path. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EASItemType ItemType = EASItemType::Weapon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxStackSize = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Tags", meta = (Categories = "Item"))
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	EASWeaponType WeaponType = EASWeaponType::None;

	/** Actor spawned and attached when this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	TSoftClassPtr<AActor> EquippedActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	FName EquippedSocketName = NAME_None;

	/** Normal attack supplied by this weapon. Equipment binds it to InputTag.Attack.Primary. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon|Attack",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	TSoftClassPtr<UGameplayAbility> PrimaryAttackAbility;

	/** Must derive from ASItemUseAbility; CommitAbility consumes UseQuantity. Null uses the default data-driven ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable|GAS",
		meta = (EditCondition = "ItemType == EASItemType::Consumable", EditConditionHides))
	TSoftClassPtr<UGameplayAbility> UseAbility;

	/** Units consumed by one successful CommitAbility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable|GAS",
		meta = (ClampMin = "1", EditCondition = "ItemType == EASItemType::Consumable", EditConditionHides))
	int32 UseQuantity = 1;

	/** Self effects used by the default consumable ability. At least one effect is required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable|GAS",
		meta = (EditCondition = "ItemType == EASItemType::Consumable", EditConditionHides))
	TArray<TSoftClassPtr<UGameplayEffect>> UseEffects;

	/** SetByCaller values shared by all UseEffects. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable|GAS",
		meta = (EditCondition = "ItemType == EASItemType::Consumable", EditConditionHides, Categories = "Data"))
	TMap<FGameplayTag, float> EffectMagnitudes;

	/** At least one listed resource must be below maximum. Set explicitly for recovery GEs.
	 * Empty disables resource checks.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable|GAS",
		meta = (EditCondition = "ItemType == EASItemType::Consumable", EditConditionHides))
	TArray<EASItemRecoveryResource> RecoveryResources;

	/** Base damage supplied to the equipped weapon ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon",
		meta = (ClampMin = "0.0", EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	float BaseDamage = 0.0f;

	/** Minimum time between normal attacks, in seconds. The attack ability enforces this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon|Attack",
		meta = (ClampMin = "0.01", Units = "s", EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	float AttackInterval = 0.5f;

	/** Normal attack reach in cm: melee reach or maximum bullet travel distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon|Attack",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	float AttackRange = 200.0f;

	/** Damage effect used by the attack ability. Existing skills supply negative Data.Damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon|Effects",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon|Ammo",
		meta = (EditCondition = "ItemType == EASItemType::Weapon && WeaponType == EASWeaponType::Ranged", EditConditionHides))
	bool bUsesAmmo = false;

	/** Inventory item consumed by this weapon. Select an item whose ItemType is Ammo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon|Ammo",
		meta = (EditCondition = "ItemType == EASItemType::Weapon && WeaponType == EASWeaponType::Ranged && bUsesAmmo", EditConditionHides))
	TObjectPtr<UASItemDefinition> AmmoDefinition;

	/** Units consumed per successful shot; charged by the server's firing ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon|Ammo",
		meta = (ClampMin = "1", EditCondition = "ItemType == EASItemType::Weapon && WeaponType == EASWeaponType::Ranged && bUsesAmmo", EditConditionHides))
	int32 AmmoPerShot = 1;
};
