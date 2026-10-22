#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "ASZoneEffectDefinition.generated.h"

class UGameplayEffect;

/** 구역 효과를 적용할 대상. */
UENUM(BlueprintType)
enum class EASZoneEffectTarget : uint8
{
	All,      // 모든 대상
	Players,  // 플레이어
	Monsters  // 몬스터
};

static_assert(ASMap::MaxDangerLevel == 5 && ASMap::MinDangerLevel == 1,
	"Zone effect level columns must match the danger level range.");

/** 같은 (ZoneId, Target)의 행을 모아 하나의 GE로 적용한다. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASZoneEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 레벨의 AASMapZone::ZoneId. 비어 있으면(None) 모든 구역에 적용하는 기본 행이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	FName ZoneId;

	/** 정확한 대상 행이 없으면 All 행을 대체 규칙으로 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	EASZoneEffectTarget Target = EASZoneEffectTarget::All;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	TSubclassOf<UGameplayEffect> Effect;

	/** SetByCaller 태그. 그룹의 유일한 행이면 비워서 수치 없이 GE만 적용할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect", meta = (Categories = "Data"))
	FGameplayTag DataTag;

	/** 위험도 1의 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	float Level1 = 1.0f;

	/** 위험도 2의 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	float Level2 = 1.0f;

	/** 위험도 3의 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	float Level3 = 1.0f;

	/** 위험도 4의 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	float Level4 = 1.0f;

	/** 위험도 5의 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	float Level5 = 1.0f;

	/** 범위를 벗어나거나 유한하지 않은 값이면 false를 반환한다. */
	bool GetValueForLevel(int32 DangerLevel, float& OutValue) const;
};

/** 서버가 구역 안의 ASC에 적용할 대상별 위험도 효과와 구역별 추가 효과 설정. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASZoneEffectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * (ZoneId, Target)별 행을 모아 같은 GE에 태그별 수치를 설정한다. 정확한 대상 그룹이 없으면 All을 사용한다.
	 * 기본(None) 그룹과 현재 구역 그룹을 현재 위험도로 함께 적용한다. 빈 테이블은 효과를 적용하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASZoneEffectRow"))
	TObjectPtr<UDataTable> ZoneEffectTable;

	/** 하나라도 보유하면 기존 구역 효과를 제거하고 새 효과를 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	FGameplayTagContainer BlockedTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.05", Units = "s"))
	float UpdateInterval = 0.25f;

	/** 정확한 대상 그룹을 찾고, 없으면 All 그룹을 반환한다. 기본 그룹의 ZoneId는 None이다. */
	bool FindRows(FName ZoneId, EASZoneEffectTarget Target, TArray<const FASZoneEffectRow*>& OutRows) const;

	/** 플레이어/몬스터 외의 팀은 All 그룹만 사용한다. */
	static EASZoneEffectTarget GetTargetForTeam(uint8 TeamId);

	bool ValidateDefinition(TArray<FString>& OutErrors) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
