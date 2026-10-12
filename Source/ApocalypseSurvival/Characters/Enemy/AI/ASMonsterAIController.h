#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ASMonsterAIController.generated.h"

class AASMonsterCharacter;
class UASMonsterCombatComponent;
class UASMonsterTargetingComponent;
class UStateTreeAIComponent;

/**
 * 일반 몬스터의 두뇌. StateTree 를 소유하고 서버에서만 돌린다.
 *
 * StateTree 는 빙의되자마자 시작하지 않는다. 몬스터가 MonsterDefinition 적용과 GAS 초기화를
 * 끝냈다고 알린 뒤에 시작한다. 그 전에 시작하면 StateTree 가 아직 없는 Ability, 아직 맞춰지지
 * 않은 능력치를 읽게 된다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API AASMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AASMonsterAIController();

	/** 팀은 조종하는 몬스터의 것을 그대로 쓴다. 빙의 전에는 NoTeam. */
	virtual FGenericTeamId GetGenericTeamId() const override;

	UFUNCTION(BlueprintPure, Category = "AI")
	UStateTreeAIComponent* GetStateTreeAIComponent() const { return StateTreeAIComponent; }

	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsAILogicRunning() const;

	/** 서버 전용. StateTree 를 멈추고 이동과 시선 고정을 푼다. 죽음 처리의 진입점이다. */
	void StopAI(const FString& Reason);

	/**
	 * 서버 전용. 타깃을 바라보게 한다. 이동 방향과 상관없이 컨트롤러 시선을 따라 몸이 돈다.
	 * 원거리 몬스터가 물러나면서 타깃을 향하는 데 쓴다.
	 */
	void SetCombatFocus(AActor* Target);

	/** 서버 전용. 시선 고정을 풀고 다시 이동 방향을 바라보게 한다. */
	void ClearCombatFocus();

	/** 서버 전용. 플레이어에게 피해를 받았을 때 공격자에게 어그로를 부여한다. */
	void NotifyMonsterDamaged(AActor* InstigatorActor, float AggroAmount);

	/** 서버 전용. 피격 경직이 시작됐을 때 현재 경로 이동과 시선 고정을 멈춘다. */
	void NotifyMonsterHitReact();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	/** 조건이 갖춰졌을 때만 StateTree 를 시작한다. 여러 번 불려도 안전하다. */
	void TryStartAI();
	void StartStateTree();
	void StopStateTree(const FString& Reason);

	/** 낮/밤 전역 규칙과 실제 추격 이동을 StateTree 밖에서 갱신한다. */
	void StartAggroLoop();
	void StopAggroLoop();
	void UpdateAggroAndMovement();
	bool UpdateReturnToSpawn(AASMonsterCharacter& Monster, UASMonsterTargetingComponent& Targeting);
	void RestoreReturningHealth(AASMonsterCharacter& Monster) const;
	void ApplyReturnMoveSpeed(AASMonsterCharacter& Monster);
	void RestoreMoveSpeed();
	void HandleNightStarted(int32 Day, bool bBossNight);
	void HandleNightEnded(int32 Day);

	void StopWaitingForMonster();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	TWeakObjectPtr<AASMonsterCharacter> ControlledMonster;

	FDelegateHandle MonsterInitializedHandle;
	FDelegateHandle NightStartedHandle;
	FDelegateHandle NightEndedHandle;

	FTimerHandle AggroUpdateTimerHandle;
	TWeakObjectPtr<AActor> MoveTarget;

	/** 밤에는 거리와 무관하게 즉시 플레이어를 추격한다. */
	bool bNightAggressive = false;

	/** 밤 종료 시점에 이미 잡고 있던 타깃은 낮이 되어도 유실되기 전까지 계속 추격한다. */
	bool bContinueTrackingNightTarget = false;

	/** 홈으로 복귀 중일 때는 타깃 탐색·공격을 중단한다. */
	bool bReturningToSpawn = false;

	/** 낮에 실제 타깃을 잡고 추격을 시작했는지. 타깃 유실 후 귀환 여부를 결정한다. */
	bool bHasEngagedDayTarget = false;

	/** 귀환 중에만 속도를 올리고, 어떤 종료 경로에서도 원래 값으로 되돌린다. */
	bool bHasCachedMoveSpeedForReturn = false;
	float CachedMoveSpeedForReturn = 0.0f;

	/** 타깃 검증·재탐색 주기. 개별 몬스터가 매 프레임 플레이어를 훑지 않게 한다. */
	float AggroUpdateInterval = 0.25f;
};
