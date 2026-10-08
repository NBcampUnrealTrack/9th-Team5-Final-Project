#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ASZoneEffectDefinition.generated.h"

class UGameplayEffect;

/** 하나의 SetByCaller 값. 인덱스 0은 위험도 1이다. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASZoneEffectMagnitude
{
	GENERATED_BODY()

	/** GE 수정자가 사용하는 SetByCaller DataTag와 일치해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect", meta = (Categories = "Data"))
	FGameplayTag DataTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	TArray<float> ValuesByDangerLevel;
};

/** 구역과 팀별 효과. 행 이름 대신 (ZoneId, TeamId)를 키로 사용한다. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASZoneEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 레벨의 AASMapZone::ZoneId. 비어 있으면(None) 모든 구역에 적용하는 기본 행이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	FName ZoneId;

	/** 정확한 팀 행이 없으면 팀 255 행을 모든 팀의 대체 규칙으로 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	uint8 TeamId = 255;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	TSubclassOf<UGameplayEffect> Effect;

	/** SetByCaller 키마다 위험도 1..5에 해당하는 값을 저장한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effect")
	TArray<FASZoneEffectMagnitude> Magnitudes;
};

/** 서버가 구역 안의 ASC에 적용할 팀별 위험도 효과와 구역별 추가 효과 설정. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASZoneEffectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 기본 효과와 구역별 추가 효과 테이블. 비어 있으면 아무 효과도 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASZoneEffectRow"))
	TObjectPtr<UDataTable> ZoneEffectTable;

	/** 하나라도 보유하면 기존 구역 효과를 제거하고 새 효과를 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	FGameplayTagContainer BlockedTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.05", Units = "s"))
	float UpdateInterval = 0.25f;

	/** 모든 구역의 기본 행에서 정확한 팀을 먼저 찾고, 없으면 팀 255 행을 반환한다. */
	const FASZoneEffectRow* FindBaseRow(uint8 TeamId) const;

	/** 해당 구역의 정확한 팀 행을 먼저 찾고, 없으면 팀 255 행을 반환한다. */
	const FASZoneEffectRow* FindZoneRow(FName ZoneId, uint8 TeamId) const;

	bool ValidateDefinition(TArray<FString>& OutErrors) const;
	static bool GetMagnitudeForLevel(const FASZoneEffectMagnitude& Magnitude, int32 DangerLevel, float& OutValue);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** 기본 행과 구역 행에 동일한 배율 검증을 적용한다. */
	static void ValidateMagnitudes(const TArray<FASZoneEffectMagnitude>& Magnitudes, const FString& OwnerPath, TArray<FString>& OutErrors);
};
