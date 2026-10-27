#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Skills/Area/ASSkillArea.h"
#include "ASSkillZeroGravity.generated.h"

/*
 * 재이 R 제로 그래비티 존 (1차)
 * - 커서 위치에 Duration(기본 6초) 동안 장판, TickInterval마다 피해 + AdditionalEffects(엘리트·보스 슬로우 등)
 * - 장판 안 Actor 적마다 StunRollInterval 간격으로 최대 StunRollCount번 StunChance 확률로 스턴 (StunEffect를 비우면 스턴 없음)
 * - 장판이 끝나면 중심 폭발, 안에 있던 적 수만큼 피해·반경 증가
 * - 장판 액터는 AreaClass에 AASZeroGravityAreaActor(또는 그 BP) 지정
 * 1차 미구현: 공중에 띄우기, 투사체 정지, 재사용으로 조기 폭발, 지정 지점 추락, Mass 스턴
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASSkillZeroGravity : public UASSkillArea
{
	GENERATED_BODY()

public:
	UASSkillZeroGravity();

protected:
	virtual void ConfigureArea(AASSkillAreaActor* Area) override;

	//스턴 GE (Has Duration, Granted Tag = State.Monster.HitReact). 비우면 스턴 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity")
	TSubclassOf<UGameplayEffect> StunEffect;

	//판정 1번당 스턴 확률 0~1 (커브 테이블을 연결하면 스킬 레벨별 값, 예: 1레벨 0.3 / 강화 0.5)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity")
	FScalableFloat StunChance = FScalableFloat(0.3f);

	//대상 1명당 최대 판정 횟수 (처음 닿은 순간이 1번째, 성공하면 그 대상은 끝)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity", meta = (ClampMin = "0"))
	int32 StunRollCount = 2;

	//같은 대상의 다음 판정까지 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity", meta = (ClampMin = "0.0", Units = "s"))
	float StunRollInterval = 2.0f;

	//종료 폭발 기본 피해 = 1회 피해 x 이 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity", meta = (ClampMin = "0.0"))
	float BurstDamageScale = 2.0f;

	//장판 안 적 1명당 폭발 피해 증가 (0.1 = +10%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity", meta = (ClampMin = "0.0"))
	float BurstBonusPerTarget = 0.1f;

	//장판 안 적 1명당 폭발 반경 증가 (0.02 = +2%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity", meta = (ClampMin = "0.0"))
	float BurstRadiusPerTarget = 0.02f;

	//종료 폭발의 경직 (장판 틱은 Impact 카테고리의 Impact Preset 사용, 기본 None)
	//Knockdown = 일반 몹 넉다운(Mass 포함), 엘리트 강한 경직, 보스 약한 경직
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity")
	EASImpactPreset BurstImpactPreset = EASImpactPreset::Knockdown;

	//폭발 강화에 세는 최대 인원
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|ZeroGravity", meta = (ClampMin = "0"))
	int32 MaxBurstTargets = 20;
};
