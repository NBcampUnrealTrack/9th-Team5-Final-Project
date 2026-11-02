#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Core/UI/ASLoadingScreenSubsystem.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShaderPipelineCache.h"
#include "Core/Loading/ASLoadingDefinition.h"
#include "Core/Framework/ASGameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

void UASLoadingFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UASLoadingScreenSubsystem>();
	FWorldDelegates::OnGameInstanceWorldChanged.AddUObject(this, &ThisClass::HandleWorldChanged);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
}

void UASLoadingFlowSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FWorldDelegates::OnGameInstanceWorldChanged.RemoveAll(this);
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
	if (!GetWorld() || !IsLoadingLevelMap(GetWorld()->GetOutermost()->GetName())) return;
	if (bActive) return;
	Tasks.Empty();
	Status = {};
	Status.Phase = EASLoadingPhase::Preparing;
	Status.Title = Title.IsEmpty() ? NSLOCTEXT("ASLoading", "Title", "Loading") : Title;
	bActive = true;
	bWaitForPSO = bInWaitForPSO;
	Timeout = FMath::IsFinite(TimeoutSeconds) ? FMath::Max(1.f, TimeoutSeconds) : 120.f;
	StartedAt = FPlatformTime::Seconds();
	ReadySince = 0;
	Publish();
}

UASLoadingDefinition* UASLoadingFlowSubsystem::GetLoadingDefinition() const
{
	const auto* Instance = Cast<UASGameInstance>(GetGameInstance());
	return Instance ? Instance->LoadingDefinition.Get() : nullptr;
}

bool UASLoadingFlowSubsystem::IsLoadingLevelMap(const FString& MapName) const
{
	const auto* Definition = GetLoadingDefinition();
	const FString Loading = Definition ? Definition->LoadingMap.ToSoftObjectPath().GetLongPackageName() : FString();
	return !Loading.IsEmpty() && NormalizeMapName(MapName) == Loading;
}

bool UASLoadingFlowSubsystem::TravelThroughLoadingLevel(const FString& DestinationMap)
{
	UWorld* World = GetWorld();
	AGameModeBase* Mode = World ? World->GetAuthGameMode() : nullptr;
	const FString Target = NormalizeMapName(DestinationMap);
	const auto* Definition = GetLoadingDefinition();
	if (!Definition)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingLevel] Assign a Loading Definition data asset in the ASGameInstance Blueprint defaults."));
		return false;
	}
	const FString Loading = Definition->LoadingMap.ToSoftObjectPath().GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(Loading) || !FPackageName::DoesPackageExist(Loading))
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingLevel] Select an existing Loading Map in the Loading Definition data asset."));
		return false;
	}
	if (!FPackageName::IsValidLongPackageName(Target) || !FPackageName::DoesPackageExist(Target))
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingLevel] Supply a valid destination map or select Default Game Map in the Loading Definition data asset."));
		return false;
	}
	if (!Mode || World->IsInSeamlessTravel() || IsLoadingLevelMap(World->GetOutermost()->GetName())
		|| Target == Loading) return false;
	PendingDestination = Target;
	PendingTravelId = FGuid::NewGuid();
	ExpectedPlayers = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		if (It->IsValid() && It->Get()->PlayerState) ++ExpectedPlayers;
	ExpectedPlayers = FMath::Max(1, ExpectedPlayers);
	const bool bOldSeamless = Mode->bUseSeamlessTravel;
	Mode->bUseSeamlessTravel = true;
	const FString URL = Loading + (World->GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT(""));
	if (!World->ServerTravel(URL, true))
	{
		Mode->bUseSeamlessTravel = bOldSeamless;
		PendingDestination.Empty();
		PendingTravelId.Invalidate();
		ExpectedPlayers = 0;
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[LoadingLevel] Lobby -> %s -> %s (%d players)"), *Loading, *Target, ExpectedPlayers);
	return true;
}

