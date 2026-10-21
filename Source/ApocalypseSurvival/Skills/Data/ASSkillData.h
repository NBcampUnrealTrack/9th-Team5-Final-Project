#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ASSkillData.generated.h"

class UTexture2D;

/*
 * DT_SkillStats 한 행 = 스킬 하나의 밸런스 수치
 * 스킬 종류에 맞는 칸만 사용 (근접: Melee, 투사체: Projectile, 범위: Area, 돌진: Dash 카테고리)
 */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASSkillStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	//공통
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "0.0"))
	float BaseDamage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "0.0"))
	float AttackPowerScale = 1.0f;

	//0이면 쿨타임 GE에 적힌 시간 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 0.0f;

	//0이면 코스트 GE에 적힌 값 사용 - 마나
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "0.0"))
	float ManaCost = 0.0f;

	//0이면 코스트 GE에 적힌 값 사용 - 스태미나
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "0.0"))
	float StaminaCost = 0.0f;

	//위치형 최대 사거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxCastRange = 800.0f;

	//근접
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeRadius = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "1.0", ClampMax = "360.0", Units = "deg"))
	float Angle = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (Units = "cm"))
	float ForwardOffset = 0.0f;

	//투사체
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Speed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRange = 1500.0f;

	//0 = 첫 적에서 사라짐, -1 = 무한 관통
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "-1"))
	int32 PierceCount = 0;

	//범위
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area", meta = (ClampMin = "0.0", Units = "cm"))
	float AreaRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area", meta = (ClampMin = "0.05", Units = "s"))
	float Duration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area", meta = (ClampMin = "0.05", Units = "s"))
	float TickInterval = 0.5f;

	//돌진
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.05", Units = "s"))
	float DashDuration = 0.25f;

	//충돌 판정 반경
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", Units = "cm"))
	float DashHitRadius = 60.0f;
	
	//버프 지속시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff", meta = (ClampMin = "0.0", Units = "s"))
	float BuffDuration = 1.0f;
	
	
};

/*
 * 스킬 하나의 정보 (DA_Skill_*)
 * 표시 정보 + DT_SkillStats 행 참조, 나중에 특화(탤런트) 목록도 여기에 추가
 */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASSkillData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UTexture2D> Icon;

	//공용 쿨타임 GE(GE_SkillCooldown)에 붙일 이 스킬의 쿨타임 태그 (스킬바 표시·재사용 막기에 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill", meta = (Categories = "Cooldown"))
	FGameplayTag CooldownTag;

	//DT_SkillStats의 행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill", meta = (RowType = "/Script/ApocalypseSurvival.ASSkillStatsRow"))
	FDataTableRowHandle StatsRow;

	//행을 못 찾으면 nullptr
	const FASSkillStatsRow* FindStats() const
	{
		return StatsRow.IsNull() ? nullptr : StatsRow.GetRow<FASSkillStatsRow>(TEXT("ASSkillData"));
	}
};
