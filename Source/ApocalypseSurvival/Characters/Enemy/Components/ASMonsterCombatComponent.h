#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "GameplayAbilitySpec.h"
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

	/** 패턴 Definition과 그 순서에 대응하는 Ability Spec Handle을 등록한다. */
	void ConfigurePatterns(const TArray<FASMonsterAttackPattern>& InPatterns, const TArray<FGameplayAbilitySpecHandle>& InAbilityHandles);

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	float GetAttackRange() const { return AttackRange; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	FGameplayTag GetAttackAbilityTag() const { return AttackAbilityTag; }

	/** 패턴 목록을 사용 중인지. 비어 있으면 기존 단일 공격 경로를 유지한다. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	bool UsesAttackPatterns() const { return !AttackPatterns.IsEmpty(); }

	/** 두 캡슐 표면 사이의 수평 거리가 사거리 안이면 true. 높이 차이는 무시한다. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	bool IsTargetInAttackRange(const AActor* Target) const;

	/** 현재 거리·패턴 쿨다운·공용 후딜 기준으로 공격을 요청할 수 있는지 검사한다. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	bool CanRequestAttack(const AActor* Target) const;

	/**
	 * 서버 전용. 타깃이 사거리 안이면 공격 Ability 활성화를 요청한다.
	 * 반환값은 Ability 가 실제로 시작됐는지 여부다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
	bool TryRequestAttack(AActor* Target);

	/** 패턴 Ability가 아직 실행 중인지. 즉발 패턴은 활성화 직후 false가 될 수 있다. */
	bool IsPatternAttackActive() const;

	/** StateTree가 패턴 Ability 종료를 확인했을 때 호출한다. 공용 후딜은 이 시점부터 시작한다. */
	void NotifyPatternAttackFinished();

private:
	UASAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	bool IsPatternInRange(const FASMonsterAttackPattern& Pattern, const AActor* Target) const;
	bool IsPatternReady(int32 PatternIndex, double CurrentTime) const;
	bool TryRequestPatternAttack(AActor* Target);
	double GetWorldTimeSeconds() const;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (Categories = "Ability.Monster"))
	FGameplayTag AttackAbilityTag;

	/** 하나의 패턴이 끝난 뒤, 다른 패턴을 포함해 다음 공격까지 기다리는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float PostAttackRecovery = 0.4f;

	struct FPatternRuntime
	{
		FGameplayAbilitySpecHandle AbilityHandle;
		double CooldownEndTime = 0.0;
	};

	TArray<FASMonsterAttackPattern> AttackPatterns;
	TArray<FPatternRuntime> PatternRuntime;
	double NextAttackAllowedTime = 0.0;
	bool bWaitingForPatternAttackEnd = false;
};
