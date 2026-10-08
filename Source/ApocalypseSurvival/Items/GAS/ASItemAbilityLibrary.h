#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayEffectTypes.h"
#include "ASItemAbilityLibrary.generated.h"

class UASItemDefinition;
class UASItemInstance;
class UASInventoryComponent;
class UAbilitySystemComponent;
class APawn;

/** Item data access shared by abilities regardless of their base class. */
UCLASS()
class APOCALYPSESURVIVAL_API UASItemAbilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Pass the ability's GetCurrentSourceObject() result. Returns null for non-item sources. */
	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	static UASItemInstance* GetItemFromSourceObject(UObject* SourceObject);

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	static UASItemDefinition* GetItemDefinitionFromSourceObject(UObject* SourceObject);

	/** Returns null for a missing source, missing definition, or a non-weapon item. */
	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	static UASItemDefinition* GetWeaponDefinitionFromSourceObject(UObject* SourceObject);

	UFUNCTION(BlueprintPure, Category = "Item|GAS")
	static UASInventoryComponent* GetInventoryFromSourceObject(UObject* SourceObject);

	/** Rejects a mismatched inventory owner or stale GAS avatar after respawn. */
	static UAbilitySystemComponent* ResolveInventoryAbilitySystem(const UASInventoryComponent* Inventory, APawn* Avatar);

	/** For attack abilities with another base class. Commit once per actual attack on the server. */
	UFUNCTION(BlueprintPure, Category = "Item|GAS|Weapon")
	static bool CanCommitWeaponAttack(UObject* SourceObject, APawn* Avatar);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|GAS|Weapon")
	static bool TryCommitWeaponAttack(UObject* SourceObject, APawn* Avatar);

	/** Returns one damage spec for the prototype. Snapshot at firing time and apply on the server.
	 * AdditionalDamage is the combat system's calculated attribute contribution.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|GAS|Weapon")
	static TArray<FGameplayEffectSpecHandle> MakeWeaponHitEffectSpecs(
		UObject* SourceObject, UAbilitySystemComponent* SourceASC, float AdditionalDamage = 0.0f);
};
