#pragma once

#include "CoreMinimal.h"
#include "MassRepresentationTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualFragment.h"
#include "ASMassMonsterTemporaryVisualSubsystem.generated.h"

class UMaterialInstanceDynamic;

/**
 * 임시 Mass 몬스터 색상 표현용 ISM Descriptor를 월드별로 보관한다.
 * 콘텐츠 머티리얼을 추가하지 않고 기존 BasicShapeMaterial의 Color 파라미터를 MID로만 덮어쓴다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterTemporaryVisualSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Entity Config Template 구성 시 호출한다. Processor가 ISM 배열에 접근하기 전에
	 * 원본 Descriptor의 Hit/Dead 변형을 등록해 접근 충돌을 방지한다.
	 */
	void RegisterStateVisuals(
		const FStaticMeshInstanceVisualizationDesc& BaseDesc,
		FStaticMeshInstanceVisualizationDescHandle BaseHandle);

	/** Template 단계에 미리 등록한 상태 Descriptor를 반환한다. */
	FStaticMeshInstanceVisualizationDescHandle GetVisualHandle(
		EASMassMonsterTemporaryVisualState State,
		FStaticMeshInstanceVisualizationDescHandle BaseHandle);

private:
	struct FStateVisualHandles
	{
		FStaticMeshInstanceVisualizationDescHandle Hit;
		FStaticMeshInstanceVisualizationDescHandle Dead;
	};

	FStaticMeshInstanceVisualizationDescHandle CreateStateVisual(
		const FStaticMeshInstanceVisualizationDesc& BaseDesc,
		const FLinearColor& Color);

	/** 원본 Descriptor별 Hit/Dead Descriptor 캐시. */
	TMap<int32, FStateVisualHandles> StateVisualHandlesByBaseHandle;

	/** 런타임 MID가 GC되지 않도록 월드 Subsystem이 소유한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> StateMaterials;
};
