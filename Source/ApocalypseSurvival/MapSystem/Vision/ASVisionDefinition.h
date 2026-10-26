#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MapSystem/Vision/ASVisionTypes.h"
#include "ASVisionDefinition.generated.h"

class AActor;
class UMaterialInterface;

/** 맵 규칙과 독립적인 팀 시야 설정과 안개 표시 리소스. */
UCLASS(Blueprintable, BlueprintType)
class APOCALYPSESURVIVAL_API UASVisionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UASVisionDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision", meta = (ShowOnlyInnerProperties))
	FASVisionSettings Settings;

	/** 서버가 이 클래스와 자식 액터에 없는 시야 소스를 추가한다. 비우면 자동 추가를 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Attach")
	TArray<TSubclassOf<AActor>> AutoVisionSourceClasses;

	/** 서버가 이 클래스와 자식 액터에 없는 가시성 컴포넌트를 추가한다. 비우면 자동 추가를 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Attach")
	TArray<TSubclassOf<AActor>> AutoVisibilityTargetClasses;

	/** 기본값은 State.Dead. ASC에 이 태그가 있는 캐릭터는 시야를 제공하지 않으며, 비우면 검사를 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FGameplayTag DeadStateTag;

	/** 서버에서 이 애셋 태그 중 하나를 가진 어빌리티가 활성화되면 소유자의 수풀 은폐를 AttackRevealSeconds 동안 해제한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FGameplayTagContainer AttackRevealAbilityTags;

	/**
	 * 팀 시야 밖의 게임 화면을 어둡게 하는 선택적 포스트 프로세스 머티리얼. 비우면 월드 안개를 끄며 미니맵 안개는 유지한다.
	 * 텍스처 "FogTexture"를 받는다 (B: 현재 로컬 팀 논리 시야, A: 보이는 곳은 0·나머지는 탐색/미탐색 어둡기).
	 * 벡터 "MapWorldMin" / "MapWorldSize" (XY, cm)는 맵 정의의 영역(UASMapDefinition::MapWorldMin..Max)에서 가져와 월드 위치를 안개 UV에 대응시킨다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision|Fog")
	TObjectPtr<UMaterialInterface> WorldFogMaterial;

	/** 탐색한 셀의 알파. 0은 투명, 255는 불투명한 검정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision|Fog")
	uint8 ExploredAlpha = 150;

	/** 미탐색 셀의 알파. 0은 투명, 255는 불투명한 검정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision|Fog")
	uint8 UnexploredAlpha = 235;

	/** 시야 설정의 값 범위를 검사한다. 오류가 없으면 true. */
	bool ValidateDefinition(TArray<FString>& OutErrors) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
