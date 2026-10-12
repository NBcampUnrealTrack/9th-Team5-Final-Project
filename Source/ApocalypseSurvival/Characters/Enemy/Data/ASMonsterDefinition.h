#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ASMonsterDefinition.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UASMonsterHealthBarWidget;
class UStateTree;

/** 몬스터가 상황에 따라 선택해서 실행하는 공격 패턴 하나. */
USTRUCT(BlueprintType)
struct FASMonsterAttackPattern
{
	GENERATED_BODY()

	/** 패턴 식별자. 쿨다운 추적과 디버그 로그에 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (Categories = "Ability.Monster.Attack"))
	FGameplayTag PatternId;

	/** 이 패턴이 선택됐을 때 서버가 부여·활성화할 Ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "cm"))
	float MinRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRange = 200.0f;

	/** 사용 가능한 패턴끼리 선택할 때의 상대 확률. 0이면 선택 후보에서 제외된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.0f;

	/** 동일 패턴을 다시 선택할 수 있기까지의 시간. Ability 활성화 성공 시점부터 잰다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 1.0f;
};

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

	/** 피해를 받았을 때 같은 공격자에게 어그로를 전파할 주변 몬스터 탐색 반경. 0이면 전파하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float AssistAggroRadius = 900.0f;

	/** 직접 피해를 받았을 때 공격자에게 부여할 어그로. 최대치는 100이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
	float DirectHitAggro = 100.0f;

	/** 타깃이 추적 해제 거리 밖에 있을 때 초당 잃는 어그로. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
	float AggroDecayPerSecond = 25.0f;

	/** CombatComponent가 활성화할 Ability의 태그. 예: Ability.Monster.Attack.Ranged */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Ability.Monster"))
	FGameplayTag AttackAbilityTag;

	/** 서버에서 이 몬스터 ASC에 한 번 부여할 기본 Ability 목록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	/**
	 * 엘리트·보스가 상황에 따라 선택하는 공격 목록. 비어 있으면 기존 AttackAbilityTag 단일 공격을 쓴다.
	 * 여기의 AbilityClass는 GrantedAbilities에 중복해서 넣지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (TitleProperty = "PatternId"))
	TArray<FASMonsterAttackPattern> AttackPatterns;

	/** 피해를 입고 살아남았을 때 자신에게 적용할 짧은 경직 GameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reaction")
	TSubclassOf<UGameplayEffect> HitReactEffect;

	/** 사망 시 충돌을 끈 뒤 월드에 남겨 둘 시간. 0이면 즉시 제거한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0.0", Units = "s"))
	float DeathDespawnDelay = 1.5f;

	/** 이 몬스터 원형이 머리 위 월드 체력바를 사용할지 결정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	bool bUseWorldHealthBar = false;

	/** 생성 직후부터 체력바를 표시한다. 꺼 두면 전투 진입 시 SetHealthBarVisible로 켤 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides))
	bool bHealthBarInitiallyVisible = true;

	/** 비어 있으면 코드에서 제공하는 기본 체력바 Widget을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides))
	TSubclassOf<UASMonsterHealthBarWidget> HealthBarWidgetClass;

	/** Character 원점에서 체력바까지의 상대 위치. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides, Units = "cm"))
	FVector HealthBarOffset = FVector(0.0f, 0.0f, 140.0f);

	/** Widget의 DesiredSize가 계산되기 전 첫 프레임에 사용하는 안전한 초기 크기. 이후에는 WBP 크기가 우선한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides, ClampMin = "1.0"))
	FVector2D HealthBarDrawSize = FVector2D(250.0f, 30.0f);

	/** 줌아웃할수록 체력바를 제한적으로 축소한다. 몬스터별 Tick은 사용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides))
	bool bScaleHealthBarWithCameraZoom = true;

	/** 카메라가 가장 가까울 때의 체력바 크기 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "bUseWorldHealthBar && bScaleHealthBarWithCameraZoom", EditConditionHides, ClampMin = "0.1"))
	float HealthBarScaleAtMinZoomDistance = 1.0f;

	/** 카메라가 가장 멀 때의 체력바 크기 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
		meta = (EditCondition = "bUseWorldHealthBar && bScaleHealthBarWithCameraZoom", EditConditionHides, ClampMin = "0.1"))
	float HealthBarScaleAtMaxZoomDistance = 0.75f;

	/** 몬스터의 전반 행동을 결정하는 StateTree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UStateTree> StateTree;
};
