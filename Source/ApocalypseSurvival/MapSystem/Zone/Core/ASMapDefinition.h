#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "MapSystem/Zone/Core/ASZoneDangerSchedule.h"
#include "ASMapDefinition.generated.h"

class UTexture2D;

/** 맵 공통 규칙과 표시 리소스. 구역별 데이터는 레벨 액터에 둔다. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASMapDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UASMapDefinition();

	/** 날짜별 낮/밤 위험도 증가 한도와 몬스터 우선·확률 규칙. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (RequiredAssetDataTags = "RowStructure=/Script/ApocalypseSurvival.ASZoneDangerScheduleRow"))
	TObjectPtr<UDataTable> DangerScheduleTable;

	/** 요청 날짜 이하에서 가장 큰 Day의 행. 마지막 행은 이후 날짜에도 재사용한다. */
	const FASZoneDangerScheduleRow* FindDangerScheduleRow(int32 Day) const;

	/**
	 * 맵 텍스처가 덮는 월드 XY 사각형.
	 * 맵 U는 월드 +Y 방향, 맵 V는 월드 -X 방향으로 증가한다 (맵 위쪽은 +X).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Units = "cm"))
	FVector2D MapWorldMin = FVector2D(-10000.0, -10000.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Units = "cm"))
	FVector2D MapWorldMax = FVector2D(10000.0, 10000.0);

	/** 맵 위젯에서 구역 아래에 그리는 선택적 탑다운 이미지. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UTexture2D> MapTexture;

	/** 인덱스 0은 위험도 1이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (EditFixedSize))
	TArray<FLinearColor> DangerColors;

	/**
	 * 공통 규칙과 표시 설정의 값 범위를 검사한다.
	 * @return 오류가 없으면 true.
	 */
	bool ValidateDefinition(TArray<FString>& OutErrors) const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FLinearColor GetDangerColor(int32 DangerLevel) const;

	/** 월드 위치를 [0,1] 맵 UV로 변환한다. 범위 밖의 값은 [0,1] 밖에 놓인다. */
	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FVector2D WorldToMapUV(const FVector& WorldLocation) const;

	/** 맵 UV를 월드 XY 위치로 되돌린다 (Z = 0). */
	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FVector MapUVToWorld(const FVector2D& MapUV) const;

	FVector2D GetMapWorldSize() const { return (MapWorldMax - MapWorldMin).ComponentMax(FVector2D(1.0, 1.0)); }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
