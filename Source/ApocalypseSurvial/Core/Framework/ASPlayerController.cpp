#include "Core/Framework/ASPlayerController.h"

#include "ApocalypseSurvial.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Core/Framework/ASPlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"

AASPlayerController::AASPlayerController()
{
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("PathFollowingComponent"));
	PlayerCameraManagerClass = AASPlayerCameraManager::StaticClass();

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AASPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInputComponent))
	{
		UE_LOG(LogApocalypseSurvial, Error, TEXT("'%s' has no EnhancedInputComponent. Input will not be bound."), *GetNameSafe(this));
		return;
	}

	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Started, this, &AASPlayerController::OnMoveStarted);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AASPlayerController::OnMoveHeld);
	EnhancedInputComponent->BindAction(StopAction, ETriggerEvent::Started, this, &AASPlayerController::OnStop);

	EnhancedInputComponent->BindAction(CameraLockToggleAction, ETriggerEvent::Started, this, &AASPlayerController::OnCameraLockToggled);
	EnhancedInputComponent->BindAction(CameraCenterAction, ETriggerEvent::Started, this, &AASPlayerController::OnCameraCenterStarted);
	EnhancedInputComponent->BindAction(CameraCenterAction, ETriggerEvent::Completed, this, &AASPlayerController::OnCameraCenterCompleted);
	EnhancedInputComponent->BindAction(CameraCenterAction, ETriggerEvent::Canceled, this, &AASPlayerController::OnCameraCenterCompleted);
}

void AASPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateCameraEdgeScroll(DeltaTime);
}

AASPlayerCameraManager* AASPlayerController::GetASPlayerCameraManager() const
{
	return Cast<AASPlayerCameraManager>(PlayerCameraManager);
}

bool AASPlayerController::GetCursorWorldLocation(FVector& OutLocation) const
{
	FHitResult Hit;
	if (!GetHitResultUnderCursor(CursorTraceChannel, true, Hit))
	{
		return false;
	}

	OutLocation = Hit.Location;
	return true;
}

void AASPlayerController::RequestMoveTo(const FVector& Destination)
{
	if (!IsValid(GetPawn()))
	{
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!IsValid(NavSys))
	{
		return;
	}

	// Clicks on walls or outside the walkable area still move the pawn as
	// close as it can get instead of being ignored.
	FNavLocation NavLocation;
	if (!NavSys->ProjectPointToNavigation(Destination, NavLocation, NavProjectExtent))
	{
		return;
	}

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, NavLocation.Location);
	LastMoveRequestTime = GetWorld()->GetTimeSeconds();
}

void AASPlayerController::OnMoveStarted()
{
	FVector Destination;
	if (!GetCursorWorldLocation(Destination))
	{
		return;
	}

	RequestMoveTo(Destination);

	if (IsValid(MoveCursorFX))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MoveCursorFX, Destination);
	}
}

void AASPlayerController::OnMoveHeld()
{
	// Path finding every frame is wasteful, so held input is throttled.
	if (GetWorld()->GetTimeSeconds() - LastMoveRequestTime < HeldMoveInterval)
	{
		return;
	}

	FVector Destination;
	if (GetCursorWorldLocation(Destination))
	{
		RequestMoveTo(Destination);
	}
}

void AASPlayerController::OnStop()
{
	StopMovement();
}

void AASPlayerController::OnCameraLockToggled()
{
	if (AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager())
	{
		CameraManager->SetLockedToPawn(!CameraManager->IsLockedToPawn());
	}
}

void AASPlayerController::OnCameraCenterStarted()
{
	if (AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager())
	{
		CameraManager->SetCentering(true);
	}
}

void AASPlayerController::OnCameraCenterCompleted()
{
	if (AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager())
	{
		CameraManager->SetCentering(false);
	}
}

void AASPlayerController::UpdateCameraEdgeScroll(const float DeltaTime)
{
	AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager();
	if (!IsValid(CameraManager))
	{
		return;
	}

	// Fails while the cursor is outside the viewport, which also keeps the
	// camera still when the game window is not under the mouse.
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return;
	}

	FVector2D PanDirection = FVector2D::ZeroVector;
	if (MouseX <= EdgeScrollMargin)
	{
		PanDirection.X = -1.0;
	}
	else if (MouseX >= ViewportSizeX - EdgeScrollMargin)
	{
		PanDirection.X = 1.0;
	}

	// Viewport Y grows downwards.
	if (MouseY <= EdgeScrollMargin)
	{
		PanDirection.Y = 1.0;
	}
	else if (MouseY >= ViewportSizeY - EdgeScrollMargin)
	{
		PanDirection.Y = -1.0;
	}

	CameraManager->AddPanInput(PanDirection, DeltaTime);
}
