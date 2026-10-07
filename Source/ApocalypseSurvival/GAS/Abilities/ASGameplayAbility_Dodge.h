#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ASGameplayAbility_Dodge.generated.h"

/**
 * 회피. 커서 방향으로 짧고 빠르게 이동한다.
 *
 * 팀의 첫 GAS 어빌리티 예시다. 비용(스태미나)과 쿨타임은 BP 자식에서 지정한 효과가 맡고,
 * 이 클래스는 방향을 정해서 움직이는 것만 한다.
 *
 * 방향은 커서를 보는 쪽(본인 클라이언트)만 안다. 서버는 커서 위치를 모르므로
 * 클라이언트가 방향을 타깃 데이터로 보내고, 서버는 그걸 받아 같은 방향으로 움직인다.
 * 둘 다 같은 루트 모션을 돌리므로 클라이언트는 누르자마자 움직이고 서버 결과와 어긋나지 않는다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASGameplayAbility_Dodge : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASGameplayAbility_Dodge();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility,
		bool bWasCancelled) override;

private:
	/** 본인 클라이언트(또는 호스트)에서 커서 방향을 구한다. 커서가 캐릭터 바로 위면 바라보는 방향을 쓴다. */
	bool TryGetCursorDirection(FVector& OutDirection) const;

	/** 서버가 클라이언트의 방향 데이터를 받았을 때. */
	void OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);

	/** 방향이 정해진 뒤 실제 이동. 클라이언트와 서버 양쪽에서 같은 인자로 불린다. */
	void PerformDodge(const FVector& Direction);

	UFUNCTION()
	void OnDodgeFinished();

	/** 이동 거리(cm). 임시값이며 기획 검증 대상이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0", Units = "cm"))
	float DodgeDistance = 400.0f;

	/** 이동에 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.01", Units = "s"))
	float DodgeDuration = 0.3f;
};
