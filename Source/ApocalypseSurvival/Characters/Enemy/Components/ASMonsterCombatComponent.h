#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ASMonsterCombatComponent.generated.h"

class UASAbilitySystemComponent;

/**
 * 공격 사거리를 확인하고, 공격 Ability 의 활성화를 태그로 요청한다.
 *
 * 쿨다운, 비용, 동시 실행 제한은 여기서 판단하지 않는다. 그건 Ability 의 Cooldown/Cost
 * GameplayEffect 와 차단 태그가 맡고, 막히면 활성화가 실패해서 false 로 돌아온다.
 */
UCLASS(ClassGroup = (Enemy), BlueprintType, meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASMonsterCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASMonsterCombatComponent();

	void Configure(float InAttackRange, FGameplayTag InAttackAbilityTag);

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	float GetAttackRange() const { return AttackRange; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	FGameplayTag GetAttackAbilityTag() const { return AttackAbilityTag; }

	/** 두 캡슐 표면 사이의 수평 거리가 사거리 안이면 true. 높이 차이는 무시한다. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	bool IsTargetInAttackRange(const AActor* Target) const;

	/**
	 * 서버 전용. 타깃이 사거리 안이면 공격 Ability 활성화를 요청한다.
	 * 반환값은 Ability 가 실제로 시작됐는지 여부다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
	bool TryRequestAttack(AActor* Target);

private:
	UASAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (Categories = "Ability.Monster"))
	FGameplayTag AttackAbilityTag;
};
