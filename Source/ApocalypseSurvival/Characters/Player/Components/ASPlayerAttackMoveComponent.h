#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "ASPlayerAttackMoveComponent.generated.h"

class AActor;
struct FASPrimaryAttackRequest;

DECLARE_MULTICAST_DELEGATE_OneParam(FASAttackMoveRequestDelegate, const FASPrimaryAttackRequest&);

/** 일반 공격 어빌리티가 소비하는 단발 공격 요청. */
USTRUCT()
struct FASPrimaryAttackRequest
{
	GENERATED_BODY()

	/** 타깃 공격이면 유효한 적, 방향성 투사체면 비어 있다. */
	TWeakObjectPtr<AActor> Target;

	/** 클릭한 월드 위치. 타깃이 없을 때 발사 방향을 계산하는 기준이다. */
	FVector TargetLocation = FVector::ZeroVector;

	/** Client Bubble과 서버가 공유하는 Mass replication ID. 0이면 Actor/비타겟 요청이다. */
	uint32 MassTargetNetworkId = 0;
};

UENUM()
enum class EASAttackMoveState : uint8
{
	Idle,
	Armed,
	Seeking,
	Chasing,
	Attacking
};

/**
 * A + 클릭 공격 명령을 로컬에서 해석한다.
 *
 * 타깃 모드에서는 적 탐색·추적·반복 공격을 담당하고, 방향성 투사체 모드에서는
 * 클릭 방향을 담은 단발 요청만 일반 공격 어빌리티에 전달한다. 피해·자원 소비는
 * 언제나 GAS와 서버 권한 공격 실행이 맡는다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASPlayerAttackMoveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASPlayerAttackMoveComponent();

	void ArmAttackMove();
	bool IsArmed() const { return State == EASAttackMoveState::Armed; }
	bool HasActiveCommand() const { return State != EASAttackMoveState::Idle; }
	/** 우클릭한 위치 근처의 보이는 적을 지정해 1회 공격한다(사거리 밖이면 다가감). 적이 없으면 false */
	bool StartTargetAttack(const FVector& CursorLocation);
	bool ConfirmAttackMove(const FVector& Destination);
	void CancelAttackMove();
	void NotifyExternalMoveRequested();
	void NotifyAttackFinished();

	/** 적을 사거리 안에서 발견했을 때 Controller가 일반 공격 Ability를 시작하도록 알린다. */
	FASAttackMoveRequestDelegate OnAttackRequested;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool UsesAttackMoveTargeting() const;
	float GetAttackRange() const;
	bool FindBestTarget(EASCombatTargetSortPolicy SortPolicy, float SearchRadius);
	bool RefreshCurrentTarget(FVector& OutLocation, float& OutHitRadius);
	bool IsTargetInAttackRange(const FVector& TargetLocation, float TargetHitRadius) const;
	void RequestAttack(AActor* Target, const FVector& TargetLocation);
	void UpdateTargetedCommand(double CurrentTime);
	void RequestMove(const FVector& Destination, double CurrentTime);

	UPROPERTY(EditDefaultsOnly, Category = "Attack Move", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumAcquireRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack Move", meta = (ClampMin = "0.01", Units = "s"))
	float SearchInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack Move", meta = (ClampMin = "0.01", Units = "s"))
	float RepathInterval = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack Move", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRangeTolerance = 15.0f;
	
	/** 우클릭으로 대상을 고르는 커서 주변 반경 (임시값) */
	UPROPERTY(EditDefaultsOnly, Category = "Attack Move", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetPickRadius = 80.0f;

	/** 우클릭으로 지정한 대상만 공격한다. 대상을 잃으면 다른 적을 찾지 않고 명령을 끝낸다 */
	bool bLockedTarget = false;

	UPROPERTY(EditDefaultsOnly, Category = "Attack Move", meta = (ClampMin = "0.0", Units = "cm"))
	float DestinationAcceptanceRadius = 50.0f;

	EASAttackMoveState State = EASAttackMoveState::Idle;
	FVector CommandDestination = FVector::ZeroVector;
	FASCombatTargetRef CurrentTarget;
	double NextSearchTime = 0.0;
	double NextRepathTime = 0.0;
};
