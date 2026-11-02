#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASLoadingScreenSubsystem.generated.h"

class SWidget;
class UGameViewportClient;
class UNetDriver;
class APlayerController;
struct FASLoadingStatus;
struct FASLoadingScreenModel;

UCLASS()
class APOCALYPSESURVIVAL_API UASLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	void ShowLoadingScreen(const FSimpleDelegate& OnCancel = FSimpleDelegate());
	void HideLoadingScreen();
	void UpdateLoadingStatus(const FASLoadingStatus& Status);

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	TSharedPtr<SWidget> ViewportScreen;
	TWeakObjectPtr<UGameViewportClient> LoadingViewport;
	TSharedPtr<FASLoadingScreenModel, ESPMode::ThreadSafe> ScreenModel;
	TWeakObjectPtr<APlayerController> ErrorController;
	bool bPreviousMouseCursor = false;
};
