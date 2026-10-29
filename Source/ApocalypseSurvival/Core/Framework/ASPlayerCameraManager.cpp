#include "Core/Framework/ASPlayerCameraManager.h"

AASPlayerCameraManager::AASPlayerCameraManager()
{
	// 엔진 기본값은 90 이다. CameraDistance 와 한 쌍이므로 한쪽만 바꾸지 말 것.
	DefaultFOV = 40.0f;
}

bool AASPlayerCameraManager::IsSpectating() const
{
	// 빙의 전(폰 없음)에는 뷰 타깃이 컨트롤러 자신이라 관전으로 보지 않는다.
	const APawn* OwnPawn = PCOwner ? PCOwner->GetPawn() : nullptr;
	const AActor* CurrentTarget = GetViewTarget();
	return OwnPawn && CurrentTarget && CurrentTarget != OwnPawn;
}

void AASPlayerCameraManager::AddPanInput(const FVector2D& Direction, const float DeltaTime)
{
	if (IsFollowingPawn() || IsSpectating() || Direction.IsNearlyZero())
	{
		return;
	}

	// 화면 기준 방향을 월드 방향으로 바꾼다. 카메라가 Yaw 만큼 돌아 있으므로
	// 화면 위쪽은 그만큼 돌린 X 축, 화면 오른쪽은 그만큼 돌린 Y 축이다.
	const FRotationMatrix YawMatrix(FRotator(0.0f, CameraYaw, 0.0f));
	const FVector ScreenUp = YawMatrix.GetUnitAxis(EAxis::X);
	const FVector ScreenRight = YawMatrix.GetUnitAxis(EAxis::Y);

	// 정규화하지 않으면 대각선(모서리)으로 갈 때 1.41배 빨라진다.
	const FVector2D PanDirection = Direction.GetSafeNormal();

	FocusLocation += (ScreenRight * PanDirection.X + ScreenUp * PanDirection.Y) * PanSpeed * DeltaTime;
}

void AASPlayerCameraManager::AddZoomInput(const float AxisValue)
{
	// 확대(양수)는 거리를 줄이는 것이라 뺀다.
	const float RequestedDistance = TargetCameraDistance - AxisValue * ZoomStep;
	TargetCameraDistance = bDebugZoomUnlocked
		? FMath::Max(1.0f, RequestedDistance)
		: FMath::Clamp(RequestedDistance, MinCameraDistance, MaxCameraDistance);
}

void AASPlayerCameraManager::SetDebugZoomUnlocked(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
	bDebugZoomUnlocked = bEnabled;
	if (!bEnabled)
	{
		TargetCameraDistance = FMath::Clamp(TargetCameraDistance, MinCameraDistance, MaxCameraDistance);
		ViewCameraDistance = FMath::Clamp(ViewCameraDistance, MinCameraDistance, MaxCameraDistance);
	}
#endif
}

float AASPlayerCameraManager::GetZoomAlpha() const
{
	if (FMath::IsNearlyEqual(MinCameraDistance, MaxCameraDistance))
	{
		return 0.0f;
	}

	return FMath::Clamp(
		(ViewCameraDistance - MinCameraDistance) / (MaxCameraDistance - MinCameraDistance),
		0.0f,
		1.0f);
}

void AASPlayerCameraManager::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 생성자가 아니라 여기서 넣는다. 생성자 시점에는 BP 에서 바꾼 값이 아직 반영되지 않았다.
	TargetCameraDistance = FMath::Clamp(CameraDistance, MinCameraDistance, MaxCameraDistance);
	ViewCameraDistance = TargetCameraDistance;
	LastBroadcastZoomAlpha = GetZoomAlpha();
}

void AASPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, const float DeltaTime)
{
	if (bBossIntroActive)
	{
		OutVT.POV = BossIntroView;
		return;
	}
	// 뷰 타깃은 보통 조종 중인 폰이다. 엔진이 빙의할 때 자동으로 지정한다.
	const AActor* Target = OutVT.Target;
	const bool bTargetChanged = LastViewTarget.Get() != Target;
	if (IsValid(Target))
	{
		const FVector TargetLocation = Target->GetActorLocation();
		if (IsFollowingPawn() || IsSpectating() || bTargetChanged)
		{
			FocusLocation = TargetLocation;
		}
		else
		{
			// 자유 카메라도 높이만은 폰을 따라간다. 경사나 계단에서
			// 화면 구도가 달라지지 않게 하기 위해서다.
			FocusLocation.Z = TargetLocation.Z;
		}
	}
	LastViewTarget = OutVT.Target;

	// 부드럽게 따라가는 것은 자유 이동뿐이다. 출발과 정지가 급하지 않게 하려는 것이다.
	// 폰을 따라가는 중(고정, 복귀)과 뷰 타깃이 바뀐 순간(첫 생성, 리스폰)은
	// 곧바로 그 위치를 비춘다.
	if (bTargetChanged || IsFollowingPawn() || IsSpectating() || FocusInterpSpeed <= 0.0f)
	{
		ViewFocusLocation = FocusLocation;
	}
	else
	{
		ViewFocusLocation = FMath::VInterpTo(ViewFocusLocation, FocusLocation, DeltaTime, FocusInterpSpeed);
	}

	ViewCameraDistance = FMath::FInterpTo(ViewCameraDistance, TargetCameraDistance, DeltaTime, ZoomInterpSpeed);
	const float ZoomAlpha = GetZoomAlpha();
	if (!FMath::IsNearlyEqual(ZoomAlpha, LastBroadcastZoomAlpha, 0.001f))
	{
		LastBroadcastZoomAlpha = ZoomAlpha;
		OnCameraZoomChanged.Broadcast(ZoomAlpha);
	}

	// 포커스에서 "카메라가 보는 방향의 반대"로 거리만큼 물러난 곳이 카메라 위치다.
	const FRotator CameraRotation(CameraPitch, CameraYaw, 0.0f);
	OutVT.POV.Location = ViewFocusLocation - CameraRotation.Vector() * ViewCameraDistance;
	OutVT.POV.Rotation = CameraRotation;
	OutVT.POV.FOV = DefaultFOV;
}

void AASPlayerCameraManager::BeginBossIntro(const FVector& BossLocation)
{
	if (bBossIntroActive) return;
	SavedBossIntroView = GetCameraCacheView();
	BossIntroView = SavedBossIntroView;
	BossIntroCameraLocation = BossLocation - SavedBossIntroView.Rotation.Vector() * ViewCameraDistance;
	bBossIntroActive = true;
}

void AASPlayerCameraManager::SetBossIntroBlend(float Alpha)
{
	BossIntroView.Location = FMath::Lerp(SavedBossIntroView.Location, BossIntroCameraLocation,
		FMath::SmoothStep(0.0f, 1.0f, Alpha));
	UpdateCamera(0.0f);
}

void AASPlayerCameraManager::EndBossIntro()
{
	if (!bBossIntroActive) return;
	SetBossIntroBlend(0.0f);
	bBossIntroActive = false;
}
