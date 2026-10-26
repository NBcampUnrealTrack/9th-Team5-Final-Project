#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "ASPlayerController.generated.h"

class AASPlayerCameraManager;
class UASAbilitySystemComponent;
class UASPlayerAttackMoveComponent;
class UASInputConfig;
class UASInventoryComponent;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;
class UPathFollowingComponent;
class UASItemInteractionComponent;
class AActor;
struct FInputActionValue;
struct FASPrimaryAttackRequest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASLocalPawnChangedSignature, APawn*, NewPawn);

/**
 * 클릭 이동 방식의 플레이어 컨트롤러.
 *
 * 이동 명령은 명령을 내린 머신(호스트 또는 본인 클라이언트)에서 길을 찾아 따라간다.
 * 서버 응답을 기다리지 않으므로 클릭에 바로 반응한다. 클라이언트에서도 길을 찾아야
 * 하므로 DefaultEngine.ini 의 bAllowClientSideNavigation 이 켜져 있어야 한다.
 *
 * 시점은 AASPlayerCameraManager 가 만든다. 캐릭터와 따로 움직이는 자유 카메라이고,
 * 이 컨트롤러가 화면 가장자리 스크롤과 휠 입력을 넘겨 준다.
 *
 * 어빌리티 입력(공격, 회피, QWER)은 키마다 함수를 두지 않는다. UASInputConfig 에 적힌
 * "키 → 입력 태그" 대로 핸들러를 걸고, 눌리면 태그만 폰의 ASC 에 넘긴다.
 *
 * 인벤토리도 여기에 붙는다. 캐릭터가 죽어도 남고, 본인에게만 복제되기 때문이다. Lyra 와 같다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API AASPlayerController : public APlayerController
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

	UFUNCTION(BlueprintPure, Category = "Item")
	UASInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	UASPlayerAttackMoveComponent* GetAttackMoveComponent() const { return AttackMoveComponent; }

	/** 실제 시전이 확정된 어빌리티가 기존 클릭 이동·공격 이동을 넘겨받는다. */
	void CancelMovementForCommittedAbility();

	/** 공격 이동이 준비한 요청을 Primary Attack Ability가 한 번 소비한다. */
	bool ConsumeAttackMoveRequest(AActor*& OutTarget, FVector& OutTargetLocation,
		uint32& OutMassTargetNetworkId);

	/**
	 * 로컬 플레이어가 조종하는 폰이 바뀌었을 때. 이 머신의 화면을 가진 컨트롤러에서만 방송한다.
	 * HUD 가 새 폰의 ASC 로 다시 연결할 때 쓴다. 빙의가 풀리면 NewPawn 은 null 이다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Pawn")
	FASLocalPawnChangedSignature OnLocalPawnChanged;

	/**
	 * 디버그. 장착 UI 가 생기기 전까지 콘솔로 장착을 시험한다.
	 * 인벤토리의 첫 번째 무기를 장착한다. 클라이언트에서 쳐도 서버 RPC 로 전달된다.
	 */
	UFUNCTION(Exec)
	void ASEquipWeapon();

	UFUNCTION(Exec)
	void ASUnequipWeapon();

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

	/** 어빌리티 입력. 태그는 AbilityInputConfig 의 항목에서 바인딩할 때 같이 넘어온다. */
	void OnAbilityInputPressed(FGameplayTag InputTag);
	void OnAbilityInputReleased(FGameplayTag InputTag);

	/** 조종 중인 폰이 쓰는 ASC. 플레이어는 PlayerState 의 것이다. 폰이 없으면 null. */
	UASAbilitySystemComponent* GetPawnAbilitySystemComponent() const;
	void HandleAttackMoveRequest(const FASPrimaryAttackRequest& Request);
	void TryActivatePrimaryAttack();

	/** 조종 중인 폰이 죽어 있는가. 죽은 동안은 클릭 이동과 상호작용을 받지 않는다. 어빌리티 입력은 ASC 가 막는다. */
	bool IsPawnDead() const;

	/** 커서가 뷰포트 가장자리에 있는 동안 카메라를 그 방향으로 민다. */
	void UpdateCameraEdgeScroll(float DeltaTime);

	/** 조종 중인 폰이 내비메시 경로를 따라가게 한다. */
	UPROPERTY(VisibleDefaultsOnly, Category = "AI")
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASItemInteractionComponent> ItemInteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASInventoryComponent> InventoryComponent;

	/** A + 클릭 명령을 해석한다. 공격 피해·비용은 GAS가 계속 담당한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASPlayerAttackMoveComponent> AttackMoveComponent;

	// ── 입력 ──────────────────────────────────────────────
	// C++ 에서 에셋을 직접 참조하지 않는다. BP 자식에서 지정한다.

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 어빌리티용 키 목록. 이동·카메라 입력은 아래 액션들이 따로 맡는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UASInputConfig> AbilityInputConfig;

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
	TWeakObjectPtr<AActor> PendingAttackMoveTarget;
	FVector PendingAttackMoveTargetLocation = FVector::ZeroVector;
	uint32 PendingAttackMoveMassTargetNetworkId = 0;
	bool bHasPendingAttackMoveRequest = false;
};
