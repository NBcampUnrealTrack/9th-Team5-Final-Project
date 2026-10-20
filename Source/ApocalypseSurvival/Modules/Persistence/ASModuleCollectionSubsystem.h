#pragma once

#include "CoreMinimal.h"
#include "Modules/Data/ASModuleTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASModuleCollectionSubsystem.generated.h"

class APlayerController;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASModuleCollectionChanged);

/** Local progression and next-run lobby choices. Does not apply gameplay effects or send server RPCs. */
UCLASS()
class APOCALYPSESURVIVAL_API UASModuleCollectionSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	static UASModuleCollectionSubsystem* Get(const APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "Modules")
	TArray<FASModuleInstanceData> GetOwnedModules() const { return OwnedModules; }

	UFUNCTION(BlueprintPure, Category = "Modules")
	TArray<FASModuleInstanceData> GetEquippedModules() const;

	UFUNCTION(BlueprintPure, Category = "Modules")
	TArray<FName> GetUnlockedModuleIds() const { return UnlockedModuleIds; }

	UFUNCTION(BlueprintPure, Category = "Modules")
	bool IsModuleUnlocked(FName ModuleId) const { return !ModuleId.IsNone() && UnlockedModuleIds.Contains(ModuleId); }
	bool OwnsModuleInstance(FGuid InstanceId) const;

	/** Unlocks the module type for future rewards. */
	UFUNCTION(BlueprintCallable, Category = "Modules")
	bool UnlockModule(FName ModuleId, FString& OutError);

	/** Adds an already-rolled reward instance; its option values are never rerolled here. */
	UFUNCTION(BlueprintCallable, Category = "Modules")
	bool GrantModuleInstance(const FASModuleInstanceData& Instance, FString& OutError);

	/** Saves the next run's selection. An invalid FGuid clears the slot. */
	UFUNCTION(BlueprintCallable, Category = "Modules")
	bool EquipModule(int32 SlotIndex, FGuid InstanceId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Modules")
	bool LoadProfile(FString& OutError);

	UPROPERTY(BlueprintAssignable, Category = "Modules")
	FASModuleCollectionChanged OnCollectionChanged;

private:
	bool CommitProfile(const TArray<FName>& NewUnlocked, const TArray<FASModuleInstanceData>& NewOwned, const TArray<FGuid>& NewEquipped, FString& OutError);
	FString GetSaveSlotName() const;

	UPROPERTY(Transient)
	TArray<FName> UnlockedModuleIds;

	UPROPERTY(Transient)
	TArray<FASModuleInstanceData> OwnedModules;

	UPROPERTY(Transient)
	TArray<FGuid> EquippedModuleInstanceIds;

	/** Failed/unsupported saves must not be silently overwritten by a fresh profile. */
	bool bProfileLoaded = false;
};
