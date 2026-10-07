#pragma once

#include "CoreMinimal.h"
#include "Characters/ASCharacterBase.h"
#include "ASMonsterCharacter.generated.h"

class UASAbilitySystemComponent;
class UASCombatAttributeSet;
class UASHealthAttributeSet;
class UASMonsterCombatComponent;
class UASMonsterDefinition;
class UASMonsterTargetingComponent;
struct FOnAttributeChangeData;
struct FGameplayEffectSpec;
struct FActiveGameplayEffectHandle;
class UAbilitySystemComponent;

/**
 * 일반 몬스터 공통 Character.
 * 공격 종류는 자식 클래스가 아니라 MonsterDefinition, StateTree, GAS Ability 조합으로 결정한다.
 *
 * 두뇌(StateTree)는 이 클래스가 아니라 AASMonsterAIController 가 소유한다.
 * 이 클래스는 Definition 적용과 GAS 초기화를 끝낸 뒤 OnMonsterInitialized 로 알리기만 한다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API AASMonsterCharacter : public AASCharacterBase
{
	GENERATED_BODY()

public:
	AASMonsterCharacter();

	/** 서버에서 Definition 적용과 GAS 초기화가 끝난 직후 한 번 불린다. AI 컨트롤러가 이걸 기다린다. */
	FSimpleMulticastDelegate OnMonsterInitialized;

	/** 서버 전용. 클라이언트에서는 항상 false 다. */
	bool IsMonsterInitialized() const { return bMonsterInitialized; }

	/** State.Monster.Dead 태그로 판정한다. 태그가 복제되므로 모든 머신에서 맞다. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const;

	/**
	 * 서버 전용. true 면 몸이 이동 방향이 아니라 컨트롤러 시선(AIController 의 Focus)을 따라 돈다.
	 * 원거리 몬스터가 타깃을 보면서 움직일 때 쓴다.
	 */
	void SetFaceControllerFocus(bool bFaceControllerFocus);

	/** 서버 전용. 피해를 준 플레이어에게 즉시 어그로를 건다. */
	void NotifyDamagedBy(AActor* InstigatorActor);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UASMonsterDefinition* GetMonsterDefinition() const { return MonsterDefinition; }

	UFUNCTION(BlueprintPure, Category = "Enemy|GAS")
	UASAbilitySystemComponent* GetASAbilitySystemComponent() const { return OwnedAbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Components")
	UASMonsterTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Components")
	UASMonsterCombatComponent* GetCombatComponent() const { return CombatComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 자기 체력 변화를 구독한다. 베이스가 ASC 연결을 마친 뒤 모든 머신에서 불린다. */
	virtual void OnAbilitySystemInitialized() override;

	/** Definition 값을 Health/Combat AttributeSet과 컴포넌트에 적용하고 기본 Ability를 부여한다. */
	void ApplyMonsterDefinition();

	/**
	 * 서버 전용. 체력이 0 이하가 되면 한 번만 불린다.
	 * 죽음 태그만 붙이고 이동과 AI 정지는 StopMovementAndAI 로 넘긴다.
	 * 애니메이션, 충돌 해제, 소멸, 보상은 아직 없다.
	 */
	virtual void HandleDeath();

	/**
	 * 죽음 처리에서 이동과 AI 를 멈추는 진입점. 자식에서 늘릴 수 있다.
	 *
	 * 죽음은 StateTree 가 아니라 Character/GAS 생명주기가 처리한다. 체력이 0 이 되면 여기서
	 * AIController 의 StopLogic 으로 트리를 끝낸다. 그러므로 초기 StateTree 에는 Dead 상태를
	 * 만들지 않는다. 만들어도 트리가 먼저 멈추기 때문에 실행되지 않는다.
	 */
	virtual void StopMovementAndAI();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UASMonsterDefinition> MonsterDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASAbilitySystemComponent> OwnedAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASHealthAttributeSet> OwnedHealthAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASCombatAttributeSet> OwnedCombatAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

private:
	void UnbindHealthDelegate();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void UnbindDamageEffectDelegate();
	void HandleGameplayEffectApplied(UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayEffectSpec& AppliedEffect, FActiveGameplayEffectHandle ActiveEffectHandle);

	bool bMonsterInitialized = false;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle DamageEffectAppliedHandle;
};
