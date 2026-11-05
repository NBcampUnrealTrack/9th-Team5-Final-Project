#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GAS/ASAbilitySet.h"
#include "Relics/Data/ASRelicDefinition.h"
#include "ASRelicComponent.generated.h"

class UASAbilitySystemComponent;

USTRUCT()
struct FASAppliedRelic
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UASRelicDefinition> Definition = nullptr;

	UPROPERTY()
	FASAbilitySet_GrantedHandles GrantedHandles;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASRelicLoadoutChanged);

/** Per-player, per-run relic slots and passive GameplayEffect grants. */
UCLASS(ClassGroup = (Relics), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASRelicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 DefaultSlotCount = 2;
	static constexpr int32 MaximumSlotCount = 3;

	UASRelicComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server-only. Adds a relic and applies its passive effects to the owning PlayerState ASC. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relics")
	bool EquipRelic(UASRelicDefinition* Definition, FString& OutError);

	/** Server-only. Removes one relic and only the effects it granted. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relics")
	bool UnequipRelic(FName RelicId);

	/** Server-only. Slot upgrades may increase capacity from two to three during this run. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relics")
	bool SetSlotCount(int32 NewSlotCount);

	/** Clears equipped relics, granted effects and slot upgrades at match end. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relics")
	void ResetForNewMatch();

	UFUNCTION(BlueprintPure, Category = "Relics")
	TArray<UASRelicDefinition*> GetEquippedRelics() const;

	UFUNCTION(BlueprintPure, Category = "Relics")
	int32 GetSlotCount() const { return SlotCount; }

	UPROPERTY(BlueprintAssignable, Category = "Relics")
	FASRelicLoadoutChanged OnRelicLoadoutChanged;

private:
	UASAbilitySystemComponent* GetRelicASC() const;
	void RemoveAppliedRelic(FName RelicId);

	UFUNCTION()
	void OnRep_EquippedRelics();

	/** Private to the owning player; effects themselves replicate through GAS. */
	UPROPERTY(ReplicatedUsing = OnRep_EquippedRelics)
	TArray<TObjectPtr<UASRelicDefinition>> EquippedRelics;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedRelics)
	int32 SlotCount = DefaultSlotCount;

	/** Server-only handles used to remove this component's effects without touching other buffs. */
	UPROPERTY(Transient)
	TArray<FASAppliedRelic> AppliedRelics;
};
