#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Map/Core/ASMapTypes.h"
#include "ASMapDefinition.generated.h"

class UMaterialInterface;
class UTexture2D;

/** 맵 공통 규칙과 시야·표시 리소스. 구역별 데이터는 레벨 액터에 둔다. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASMapDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UASMapDefinition();

	/** 매 낮/밤 시작에 각 구역의 위험도가 1 증가할 확률. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DangerIncreaseChance = 0.7f;

	/** 선택 사항. ASC에 이 태그가 있는 캐릭터는 시야를 제공하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FGameplayTag DeadStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision")
	FASVisionSettings Vision;

	/**
	 * 맵 텍스처와 안개 마스크가 덮는 월드 XY 사각형.
	 * 맵 U는 월드 +Y 방향, 맵 V는 월드 -X 방향으로 증가한다 (맵 위쪽은 +X).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Units = "cm"))
	FVector2D MapWorldMin = FVector2D(-10000.0, -10000.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Units = "cm"))
	FVector2D MapWorldMax = FVector2D(10000.0, 10000.0);

	/** 맵 위젯에서 구역 아래에 그리는 선택적 탑다운 이미지. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UTexture2D> MapTexture;

	/**
	 * 팀 시야 밖의 게임 화면을 어둡게 하는 선택적 포스트 프로세스 머티리얼.
	 * 텍스처 "FogTexture"를 받는다 (알파: 보이는 곳은 0, 나머지는 탐색/미탐색 어둡기).
	 * 벡터 "MapWorldMin" / "MapWorldSize" (XY, cm)로 월드 위치를 안개 UV에 대응시킨다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UMaterialInterface> WorldFogMaterial;

	/** 인덱스 0은 위험도 1이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (EditFixedSize))
	TArray<FLinearColor> DangerColors;

	/**
	 * 공통 규칙과 시야·표시 설정의 값 범위를 검사한다.
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
