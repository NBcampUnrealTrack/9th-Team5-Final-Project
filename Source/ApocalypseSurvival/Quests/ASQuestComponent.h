#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/Data/ASQuestTypes.h"
#include "Quests/Persistence/ASQuestSaveData.h"
#include "ASQuestComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASOnQuestsChanged);

class AASQuestNpc;
class UASQuestDialogueComponent;

/** Personal quests, owned by PlayerState so death/respawn preserves progress. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	friend class UASQuestDialogueComponent;
	UASQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority only, after restore. The asset must be in the project quest catalog. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	bool AcceptQuest(UASQuestDefinition* Definition);

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool CanAcceptQuest(const UASQuestDefinition* Definition) const;

	/** Compatibility entry point. New interactions use ASQuestDialogueComponent directly. */
	void BeginNpcDialogue(AASQuestNpc* Npc);

	/** Reportable quests, new offers, then reminders. Priority and catalog order resolve ties. */
	UFUNCTION(BlueprintPure, Category = "Quest|Dialogue")
	UASQuestDefinition* FindQuestForNpc(FName NpcId) const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Quest|Dialogue", meta = (DeprecatedFunction, DeprecationMessage = "Use ASQuestDialogueComponent.ServerAdvanceDialogue"))
	void ServerAdvanceDialogue();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Quest|Dialogue", meta = (DeprecatedFunction, DeprecationMessage = "Use ASQuestDialogueComponent.ServerCloseDialogue"))
	void ServerCloseDialogue();

	/** Accumulates matching objectives on active quests. Returns whether anything changed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	bool HandleQuestEvent(const FASQuestEvent& Event);

	/** Number still eligible for collection, respecting acceptance, sequence and duplicate protection. */
	UFUNCTION(BlueprintPure, Category = "Quest|Inventory")
	int32 GetCollectibleQuestItemCount(const UASItemDefinition* Item) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Inventory")
	int32 CollectQuestItem(UASItemDefinition* Item, int32 Quantity);

	/** With missing entries enabled this is also the quest-tab acquisition checklist. */
	UFUNCTION(BlueprintPure, Category = "Quest|Inventory")
	TArray<FASQuestItemEntry> GetQuestItems(bool bIncludeMissing = false) const;

	/** Clears unfinished run-bound evidence on defeat or a clear without all objectives. */
	void EndQuestRun();

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

	/** Server-only, transactional restore. Definitions must exist in the project quest catalog. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Save")
	bool RestoreSaveData(const FASQuestSaveData& Data, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "Quest|Save")
	bool IsPersistenceReady() const;

	/** Server changes and client OnRep both notify. Read GetQuests() when initially binding UI too. */
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FASOnQuestsChanged OnQuestsChanged;

protected:
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;

	/** Serialized legacy data only; no longer used to register or accept quests. */
	UPROPERTY()
	TArray<TObjectPtr<UASQuestDefinition>> StartingQuests;

	/** Runtime cache loaded exclusively from Project Settings -> Quests -> Catalog. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UASQuestDefinition>> QuestCatalog;

	UPROPERTY()
	TArray<TSoftObjectPtr<UASQuestDefinition>> QuestCatalogAssets;

private:
	/** Defer notifications until the outermost state operation has committed. */
	class FScopedQuestChange
	{
	public:
		explicit FScopedQuestChange(UASQuestComponent& InOwner);
		~FScopedQuestChange();
		FScopedQuestChange(const FScopedQuestChange&) = delete;
		FScopedQuestChange& operator=(const FScopedQuestChange&) = delete;
	private:
		UASQuestComponent& Owner;
	};
	int32 QuestChangeDepth = 0;
	bool bQuestChangePending = false;
	void FlushQuestChanges();
	/** Only prerequisite-free bootstrap quests may start without explicit acceptance. */
	bool IsAutomaticQuest(const UASQuestDefinition* Definition) const;
	void AcceptAvailableAutomaticQuests();
	FString CatalogLoadError;
	bool ApplyQuestEvent(const FASQuestEvent& Event, const UASItemDefinition* CollectedItem);
	bool ClaimRewardInternal(FName QuestId, FName NpcId);
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
