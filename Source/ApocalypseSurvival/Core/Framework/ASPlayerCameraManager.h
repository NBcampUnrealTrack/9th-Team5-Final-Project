#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ASPlayerCameraManager.generated.h"

/**
 * 폰에 붙어 있지 않은 탑다운 카메라.
 *
 * 카메라는 바닥의 한 점(포커스)을 내려다본다. 포커스는 자유롭게 움직이거나,
 * 고정(Y)·복귀(스페이스) 상태일 때 폰을 따라간다.
 *
 * 시야각을 좁히고(40도) 카메라를 멀리 둔다. 넓은 시야각으로 가까이서 보면 화면
 * 가장자리의 캐릭터와 건물이 기울고 늘어져 보여서 거리와 범위를 재기 어렵다.
 *
 * 여기 있는 값은 전부 보는 사람의 화면에만 쓰인다. 복제하지 않는다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API AASPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AASPlayerCameraManager();

	/**
	 * 포커스를 바닥을 따라 민다.
	 * @param Direction  화면 기준 방향. X 가 오른쪽, Y 가 위쪽이다.
	 */
	void AddPanInput(const FVector2D& Direction, float DeltaTime);

	/** 고정 상태에서는 카메라가 폰을 계속 따라가고 AddPanInput 을 무시한다. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetLockedToPawn(bool bInLocked) { bLockedToPawn = bInLocked; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsLockedToPawn() const { return bLockedToPawn; }

	/** 켜져 있는 동안만 폰을 따라간다. 고정 상태는 바꾸지 않는다. */
	void SetCentering(bool bInCentering) { bCentering = bInCentering; }

	/**
	 * 카메라를 포커스 쪽으로 당기거나 민다.
	 * @param AxisValue  마우스 휠 칸 수. 양수가 확대다.
	 */
	void AddZoomInput(float AxisValue);

protected:
	virtual void PostInitializeComponents() override;

	/** 매 프레임 시점(위치·회전·시야각)을 계산한다. 엔진이 이 결과로 화면을 그린다. */
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;

private:
	bool IsFollowingPawn() const { return bLockedToPawn || bCentering; }

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "-10.0", Units = "deg"))
	float CameraPitch = -55.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (Units = "deg"))
	float CameraYaw = 0.0f;

	/**
	 * 플레이 시작 시 포커스에서 카메라까지의 거리.
	 *
	 * 시야각과 한 쌍으로 정한 값이다. 화면에 담기는 바닥의 폭은
	 * 2 × 거리 × tan(시야각 / 2) 이므로, 시야각을 바꾸면 거리도 같이 바꿔야
	 * 캐릭터가 같은 크기로 보인다. 40도에 3850 은 약 28m 폭이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float CameraDistance = 3850.0f;

	// ── 확대·축소 ─────────────────────────────────────────

	/** 가장 가까이 당겼을 때의 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "0.0", Units = "cm"))
	float MinCameraDistance = 1925.0f;

	/** 가장 멀리 밀었을 때의 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxCameraDistance = 3850.0f;

	/** 마우스 휠 한 칸에 움직이는 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "0.0", Units = "cm"))
	float ZoomStep = 385.0f;

	/** 요청한 확대 단계에 도달하는 빠르기. 0 이면 즉시 바뀐다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "0.0"))
	float ZoomInterpSpeed = 10.0f;

	// ── 이동 ──────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PanSpeed = 3000.0f;

	/** 자유 이동 중에 화면이 포커스를 따라잡는 빠르기. 0 이면 그대로 붙어 다닌다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float FocusInterpSpeed = 20.0f;

	// ── 실행 중 상태 ──────────────────────────────────────
	// "가려는 값"과 "지금 화면에 쓰는 값"을 따로 둔다. 뒤쪽이 앞쪽을 부드럽게 따라간다.

	bool bLockedToPawn = false;
	bool bCentering = false;

	/** 카메라가 가려는 바닥 지점. */
	FVector FocusLocation = FVector::ZeroVector;

	/** FocusLocation 을 부드럽게 따라가는 값. 실제로 카메라가 바라보는 지점이다. */
	FVector ViewFocusLocation = FVector::ZeroVector;

	/** 플레이어가 요청한 확대 단계. */
	float TargetCameraDistance = 0.0f;

	/** TargetCameraDistance 를 부드럽게 따라가는 값. 실제 시점을 만들 때 쓰는 거리다. */
	float ViewCameraDistance = 0.0f;

	/** 뷰 타깃이 바뀌었는지(첫 생성, 리스폰) 알아내려고 기억해 둔다. */
	TWeakObjectPtr<AActor> LastViewTarget;
};