void UASLoadingFlowSubsystem::BeginLoadingLevelPreparation(const FString& DestinationMap, FGuid TravelId, float TimeoutSeconds)
{
	if (bPreparingInLoadingLevel && PreparationId == TravelId) return;
	if (!GetWorld() || !IsLoadingLevelMap(GetWorld()->GetOutermost()->GetName())) return;
	// Feature preparation starts only after entering the actual loading world.
	bActive = false;
	BeginLoading(NSLOCTEXT("ASLoading", "LevelTitle", "Preparing game"), TimeoutSeconds, true);
	bPreparingInLoadingLevel = true;
	bPreparationReported = false;
	PreparationDestination = NormalizeMapName(DestinationMap);
	PreparationId = TravelId;
	Status.DestinationMap = PreparationDestination;
	Status.bInLoadingLevel = true;
	Publish();
	UE_LOG(LogTemp, Log, TEXT("[LoadingLevel] Preparation started IN %s for %s"), *GetWorld()->GetOutermost()->GetName(), *PreparationDestination);
	OnLoadingStarted.Broadcast();
}

void UASLoadingFlowSubsystem::ReportLoadingLevelFailure(const FText& Error)
{
	if (Status.Phase != EASLoadingPhase::Failed) Fail(Error);
}

void UASLoadingFlowSubsystem::HandleWorldChanged(UGameInstance* Instance, UWorld* OldWorld, UWorld* NewWorld)
{
	if (Instance != GetGameInstance() || !OldWorld || OldWorld == NewWorld
		|| !IsLoadingLevelMap(OldWorld->GetOutermost()->GetName())) return;
	// World switching runs before the destination's BeginPlay. Never cover the gameplay world.
	const bool bCompleted = bPreparingInLoadingLevel && bPreparationReported && Tasks.IsEmpty();
	bActive = false;
	bPreparingInLoadingLevel = false;
	bPreparationReported = false;
	Tasks.Empty();
	Status.bInLoadingLevel = false;
	Status.Phase = bCompleted ? EASLoadingPhase::Completed : EASLoadingPhase::Cancelled;
	PendingDestination.Empty();
	PendingTravelId.Invalidate();
	ExpectedPlayers = 0;
	GetGameInstance()->GetSubsystem<UASLoadingScreenSubsystem>()->HideLoadingScreen();
	Publish();
	UE_LOG(LogTemp, Log, TEXT("[LoadingLevel] Left loading world; screen closed before destination BeginPlay"));
	if (bCompleted) OnLoadingCompleted.Broadcast();
}

FGuid UASLoadingFlowSubsystem::BeginTask(FText Label, UObject* Owner)
{
	check(IsInGameThread());
	if (!bActive || !bPreparingInLoadingLevel || !GetWorld()
		|| !IsLoadingLevelMap(GetWorld()->GetOutermost()->GetName())) return {};
	const FGuid Id = FGuid::NewGuid();
	Tasks.Add(Id, { Label, Owner ? Owner : this });
	ReadySince = 0;
	bPreparationReported = false;
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
	bPreparingInLoadingLevel = false;
	bPreparationReported = false;
	PreparationDestination.Empty();
	PreparationId.Invalidate();
	PendingDestination.Empty();
	PendingTravelId.Invalidate();
	ExpectedPlayers = 0;
	Status.bInLoadingLevel = false;
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
	if (!GetWorld() || !IsLoadingLevelMap(GetWorld()->GetOutermost()->GetName())) return true;
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
	Status.Phase = EASLoadingPhase::Preparing;
	Status.Message = !Tasks.IsEmpty() ? Tasks.CreateConstIterator().Value().Label
		: NSLOCTEXT("ASLoading", "Rendering", "Preparing rendering...");
	const bool bReady = Tasks.IsEmpty() && Status.PendingPSOs == 0;
	if (!bReady) { ReadySince = 0; bPreparationReported = false; }
	else if (ReadySince == 0) ReadySince = Now;
	// Keep the overlay through a short quiet interval for component registration and PSO submission.
	if (bReady && Now - ReadySince >= 0.2)
	{
		if (bPreparingInLoadingLevel)
		{
			bPreparationReported = Now - StartedAt >= 0.5;
			if (bPreparationReported)
			{
				Status.Phase = EASLoadingPhase::WaitingForPlayers;
				Status.Message = NSLOCTEXT("ASLoading", "WaitingPlayers", "Waiting for all players...");
			}
			Publish();
			return true;
		}
	}
	else Publish();
	return true;
}
