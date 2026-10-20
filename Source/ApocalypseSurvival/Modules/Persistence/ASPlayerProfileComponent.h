#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/Persistence/ASQuestSaveData.h"
#include "ASPlayerProfileComponent.generated.h"

class UASModuleCollectionSubsystem;
class UASQuestComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASOnPlayerProfileChanged);

/** Add alongside ASQuestComponent in the PlayerState BP to bridge quests and its local profile. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASPlayerProfileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASPlayerProfileComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Profile")
	bool IsProfileInitialized() const { return bProfileInitialized; }

	UFUNCTION(BlueprintPure, Category = "Profile")
	FString GetLastProfileError() const { return LastProfileError; }

	/** Server-side input for a future personal drop roll. Empty until this player's profile is ready. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Profile|Modules")
	TArray<FName> GetPersonalDropModuleIds() const;

	/** Trusted server integration API; rejects a second import into an initialized PlayerState. */
	bool InitializeProfile(const FASQuestSaveData& Quests, const TArray<FName>& UnlockedModules, FString& OutError);

	/** Explicit save request, for example before a menu-controlled disconnect. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Profile")
	void ServerSaveProfile();

	UPROPERTY(BlueprintAssignable, Category = "Profile")
	FASOnPlayerProfileChanged OnProfileChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UASQuestComponent* GetQuests() const;
	void TryLoadOwnerProfile();
	void FailInitialization(const FString& Error);
	void ReportLocalProfileError(const FString& Error);
	bool ResolveUnlockedModules(const TArray<FName>& Ids, TArray<FName>& OutIds, FString& OutError) const;

	UFUNCTION()
	void HandleQuestsChanged();

	UFUNCTION()
	void HandleLocalModulesChanged();

	UFUNCTION()
	void OnRep_ProfileInitialized();

	UFUNCTION(Client, Reliable)
	void ClientLoadLocalProfile();

	/** Imported once per PlayerState. Local cooperative saves are trusted, not account authentication. */
	UFUNCTION(Server, Reliable)
	void ServerRestoreLocalProfile(const FASQuestSaveData& Quests, const TArray<FName>& UnlockedModules);

	UFUNCTION(Server, Reliable)
	void ServerUpdateUnlockedModules(const TArray<FName>& UnlockedModules);

	UFUNCTION(Server, Reliable)
	void ServerReportLoadFailure(const FString& Error);

	UFUNCTION(Client, Reliable)
	void ClientSaveQuestProgress(const FASQuestSaveData& Quests);

	UFUNCTION(Client, Reliable)
	void ClientReportProfileError(const FString& Error);

	UPROPERTY(ReplicatedUsing = OnRep_ProfileInitialized)
	bool bProfileInitialized = false;

	/** Server-side personal unlocks. There is no shared drop pool. */
	UPROPERTY(Transient)
	TArray<FName> PersonalUnlockedModuleIds;

	UPROPERTY(Transient)
	FString LastProfileError;

	TWeakObjectPtr<UASModuleCollectionSubsystem> LocalCollection;
	FTimerHandle LoadRetryTimer;
	bool bInitializationFailed = false;
	bool bLocalProfileSubmitted = false;
};
