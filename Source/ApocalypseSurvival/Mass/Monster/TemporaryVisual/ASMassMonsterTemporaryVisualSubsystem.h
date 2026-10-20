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
	/** Mass 시각화 데이터에 접근하기 전에 호출해, 상태별 Descriptor를 모두 등록한다. */
	bool InitializeVisualDescriptors();

	/** InitializeVisualDescriptors() 성공 뒤에는 상태별 Handle만 반환하며 시각화 저장소를 변경하지 않는다. */
	FStaticMeshInstanceVisualizationDescHandle GetVisualHandle(EASMassMonsterTemporaryVisualState State);

private:
	bool EnsureVisualDescriptors();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DaySpawnMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NightSpawnMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DeadMaterial;

	FStaticMeshInstanceVisualizationDescHandle DaySpawnHandle;
	FStaticMeshInstanceVisualizationDescHandle NightSpawnHandle;
	FStaticMeshInstanceVisualizationDescHandle HitHandle;
	FStaticMeshInstanceVisualizationDescHandle DeadHandle;
};
