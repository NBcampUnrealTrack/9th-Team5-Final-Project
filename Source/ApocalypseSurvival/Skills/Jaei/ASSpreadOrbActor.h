#pragma once

#include "CoreMinimal.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "ASSpreadOrbActor.generated.h"

/*
 * 재이 E (1차): 퍼지는 염동 구체 (라이즈 E 느낌)
 * - 일자로 날아가다 첫 적(Actor/Mass)에 맞으면, 주변 SeekRadius 안의 다른 적 최대 SpreadCount명에게 작은 구체가 튕겨 감
 * - 작은 구체는 유도: 매 프레임 대상의 최신 위치로 방향을 꺾고, 닿으면 그 대상에게만 피해 (가는 길의 다른 적은 통과)
 * - 대상이 도중에 죽으면 작은 구체는 사라짐, 작은 구체는 다시 튕기지 않음 (1단계)
 * - 스킬은 기존 UASSkillProjectile 사용 (Projectile Class에 이 클래스의 BP 지정)
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASSpreadOrbActor : public AASSkillProjectileActor
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnTargetsHit(const FASCombatApplyResult& Result, const FVector& HitLocation) override;

	//튕겨 갈 작은 구체 클래스 (비우면 자기와 같은 클래스, 작은 구체용 BP를 따로 만들어 지정 가능)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb")
	TSubclassOf<AASSpreadOrbActor> SpreadOrbClass;

	//튕겨 갈 최대 대상 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb", meta = (ClampMin = "0"))
	int32 SpreadCount = 4;

	//작은 구체 피해 배율 (0.6 = 처음 구체의 60%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb", meta = (ClampMin = "0.0"))
	float SpreadDamageScale = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb", meta = (ClampMin = "1.0", Units = "cm/s"))
	float SpreadSpeed = 1500.0f;

	//맞은 지점에서 이 거리 안의 적에게 튕김
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb", meta = (ClampMin = "0.0", Units = "cm"))
	float SeekRadius = 600.0f;

	//true = 엘리트·보스(Actor)를 일반 Mass보다 먼저 고름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb")
	bool bPreferActorTargets = true;

	//대상을 찾을 때 찍어 볼 기준점 수 (많을수록 빽빽한 무리에서 서로 다른 적을 더 잘 찾음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SpreadSamplePoints = 12;

	//튕길 적이 SpreadCount보다 적으면 남는 구체를 사방으로 흩뿌림 (직선, 가는 길의 적을 맞힘)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb")
	bool bScatterWhenNoTarget = false;

	//흩뿌린 구체가 날아가는 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bScatterWhenNoTarget"))
	float ScatterRange = 500.0f;

	//튕길 대상과 방향을 디버그 선으로 표시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|SpreadOrb")
	bool bDrawDebug = false;

private:
	void Spread(const FVector& Origin);

	//Origin 주변에서 튕길 적 찾기 (처음 맞은 적 제외, 가까운 순)
	TArray<FASCombatTargetRef> FindSpreadTargets(const FVector& Origin) const;

	//작은 구체 생성. HomingTarget이 있으면 유도, 없으면 직선
	void SpawnChildOrb(const FVector& Origin, const FVector& Direction, const TArray<FGameplayEffectSpecHandle>& Specs,
		const FASCombatTargetRef* InHomingTarget);

	//유도 구체: 대상 쪽으로 꺾고, 닿으면 피해 (서버)
	void UpdateHoming();

	//Data.Damage만 배율을 곱한 복사본
	static FGameplayEffectSpecHandle MakeScaledSpec(const FGameplayEffectSpecHandle& Spec, float Scale);

	//튕겨서 생긴 구체 (다시 튕기지 않음)
	bool bIsSpreadChild = false;
	bool bHasSpread = false;

	//유도 대상 (비어 있으면 직선)
	FASCombatTargetRef HomingTarget;
	float HomingSpeed = 1500.0f;
};
