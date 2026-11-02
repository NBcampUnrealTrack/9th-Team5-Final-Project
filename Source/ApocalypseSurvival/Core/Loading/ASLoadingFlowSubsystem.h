#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASLoadingFlowSubsystem.generated.h"

class UASLoadingDefinition;

UENUM(BlueprintType)
enum class EASLoadingPhase : uint8 { Idle, Traveling, Preparing, Completed, Failed, Cancelled, WaitingForPlayers };

USTRUCT(BlueprintType)
struct FASLoadingStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EASLoadingPhase Phase = EASLoadingPhase::Idle;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Message;
	UPROPERTY(BlueprintReadOnly) int32 PendingPSOs = 0;
	UPROPERTY(BlueprintReadOnly) int32 PendingTasks = 0;
	UPROPERTY(BlueprintReadOnly) FString DestinationMap;
	UPROPERTY(BlueprintReadOnly) bool bInLoadingLevel = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASLoadingStatusChanged, const FASLoadingStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASLoadingStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASLoadingCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASLoadingFailed, FText, Error);

/** Preparation barrier in the loading level. Callers own their asynchronous work. Game-thread API. */
UCLASS()
class APOCALYPSESURVIVAL_API UASLoadingFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** Authority only: lobby -> loading level -> destination, preserving the current network session. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Loading|Travel")
	bool TravelThroughLoadingLevel(const FString& DestinationMap);
	UFUNCTION(BlueprintPure, Category="Loading") UASLoadingDefinition* GetLoadingDefinition() const;
	UFUNCTION(BlueprintPure, Category="Loading|Travel") FString GetDestinationMap() const { return Status.DestinationMap; }
	UFUNCTION(BlueprintPure, Category="Loading|Travel") bool IsPreparingInLoadingLevel() const { return bPreparingInLoadingLevel; }
	UFUNCTION(BlueprintPure, Category="Loading|Travel") bool IsLoadingLevelPrepared() const { return bActive && bPreparingInLoadingLevel && bPreparationReported; }
	/** Register during OnLoadingStarted, BEFORE async work. Outside preparation returns an invalid ID. */
	UFUNCTION(BlueprintCallable, Category="Loading") FGuid BeginTask(FText Label, UObject* Owner);
	UFUNCTION(BlueprintCallable, Category="Loading") void CompleteTask(FGuid TaskId);
	UFUNCTION(BlueprintCallable, Category="Loading") void FailTask(FGuid TaskId, FText Error);
	/** Cancels the screen barrier, not the callers' operations or an engine travel. */
	UFUNCTION(BlueprintCallable, Category="Loading") void CancelLoading();
	UFUNCTION(BlueprintCallable, Category="Loading") void ReturnToMainMenu();
	UFUNCTION(BlueprintPure, Category="Loading") FASLoadingStatus GetStatus() const { return Status; }
	UFUNCTION(BlueprintPure, Category="Loading") bool IsLoading() const { return bActive; }
	/** Register your preparation tasks synchronously here, then launch your own async loads. */
	UPROPERTY(BlueprintAssignable, Category="Loading") FASLoadingStarted OnLoadingStarted;
	UPROPERTY(BlueprintAssignable, Category="Loading") FASLoadingStatusChanged OnStatusChanged;
	UPROPERTY(BlueprintAssignable, Category="Loading") FASLoadingCompleted OnLoadingCompleted;
	UPROPERTY(BlueprintAssignable, Category="Loading") FASLoadingFailed OnLoadingFailed;
	void AbortForTravelFailure();
	static FString NormalizeMapName(const FString& MapName);
	bool IsLoadingLevelMap(const FString& MapName) const;
	void BeginLoadingLevelPreparation(const FString& DestinationMap, FGuid TravelId, float TimeoutSeconds);
	void ReportLoadingLevelFailure(const FText& Error);
	const FString& GetPendingDestination() const { return PendingDestination; }
	FGuid GetPendingTravelId() const { return PendingTravelId; }
	int32 GetExpectedPlayers() const { return ExpectedPlayers; }
private:
	void BeginLoading(FText Title, float TimeoutSeconds, bool bWaitForPSO);
	struct FTask { FText Label; TWeakObjectPtr<UObject> Owner; };
	TMap<FGuid, FTask> Tasks;
	FASLoadingStatus Status;
	FTSTicker::FDelegateHandle TickHandle;
	double StartedAt = 0;
	double ReadySince = 0;
	float Timeout = 120.f;
	bool bActive = false;
	bool bWaitForPSO = true;
	bool bPreparingInLoadingLevel = false;
	bool bPreparationReported = false;
	FString PreparationDestination;
	FGuid PreparationId;
	FString PendingDestination;
	FGuid PendingTravelId;
	int32 ExpectedPlayers = 0;
	void HandleWorldChanged(UGameInstance* Instance, UWorld* OldWorld, UWorld* NewWorld);
	bool Tick(float DeltaSeconds);
	void Fail(const FText& Error);
	void Publish();
};
