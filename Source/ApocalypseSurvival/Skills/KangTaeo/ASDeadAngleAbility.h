#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "ASDeadAngleAbility.generated.h"

class UGameplayEffect;

/*
 * 강태오 패시브 데드 앵글: 가까운 적이 N명 이상이면 받는 피해 감소
 * 스킬바 E 슬롯에 표시 (UASSkillAbility 상속 - SkillData 아이콘 사용), 키를 눌러도 아무 일 없음
 * 부여되자마자 스스로 켜지고 계속 유지 (부모의 조준·시전 흐름은 사용 안 함)
 * 주기적으로 주변 적 수를 세서 ReductionEffect(Infinite)를 붙였다 뗐다 함
 * 1차: Actor 몬스터만 셈 (Mass 몬스터는 셀 함수가 생기면 추가)
 * 나중: 회피 직후 1.5초 반격 (회피 종료 이벤트 필요)
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASDeadAngleAbility : public UASSkillAbility
{
	GENERATED_BODY()

public:
	UASDeadAngleAbility();

	//부여되거나 캐릭터가 바뀌면 바로 켜기
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	//패시브 - 키 입력 무시 (부모의 조준 취소·시전 확정 막기)
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override {}

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override {}

protected:
	//감지 반경 (SkillData가 없을 때, 있으면 DT의 AreaRadius 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|DeadAngle", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 300.0f;

	//이 수 이상이면 발동
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|DeadAngle", meta = (ClampMin = "1"))
	int32 RequiredEnemyCount = 3;

	//검사 주기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|DeadAngle", meta = (ClampMin = "0.05", Units = "s"))
	float CheckInterval = 0.25f;

	//피해 감소 GE (Infinite, Modifier: DamageReduction Add 0.2, Grant Tags: State.Taeo.DeadAngle)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Taeo|DeadAngle")
	TSubclassOf<UGameplayEffect> ReductionEffect;


private:
	//주기 검사: 적 수 세고 GE 켜기/끄기
	void UpdateDeadAngle();

	//반경 안 적 수 (지금은 Actor만)
	int32 CountNearbyEnemies(float SearchRadius) const;

	float GetSearchRadius() const;

	void RemoveReductionEffect();

	FTimerHandle CheckTimerHandle;

	//붙어 있는 피해 감소 GE (없으면 Invalid)
	FActiveGameplayEffectHandle ReductionEffectHandle;
};
