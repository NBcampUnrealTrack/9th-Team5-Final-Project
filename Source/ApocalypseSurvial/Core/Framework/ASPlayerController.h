#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASPlayerController.generated.h"

class AASPlayerCameraManager;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;
class UPathFollowingComponent;

/**
 * Point-and-click player controller.
 *
 * A move command path-follows on the machine that issued it (the host or the
 * owning client), so clicks respond without waiting for the server. This
 * requires bAllowClientSideNavigation in DefaultEngine.ini.
 *
 * The view comes from AASPlayerCameraManager, a free camera that this
 * controller pans by edge scrolling.
 */
UCLASS(Abstract)
class APOCALYPSESURVIAL_API AASPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AASPlayerController();

	/** Traces under the cursor and returns the world location that was hit. */
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool GetCursorWorldLocation(FVector& OutLocation) const;

	/** Walks the controlled pawn to the reachable point closest to Destination. */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void RequestMoveTo(const FVector& Destination);

	AASPlayerCameraManager* GetASPlayerCameraManager() const;

protected:
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void OnMoveStarted();
	void OnMoveHeld();
	void OnStop();
	void OnCameraLockToggled();
	void OnCameraCenterStarted();
	void OnCameraCenterCompleted();

	/** Pans the camera while the cursor rests against a viewport edge. */
	void UpdateCameraEdgeScroll(float DeltaTime);

	/** Follows NavMesh paths for the controlled pawn. */
	UPROPERTY(VisibleDefaultsOnly, Category = "AI")
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Move to the cursor. Holding the key keeps following the cursor. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Cancels the current move command. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> StopAction;

	/** Toggles between a free camera and one that follows the pawn. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraLockToggleAction;

	/** Keeps the camera on the pawn while held. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraCenterAction;

	/** Width of the viewport border that triggers edge scrolling. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float EdgeScrollMargin = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TEnumAsByte<ECollisionChannel> CursorTraceChannel = ECC_Visibility;

	/** Seconds between move commands while the move key is held. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.0", Units = "s"))
	float HeldMoveInterval = 0.1f;

	/** How far a clicked point may be from the NavMesh and still be walked to. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	FVector NavProjectExtent = FVector(400.0f, 400.0f, 500.0f);

	/** Spawned locally at the clicked location. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UNiagaraSystem> MoveCursorFX;

	double LastMoveRequestTime = 0.0;
};
