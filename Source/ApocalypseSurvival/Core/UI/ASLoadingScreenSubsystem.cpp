#include "Core/UI/ASLoadingScreenSubsystem.h"

#include "Core/UI/SASLoadingScreen.h"
#include "Core/Framework/ASSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
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
	ViewportScreen = SNew(SASLoadingScreen).OnCancel(OnCancel);
	Viewport->AddViewportWidgetContent(ViewportScreen.ToSharedRef(), 10000);
	UE_LOG(LogTemp, Log, TEXT("[Loading] Show"));
}

void UASLoadingScreenSubsystem::HideLoadingScreen()
{
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

void UASLoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& Context, const FString& MapName)
{
	if (Context.OwningGameInstance != GetGameInstance() || IsRunningDedicatedServer() || IsRunningCommandlet()) return;
	ShowLoadingScreen();
	// PIE shares the movie player across instances. Its viewport overlay remains independent.
	if (Context.WorldType == EWorldType::PIE || !IsMoviePlayerEnabled()) return;
	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.WidgetLoadingScreen = SNew(SASLoadingScreen);
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
	bOwnsMovieScreen = true;
}

void UASLoadingScreenSubsystem::HandleSeamlessTravelStart(UWorld* World, const FString& MapName)
{
	if (World && World->GetGameInstance() == GetGameInstance()) ShowLoadingScreen();
}

void UASLoadingScreenSubsystem::HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (!ViewportScreen.IsValid() || !World || World != GetWorld() || World == SourceWorld.Get()) return;
	// Do not uncover the intermediate seamless-travel world or a client waiting for BeginPlay.
	if (World->HasBegunPlay() && !World->IsInSeamlessTravel())
	{
		UE_LOG(LogTemp, Log, TEXT("[Loading] Ready: %s"), *World->GetMapName());
		HideLoadingScreen();
	}
}

void UASLoadingScreenSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type FailureType, const FString& Error)
{
	if (World && Driver && Driver != World->GetNetDriver() && Driver->NetDriverName != FName(TEXT("PendingNetDriver"))
		&& Driver->NetDriverName != FName(TEXT("GameNetDriver"))) return;
	if (World && World->GetGameInstance() == GetGameInstance()
		&& (World->GetNetMode() != NM_ListenServer || UASSessionSubsystem::ShouldEndHostSessionForNetworkFailure(FailureType)))
	{
		HideLoadingScreen();
	}
}

void UASLoadingScreenSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (World && World->GetGameInstance() == GetGameInstance()) HideLoadingScreen();
}
