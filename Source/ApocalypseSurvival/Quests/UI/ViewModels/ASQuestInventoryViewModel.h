#pragma once
#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ASQuestInventoryViewModel.generated.h"

class APlayerController;
class APawn;

USTRUCT(BlueprintType)
struct FASQuestGoalPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Label;
	UPROPERTY(BlueprintReadOnly) FText Count;
	UPROPERTY(BlueprintReadOnly) bool bComplete = false;
	bool operator==(const FASQuestGoalPresentation& Other) const { return Label.EqualTo(Other.Label) && Count.EqualTo(Other.Count) && bComplete == Other.bComplete; }
};

USTRUCT(BlueprintType)
struct FASQuestItemPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Name;
	UPROPERTY(BlueprintReadOnly) FText Acquisition;
	UPROPERTY(BlueprintReadOnly) bool bAcquired = false;
	bool operator==(const FASQuestItemPresentation& Other) const { return Name.EqualTo(Other.Name) && Acquisition.EqualTo(Other.Acquisition) && bAcquired == Other.bAcquired; }
};

USTRUCT(BlueprintType)
struct FASQuestJournalEntryPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName QuestId;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Description;
	UPROPERTY(BlueprintReadOnly) FText Status;
	UPROPERTY(BlueprintReadOnly) bool bReported = false;
	UPROPERTY(BlueprintReadOnly) bool bReadyToReport = false;
	UPROPERTY(BlueprintReadOnly) TArray<FASQuestGoalPresentation> Goals;
	UPROPERTY(BlueprintReadOnly) TArray<FASQuestItemPresentation> Items;
	bool operator==(const FASQuestJournalEntryPresentation& Other) const;
};

USTRUCT(BlueprintType)
struct FASQuestInventoryPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FASQuestJournalEntryPresentation> Entries;
	UPROPERTY(BlueprintReadOnly) FText EmptyMessage;
	UPROPERTY(BlueprintReadOnly) bool bLoading = true;
	bool operator==(const FASQuestInventoryPresentation& Other) const { return Entries == Other.Entries && EmptyMessage.EqualTo(Other.EmptyMessage) && bLoading == Other.bLoading; }
};

/** Local projection only. Accepting, collecting and reporting remain server gameplay operations. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASQuestInventoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(APlayerController* Controller);
	void Shutdown();
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category = "Quest|ViewModel") void Refresh();
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Quest|ViewModel") FASQuestInventoryPresentation Presentation;
private:
	UFUNCTION() void HandlePawnChanged(APawn* Pawn);
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<class AASPlayerController> ObservedController;
	TWeakObjectPtr<class UASQuestComponent> Quests;
};
