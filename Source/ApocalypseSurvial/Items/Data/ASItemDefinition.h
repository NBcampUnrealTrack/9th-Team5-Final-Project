#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/Data/ASItemTypes.h"
#include "ASItemDefinition.generated.h"

class AActor;
class UGameplayAbility;
class UStaticMesh;
class UTexture2D;

/**
 * Static, designer-authored data shared by every instance of an item.
 * Runtime state such as quantity or equipped slot belongs to the inventory.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIAL_API UASItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Tags")
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	EASWeaponType WeaponType = EASWeaponType::None;

	/** Actor spawned and attached when this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	TSoftClassPtr<AActor> EquippedActorClass;

	/** Abilities granted while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|GAS",
		meta = (EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	TArray<TSoftClassPtr<UGameplayAbility>> GrantedAbilities;

	/** Ability activated when a consumable item is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|GAS",
		meta = (EditCondition = "ItemType == EASItemType::Healing", EditConditionHides))
	TSoftClassPtr<UGameplayAbility> UseAbility;

	/** Base damage supplied to the equipped weapon ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Weapon",
		meta = (ClampMin = "0.0", EditCondition = "ItemType == EASItemType::Weapon", EditConditionHides))
	float BaseDamage = 0.0f;

	/** Health restored when this healing item is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Healing",
		meta = (ClampMin = "0.0", EditCondition = "ItemType == EASItemType::Healing", EditConditionHides))
	float HealAmount = 0.0f;
};
