#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ASMonsterAIController.generated.h"

class AASMonsterCharacter;
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

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	/** 조건이 갖춰졌을 때만 StateTree 를 시작한다. 여러 번 불려도 안전하다. */
	void TryStartAI();

	void StopWaitingForMonster();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	TWeakObjectPtr<AASMonsterCharacter> ControlledMonster;

	FDelegateHandle MonsterInitializedHandle;
};
