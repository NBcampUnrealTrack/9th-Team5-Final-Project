#include "Core/Framework/ASPlayerCameraManager.h"

void AASPlayerCameraManager::AddPanInput(const FVector2D& Direction, const float DeltaTime)
{
	if (IsFollowingPawn() || Direction.IsNearlyZero())
	{
		return;
	}

	const FRotationMatrix YawMatrix(FRotator(0.0f, CameraYaw, 0.0f));
	const FVector ScreenUp = YawMatrix.GetUnitAxis(EAxis::X);
	const FVector ScreenRight = YawMatrix.GetUnitAxis(EAxis::Y);
	const FVector2D PanDirection = Direction.GetSafeNormal();

	FocusLocation += (ScreenRight * PanDirection.X + ScreenUp * PanDirection.Y) * PanSpeed * DeltaTime;
}

void AASPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, const float DeltaTime)
{
	const AActor* Target = OutVT.Target;
	if (IsValid(Target))
	{
		const FVector TargetLocation = Target->GetActorLocation();
		if (IsFollowingPawn() || LastViewTarget.Get() != Target)
		{
			FocusLocation = TargetLocation;
		}
		else
		{
			// A free camera still tracks the pawn's height so slopes and
			// stairs do not change the framing.
			FocusLocation.Z = TargetLocation.Z;
		}
	}
	LastViewTarget = OutVT.Target;

	const FRotator CameraRotation(CameraPitch, CameraYaw, 0.0f);
	OutVT.POV.Location = FocusLocation - CameraRotation.Vector() * CameraDistance;
	OutVT.POV.Rotation = CameraRotation;
	OutVT.POV.FOV = DefaultFOV;
}
