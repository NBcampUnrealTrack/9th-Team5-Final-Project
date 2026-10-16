#pragma once

#include "CoreMinimal.h"
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
	TArray<FName> GetOwnedModuleIds() const { return OwnedModuleIds; }

	UFUNCTION(BlueprintPure, Category = "Modules")
	TArray<FName> GetEquippedModuleIds() const { return EquippedModuleIds; }

	UFUNCTION(BlueprintPure, Category = "Modules")
	bool OwnsModule(FName ModuleId) const { return !ModuleId.IsNone() && OwnedModuleIds.Contains(ModuleId); }

	/** Call when a reward is confirmed. Duplicate unlocks succeed without adding a second copy. */
	UFUNCTION(BlueprintCallable, Category = "Modules")
	bool GrantModule(FName ModuleId, FString& OutError);

	/** Saves the next run's selection. NAME_None clears a slot; current match effects are unchanged. */
	UFUNCTION(BlueprintCallable, Category = "Modules")
	bool EquipModule(int32 SlotIndex, FName ModuleId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Modules")
	bool LoadProfile(FString& OutError);

	UPROPERTY(BlueprintAssignable, Category = "Modules")
	FASModuleCollectionChanged OnCollectionChanged;

private:
	bool CommitProfile(const TArray<FName>& NewOwned, const TArray<FName>& NewEquipped, FString& OutError);
	FString GetSaveSlotName() const;

	UPROPERTY(Transient)
	TArray<FName> OwnedModuleIds;

	UPROPERTY(Transient)
	TArray<FName> EquippedModuleIds;

	/** Failed/unsupported saves must not be silently overwritten by a fresh profile. */
	bool bProfileLoaded = false;
};
