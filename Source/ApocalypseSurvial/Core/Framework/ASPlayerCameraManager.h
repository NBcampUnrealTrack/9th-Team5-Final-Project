#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ASPlayerCameraManager.generated.h"

/**
 * Top-down camera that is not attached to the pawn.
 *
 * The view looks at a focus point on the ground. The focus point either pans
 * freely or follows the pawn while the camera is locked or being centered.
 * Everything here is local to the viewing player and is never replicated.
 */
UCLASS()
class APOCALYPSESURVIAL_API AASPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	/**
	 * Pans the focus point along the ground.
	 * @param Direction Screen-relative direction: X is right, Y is up.
	 */
	void AddPanInput(const FVector2D& Direction, float DeltaTime);

	/** Locked: the camera keeps following the pawn and ignores pan input. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetLockedToPawn(bool bInLocked) { bLockedToPawn = bInLocked; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsLockedToPawn() const { return bLockedToPawn; }

	/** Follows the pawn for as long as this is set, without changing the lock. */
	void SetCentering(bool bInCentering) { bCentering = bInCentering; }

protected:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;

private:
	bool IsFollowingPawn() const { return bLockedToPawn || bCentering; }

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "-10.0", Units = "deg"))
	float CameraPitch = -55.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (Units = "deg"))
	float CameraYaw = 0.0f;

	/** Distance from the focus point to the camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float CameraDistance = 1400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PanSpeed = 3000.0f;

	bool bLockedToPawn = false;
	bool bCentering = false;

	/** Ground point the camera looks at. */
	FVector FocusLocation = FVector::ZeroVector;

	/** Used to recenter when the view target changes, e.g. after a respawn. */
	TWeakObjectPtr<AActor> LastViewTarget;
};
