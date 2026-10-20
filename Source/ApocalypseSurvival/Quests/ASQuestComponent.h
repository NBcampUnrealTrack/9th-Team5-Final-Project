#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/Data/ASQuestTypes.h"
#include "Quests/Persistence/ASQuestSaveData.h"
#include "ASQuestComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASOnQuestsChanged);

/** Personal quests, owned by PlayerState so death/respawn preserves progress. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority only, after restore. The asset must be in QuestCatalog or StartingQuests. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	bool AcceptQuest(UASQuestDefinition* Definition);

	/** Accumulates matching objectives on active quests. Returns whether anything changed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	bool HandleQuestEvent(const FASQuestEvent& Event);

	/** Authority only. Refuses early, repeated, or unfulfillable reward claims. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	bool ClaimReward(FName QuestId);

	/** Owning client UI may request a claim; the server validates its own progress. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Quest")
	void ServerClaimReward(FName QuestId);

	UFUNCTION(BlueprintPure, Category = "Quest")
	TArray<FASQuestProgress> GetQuests() const { return Quests; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool GetQuestProgress(FName QuestId, FASQuestProgress& OutProgress) const;

	UFUNCTION(BlueprintPure, Category = "Quest|Save")
	FASQuestSaveData ExportSaveData() const;

	/** Server-only, transactional restore. Definitions must exist in QuestCatalog/StartingQuests. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Save")
	bool RestoreSaveData(const FASQuestSaveData& Data, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "Quest|Save")
	bool IsPersistenceReady() const;

	/** Server changes and client OnRep both notify. Read GetQuests() when initially binding UI too. */
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FASOnQuestsChanged OnQuestsChanged;

protected:
	virtual void BeginPlay() override;

	/** Configure on the PlayerState BP. Accepted after the personal profile has been restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	TArray<TObjectPtr<UASQuestDefinition>> StartingQuests;

	/** All persistent quest assets, including quests accepted later through NPCs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Save")
	TArray<TObjectPtr<UASQuestDefinition>> QuestCatalog;

private:
	bool HasQuestAuthority() const;
	void NotifyQuestsChanged();

	UFUNCTION()
	void OnRep_Quests();

	/** Personal quest state is sent only to the owning client. */
	UPROPERTY(ReplicatedUsing = OnRep_Quests)
	TArray<FASQuestProgress> Quests;

	/** Start closed, including before BeginPlay, until an authoritative restore succeeds. */
	UPROPERTY(Replicated)
	bool bPersistenceReady = false;
};
