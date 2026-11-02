#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Core/UI/ASLoadingScreenSubsystem.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShaderPipelineCache.h"

void UASLoadingFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UASLoadingScreenSubsystem>();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
}

void UASLoadingFlowSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Tasks.Empty();
	bActive = false;
	Super::Deinitialize();
}

FString UASLoadingFlowSubsystem::NormalizeMapName(const FString& MapName)
{
	FString CleanName = MapName;
	int32 OptionsIndex;
	if (CleanName.FindChar(TEXT('?'), OptionsIndex)) CleanName.LeftInline(OptionsIndex);
	FString Package = FPackageName::ObjectPathToPackageName(CleanName);
	const FString Short = FPackageName::GetShortName(Package);
	if (Short.StartsWith(TEXT("UEDPIE_")))
	{
		const int32 Separator = Short.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7);
		if (Separator != INDEX_NONE) Package = FPackageName::GetLongPackagePath(Package) / Short.Mid(Separator + 1);
	}
	return Package;
}

void UASLoadingFlowSubsystem::BeginLoading(FText Title, float TimeoutSeconds, bool bInWaitForPSO)
{
	check(IsInGameThread());
	if (bActive) return;
	Tasks.Empty();
	Status = {};
	Status.Phase = EASLoadingPhase::Preparing;
	Status.Title = Title.IsEmpty() ? NSLOCTEXT("ASLoading", "Title", "Loading") : Title;
	bActive = true;
	bWorldReady = true;
	bWaitForPSO = bInWaitForPSO;
	Timeout = FMath::IsFinite(TimeoutSeconds) ? FMath::Max(1.f, TimeoutSeconds) : 120.f;
	ExpectedMap.Empty();
	StartedAt = FPlatformTime::Seconds();
	ReadySince = 0;
	Publish();
	// Register all blockers here, before any async callbacks can complete.
	OnLoadingStarted.Broadcast();
}

void UASLoadingFlowSubsystem::BeginMapLoading(const FString& MapName)
{
	const FString Map = NormalizeMapName(MapName);
	// Ignore the intermediate seamless-travel world without discarding destination tasks.
	if (bActive && !ExpectedMap.IsEmpty() && ExpectedMap != Map) return;
	if (!bActive) BeginLoading(FText::GetEmpty());
	if (!bActive) return; // A start listener may have cancelled the window.
	ExpectedMap = Map;
	bWorldReady = false;
	Status.Phase = EASLoadingPhase::Traveling;
	Publish();
}

void UASLoadingFlowSubsystem::NotifyWorldReady(UWorld* World)
{
	if (!bActive || !World || World->GetGameInstance() != GetGameInstance() || World != GetWorld()) return;
	if (!ExpectedMap.IsEmpty() && NormalizeMapName(World->GetOutermost()->GetName()) != ExpectedMap) return;
	if (World->HasBegunPlay() && !World->IsInSeamlessTravel()) bWorldReady = true;
}

FGuid UASLoadingFlowSubsystem::BeginTask(FText Label, UObject* Owner)
{
	check(IsInGameThread());
	if (!bActive) BeginLoading(FText::GetEmpty());
	if (!bActive) return {};
	const FGuid Id = FGuid::NewGuid();
	Tasks.Add(Id, { Label, Owner ? Owner : this });
	ReadySince = 0;
	return Id;
}

void UASLoadingFlowSubsystem::CompleteTask(FGuid TaskId)
{
	check(IsInGameThread());
	Tasks.Remove(TaskId);
}

void UASLoadingFlowSubsystem::FailTask(FGuid TaskId, FText Error)
{
	check(IsInGameThread());
	if (Tasks.Contains(TaskId)) Fail(Error);
}

void UASLoadingFlowSubsystem::Fail(const FText& Error)
{
	bActive = false;
	Status.Phase = EASLoadingPhase::Failed;
	Status.Message = Error;
	Tasks.Empty();
	UE_LOG(LogTemp, Warning, TEXT("[Loading] %s"), *Error.ToString());
	Publish();
	OnLoadingFailed.Broadcast(Error);
}

void UASLoadingFlowSubsystem::CancelLoading()
{
	check(IsInGameThread());
	bActive = false;
	Tasks.Empty();
	Status.Phase = EASLoadingPhase::Cancelled;
	GetGameInstance()->GetSubsystem<UASLoadingScreenSubsystem>()->HideLoadingScreen();
	Publish();
}

void UASLoadingFlowSubsystem::AbortForTravelFailure() { CancelLoading(); }

void UASLoadingFlowSubsystem::ReturnToMainMenu()
{
	CancelLoading();
	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_Standalone)
	{
		GetGameInstance()->GetSubsystem<UASSessionSubsystem>()->LeaveRoom();
		return;
	}
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*UGameMapsSettings::GetGameDefaultMap()));
}

void UASLoadingFlowSubsystem::Publish()
{
	Status.PendingTasks = Tasks.Num();
	GetGameInstance()->GetSubsystem<UASLoadingScreenSubsystem>()->UpdateLoadingStatus(Status);
	OnStatusChanged.Broadcast(Status);
}

bool UASLoadingFlowSubsystem::Tick(float DeltaSeconds)
{
	if (!bActive) return true;
	const double Now = FPlatformTime::Seconds();
	if (Now - StartedAt > Timeout)
	{
		Fail(NSLOCTEXT("ASLoading", "Timeout", "Loading timed out. Return to the menu and try again."));
		return true;
	}
	for (const auto& Task : Tasks)
	{
		if (!Task.Value.Owner.IsValid())
		{
			Fail(NSLOCTEXT("ASLoading", "OwnerLost", "A required loading task lost its owner."));
			return true;
		}
	}
	Status.PendingPSOs = bWaitForPSO && !IsRunningDedicatedServer() ? FShaderPipelineCache::NumPrecompilesRemaining() : 0;
	Status.Phase = bWorldReady ? EASLoadingPhase::Preparing : EASLoadingPhase::Traveling;
	Status.Message = !bWorldReady ? NSLOCTEXT("ASLoading", "World", "Preparing level...")
		: !Tasks.IsEmpty() ? Tasks.CreateConstIterator().Value().Label
		: NSLOCTEXT("ASLoading", "Rendering", "Preparing rendering...");
	const bool bReady = bWorldReady && Tasks.IsEmpty() && Status.PendingPSOs == 0;
	if (!bReady) ReadySince = 0;
	else if (ReadySince == 0) ReadySince = Now;
	// Keep the overlay through a short quiet interval for component registration and PSO submission.
	if (bReady && Now - ReadySince >= 0.2)
	{
		bActive = false;
		Status.Phase = EASLoadingPhase::Completed;
		UE_LOG(LogTemp, Log, TEXT("[Loading] Completed: %s"), *ExpectedMap);
		GetGameInstance()->GetSubsystem<UASLoadingScreenSubsystem>()->HideLoadingScreen();
		Publish();
		OnLoadingCompleted.Broadcast();
	}
	else Publish();
	return true;
}
