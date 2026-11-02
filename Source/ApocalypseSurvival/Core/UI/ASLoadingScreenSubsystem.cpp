#include "Core/UI/ASLoadingScreenSubsystem.h"

#include "Core/UI/SASLoadingScreen.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"

void UASLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravelStart);
	FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::HandleWorldPostActorTick);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
}

void UASLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FWorldDelegates::OnSeamlessTravelStart.RemoveAll(this);
	FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	HideLoadingScreen();
	Super::Deinitialize();
}

void UASLoadingScreenSubsystem::ShowLoadingScreen(const FSimpleDelegate& OnCancel)
{
	if (ViewportScreen.IsValid() || IsRunningDedicatedServer() || IsRunningCommandlet() || !FSlateApplication::IsInitialized()) return;
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!Viewport) return;
	SourceWorld = GetWorld();
	LoadingViewport = Viewport;
	if (!ScreenModel) ScreenModel = MakeShared<FASLoadingScreenModel, ESPMode::ThreadSafe>();
	ViewportScreen = SNew(SASLoadingScreen).Model(ScreenModel).OnCancel(OnCancel)
		.OnReturnToMenu(FSimpleDelegate::CreateWeakLambda(this, [this]()
		{ GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->ReturnToMainMenu(); }));
	Viewport->AddViewportWidgetContent(ViewportScreen.ToSharedRef(), 10000);
	UE_LOG(LogTemp, Log, TEXT("[Loading] Show"));
}

void UASLoadingScreenSubsystem::HideLoadingScreen()
{
	if (APlayerController* PC = ErrorController.Get()) PC->bShowMouseCursor = bPreviousMouseCursor;
	ErrorController.Reset();
	if (UGameViewportClient* Viewport = LoadingViewport.Get(); Viewport && ViewportScreen.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(ViewportScreen.ToSharedRef());
	}
	if (bOwnsMovieScreen && IsMoviePlayerEnabled()) GetMoviePlayer()->StopMovie();
	bOwnsMovieScreen = false;
	ViewportScreen.Reset();
	LoadingViewport.Reset();
	SourceWorld.Reset();
}

void UASLoadingScreenSubsystem::UpdateLoadingStatus(const FASLoadingStatus& Status)
{
	if (!ScreenModel) ScreenModel = MakeShared<FASLoadingScreenModel, ESPMode::ThreadSafe>();
	ScreenModel->Update(Status);
	if (Status.Phase == EASLoadingPhase::Failed && !ErrorController.IsValid())
	{
		if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
		{
			ErrorController = PC;
			bPreviousMouseCursor = PC->bShowMouseCursor;
			PC->bShowMouseCursor = true;
		}
	}
	if (Status.Phase != EASLoadingPhase::Completed && Status.Phase != EASLoadingPhase::Cancelled)
		ShowLoadingScreen(); // A viewport may not have existed when startup loading began.
}

void UASLoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& Context, const FString& MapName)
{
	if (Context.OwningGameInstance != GetGameInstance() || IsRunningDedicatedServer() || IsRunningCommandlet()) return;
	ShowLoadingScreen();
	GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->BeginMapLoading(MapName);
	// PIE shares the movie player across instances. Its viewport overlay remains independent.
	if (Context.WorldType == EWorldType::PIE || !IsMoviePlayerEnabled()) return;
	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.WidgetLoadingScreen = SNew(SASLoadingScreen).Model(ScreenModel);
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
	bOwnsMovieScreen = true;
}

void UASLoadingScreenSubsystem::HandleSeamlessTravelStart(UWorld* World, const FString& MapName)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		ShowLoadingScreen();
		GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->BeginMapLoading(MapName);
	}
}

void UASLoadingScreenSubsystem::HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->NotifyWorldReady(World);
}

void UASLoadingScreenSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type FailureType, const FString& Error)
{
	if (World && Driver && Driver != World->GetNetDriver() && Driver->NetDriverName != FName(TEXT("PendingNetDriver"))
		&& Driver->NetDriverName != FName(TEXT("GameNetDriver"))) return;
	if (World && World->GetGameInstance() == GetGameInstance()
		&& (World->GetNetMode() != NM_ListenServer || UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(FailureType)))
	{
		GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->AbortForTravelFailure();
		HideLoadingScreen();
	}
}

void UASLoadingScreenSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>()->AbortForTravelFailure();
		HideLoadingScreen();
	}
}
