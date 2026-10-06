#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASPlayerController.generated.h"

class AASPlayerCameraManager;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;
class UPathFollowingComponent;
class UASItemInteractionComponent;
struct FInputActionValue;

/**
 * 클릭 이동 방식의 플레이어 컨트롤러.
 *
 * 이동 명령은 명령을 내린 머신(호스트 또는 본인 클라이언트)에서 길을 찾아 따라간다.
 * 서버 응답을 기다리지 않으므로 클릭에 바로 반응한다. 클라이언트에서도 길을 찾아야
 * 하므로 DefaultEngine.ini 의 bAllowClientSideNavigation 이 켜져 있어야 한다.
 *
 * 시점은 AASPlayerCameraManager 가 만든다. 캐릭터와 따로 움직이는 자유 카메라이고,
 * 이 컨트롤러가 화면 가장자리 스크롤과 휠 입력을 넘겨 준다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIAL_API AASPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AASPlayerController();

	/** 커서 아래로 트레이스해서 맞은 월드 위치를 돌려준다. 맞은 곳이 없으면 false. */
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool GetCursorWorldLocation(FVector& OutLocation) const;

	/** 조종 중인 폰을 Destination 에서 가장 가까운 이동 가능 지점으로 걸어가게 한다. */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void RequestMoveTo(const FVector& Destination);

	AASPlayerCameraManager* GetASPlayerCameraManager() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	UASItemInteractionComponent* GetItemInteractionComponent() const { return ItemInteractionComponent; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void SetupInputComponent() override;
	virtual void ReceivedPlayer() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	/**
	 * 조종하는 폰이 바뀌었을 때. 서버와 클라이언트 양쪽에서 불린다.
	 * 다이나믹 델리게이트에 연결하려면 UFUNCTION 이어야 한다.
	 */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

	void OnMoveStarted();
	void OnMoveHeld();
	void OnMoveCompleted();
	void OnStop();
	void OnCameraLockToggled();
	void OnCameraCenterStarted();
	void OnCameraCenterCompleted();
	void OnCameraZoom(const FInputActionValue& Value);

	/** 커서가 뷰포트 가장자리에 있는 동안 카메라를 그 방향으로 민다. */
	void UpdateCameraEdgeScroll(float DeltaTime);

	/** 조종 중인 폰이 내비메시 경로를 따라가게 한다. */
	UPROPERTY(VisibleDefaultsOnly, Category = "AI")
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASItemInteractionComponent> ItemInteractionComponent;

	// ── 입력 ──────────────────────────────────────────────
	// C++ 에서 에셋을 직접 참조하지 않는다. BP 자식에서 지정한다.

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 커서 위치로 이동. 누르고 있으면 커서를 계속 따라간다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** 진행 중인 이동 명령을 취소한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> StopAction;

	/** 자유 카메라와 폰을 따라가는 카메라를 전환한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraLockToggleAction;

	/** 누르고 있는 동안 카메라가 폰을 비춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraCenterAction;

	/** 카메라 확대·축소. 1D 축 입력이고 양수가 확대다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraZoomAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TEnumAsByte<ECollisionChannel> CursorTraceChannel = ECC_Visibility;

	/** 이동 키를 누르고 있을 때 이동 명령을 다시 내리는 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.0", Units = "s"))
	float HeldMoveInterval = 0.1f;

	/** 클릭한 위치에 로컬로만 생성하는 표시 이펙트. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UNiagaraSystem> MoveCursorFX;

	// ── 카메라 ────────────────────────────────────────────

	/** 가장자리 스크롤이 시작되는 뷰포트 테두리 폭(픽셀). */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float EdgeScrollMargin = 20.0f;

	/** 커서가 뷰포트 밖으로 이만큼(픽셀) 나가 있어도 가장자리에 있는 것으로 본다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float EdgeScrollOvershoot = 8.0f;

	// ── 이동 ──────────────────────────────────────────────

	/** 클릭한 지점이 내비메시에서 이만큼 떨어져 있어도 근처 지점을 찾아 걸어간다. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	FVector NavProjectExtent = FVector(400.0f, 400.0f, 500.0f);

	double LastMoveRequestTime = 0.0;
	bool bInteractionClickHeld = false;
};
