#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/ASAbilitySet.h"
#include "Modules/Data/ASModuleTypes.h"
#include "ASModuleComponent.generated.h"

class UASAbilitySystemComponent;

USTRUCT()
struct FASAppliedModule
{
	GENERATED_BODY()

	UPROPERTY()
	FName ModuleId;

	UPROPERTY()
	float CoreValue = 0.0f;

	UPROPERTY()
	FASAbilitySet_GrantedHandles Handles;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASModuleLoadoutChanged);

/** Server-owned, per-run module effects on PlayerState. Local progression is not replicated here. */
UCLASS(ClassGroup = (Modules), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASModuleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASModuleComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server integration point for a validated lobby selection, before avatar initialization.
	 * The caller is responsible for ownership/reward verification for remote players.
	 * This is intentionally not a client RPC accepting arbitrary local save data.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Modules")
	bool ConfigureLoadout(const TArray<FASModuleInstanceData>& Instances, FString& OutError);

	/** Call after ASC actor info/default abilities are ready. Repeated calls do not duplicate grants. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Modules")
	void InitializeForAvatar();
	void SuspendForAvatar(AActor* Avatar);

	/** Game flow may clear the previous run before configuring the next one. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Modules")
	void ResetForNextRun();

	UFUNCTION(BlueprintPure, Category = "Modules")
	TArray<FASModuleInstanceData> GetEquippedModules() const { return EquippedModules; }

	UFUNCTION(BlueprintPure, Category = "Modules")
	bool AreEffectsApplied() const { return bEffectsApplied; }

	/** Core magnitude captured for this run, also available to special module abilities. */
	UFUNCTION(BlueprintPure, Category = "Modules")
	bool GetModuleCoreValue(FName ModuleId, float& OutValue) const;

	/** Resolve a special ability's owning module without a per-module source asset. */
	bool GetAbilityCoreValue(FGameplayAbilitySpecHandle AbilityHandle, float& OutValue) const;

	UPROPERTY(BlueprintAssignable, Category = "Modules")
	FASModuleLoadoutChanged OnLoadoutChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UASAbilitySystemComponent* GetModuleASC() const;
	bool ApplyConfiguredEffects(FString& OutError);
	void RemoveModuleEffects();
	void UpdatePassives(bool bActivate);
	void RestoreResourceBounds(bool bFillResources);

	UFUNCTION()
	void OnRep_Loadout();

	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	TArray<FASModuleInstanceData> EquippedModules;

	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	bool bEffectsApplied = false;

	UPROPERTY(Transient)
	TArray<FASAppliedModule> AppliedModules;

	bool bLoadoutConfigured = false;
	TWeakObjectPtr<AActor> InitializedAvatar;
};
