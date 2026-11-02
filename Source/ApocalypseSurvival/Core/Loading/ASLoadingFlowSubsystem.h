#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASLoadingFlowSubsystem.generated.h"

UENUM(BlueprintType)
enum class EASLoadingPhase : uint8 { Idle, Traveling, Preparing, Completed, Failed, Cancelled };

USTRUCT(BlueprintType)
struct FASLoadingStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EASLoadingPhase Phase = EASLoadingPhase::Idle;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Message;
	UPROPERTY(BlueprintReadOnly) int32 PendingPSOs = 0;
	UPROPERTY(BlueprintReadOnly) int32 PendingTasks = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASLoadingStatusChanged, const FASLoadingStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASLoadingStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASLoadingCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASLoadingFailed, FText, Error);

/** A loading-screen barrier only. Callers perform and retain their own asynchronous work. Game-thread API. */
UCLASS()
class APOCALYPSESURVIVAL_API UASLoadingFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** Opens a manual loading window. If one is already active, its tasks are preserved. */
	UFUNCTION(BlueprintCallable, Category="Loading")
	void BeginLoading(FText Title, float TimeoutSeconds = 120.f, bool bWaitForPSO = true);
	/** Register BEFORE starting your async operation. Null Owner means game-instance lifetime. */
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
	void BeginMapLoading(const FString& MapName);
	void NotifyWorldReady(UWorld* World);
	void AbortForTravelFailure();
	static FString NormalizeMapName(const FString& MapName);
private:
	friend class FASLoadingFlowCoordinationTest;
	friend class FASLoadingFailureTest;
	struct FTask { FText Label; TWeakObjectPtr<UObject> Owner; };
	TMap<FGuid, FTask> Tasks;
	FASLoadingStatus Status;
	FTSTicker::FDelegateHandle TickHandle;
	FString ExpectedMap;
	double StartedAt = 0;
	double ReadySince = 0;
	float Timeout = 120.f;
	bool bActive = false;
	bool bWorldReady = true;
	bool bWaitForPSO = true;
	bool Tick(float DeltaSeconds);
	void Fail(const FText& Error);
	void Publish();
};
