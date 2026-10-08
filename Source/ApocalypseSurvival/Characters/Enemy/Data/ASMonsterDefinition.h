#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ASMonsterDefinition.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UStateTree;

/** 일반 몬스터 원형의 초기 전투 수치. */
USTRUCT(BlueprintType)
struct FASMonsterInitialAttributes
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MoveSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0.0"))
	float AttackPower = 10.0f;
};

/**
 * 근접형, 원거리형 등의 차이를 Character 상속 대신 데이터로 정의한다.
 * StateTree는 이 자산을 읽어 타깃 거리와 활성화할 공격 Ability를 결정한다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASMonsterDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster")
	FASMonsterInitialAttributes InitialAttributes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	/** 공격 쿨다운은 여기에 두지 않는다. 공격 Ability 의 Cooldown GameplayEffect 가 맡는다. */

	/** 이 거리 안의 가장 가까운 살아 있는 플레이어를 타깃으로 잡는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float DetectionRadius = 1500.0f;

	/** 타깃이 이 거리보다 멀어지면 놓아준다. 탐지 반경보다 작게 설정해도 탐지 반경으로 보정된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float LoseTargetRadius = 2200.0f;

	/** CombatComponent가 활성화할 Ability의 태그. 예: Ability.Monster.Attack.Ranged */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Ability.Monster"))
	FGameplayTag AttackAbilityTag;

	/** 서버에서 이 몬스터 ASC에 한 번 부여할 기본 Ability 목록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	/** 피해를 입고 살아남았을 때 자신에게 적용할 짧은 경직 GameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reaction")
	TSubclassOf<UGameplayEffect> HitReactEffect;

	/** 사망 시 충돌을 끈 뒤 월드에 남겨 둘 시간. 0이면 즉시 제거한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0.0", Units = "s"))
	float DeathDespawnDelay = 1.5f;

	/** 몬스터의 전반 행동을 결정하는 StateTree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UStateTree> StateTree;
};
