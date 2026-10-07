#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MapSystem/Vision/ASVisionTypes.h"
#include "ASVisionDefinition.generated.h"

class UMaterialInterface;

/** 맵 규칙과 독립적인 팀 시야 설정과 안개 표시 리소스. */
UCLASS(Blueprintable, BlueprintType)
class APOCALYPSESURVIVAL_API UASVisionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision")
	FASVisionSettings Settings;

	/** 선택 사항. ASC에 이 태그가 있는 캐릭터는 시야를 제공하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FGameplayTag DeadStateTag;

	/**
	 * 팀 시야 밖의 게임 화면을 어둡게 하는 선택적 포스트 프로세스 머티리얼.
	 * 텍스처 "FogTexture"를 받는다 (알파: 보이는 곳은 0, 나머지는 탐색/미탐색 어둡기).
	 * 벡터 "MapWorldMin" / "MapWorldSize" (XY, cm)로 월드 위치를 안개 UV에 대응시킨다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UMaterialInterface> WorldFogMaterial;

	/** 안개 텍스처가 덮는 월드 XY 사각형. U는 +Y, V는 -X 방향이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Units = "cm"))
	FVector2D FogWorldMin = FVector2D(-10000.0, -10000.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Units = "cm"))
	FVector2D FogWorldMax = FVector2D(10000.0, 10000.0);

	/** 시야 설정의 값 범위와 안개 영역을 검사한다. 오류가 없으면 true. */
	bool ValidateDefinition(TArray<FString>& OutErrors) const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	FVector2D GetFogWorldSize() const { return (FogWorldMax - FogWorldMin).ComponentMax(FVector2D(1.0, 1.0)); }

	/** 월드 위치를 안개 UV로 변환한다. 영역 밖의 값은 [0,1] 밖에 놓인다. */
	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	FVector2D WorldToFogUV(const FVector& WorldLocation) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
