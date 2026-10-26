#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMassMonsterFogMaterialSubsystem.generated.h"

class AASFogOfWarManager;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

/**
 * Mass Visualizer가 생성한 몬스터 ISM Material Slot에 월드별 Fog MID를 공유 바인딩한다.
 * Entity별 MID나 Per-Instance 파라미터 갱신은 만들지 않는다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterFogMaterialSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Mass 시각화 Processor에서 호출한다. 새 ISM은 제한 주기로 탐색하고, 상태 변경은 즉시 모든 MID에 반영한다. */
	void Refresh(const AASFogOfWarManager* FogManager);

private:
	bool IsMassMonsterMaterial(const UMaterialInterface* Material) const;
	UMaterialInstanceDynamic* FindOrCreateFogMaterial(UMaterialInterface* Material);
	void ApplyFogParameters(UMaterialInstanceDynamic& Material) const;
	void ScanMassVisualizerMaterials();

	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInstanceDynamic>> FogMaterialInstances;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentFogTexture;

	FVector2D CurrentMapWorldMin = FVector2D::ZeroVector;
	FVector2D CurrentMapWorldSize = FVector2D(1.0, 1.0);
	FVector2D CurrentLocalVisionCenter = FVector2D::ZeroVector;
	float CurrentLocalVisionRadius = 0.0f;
	float CurrentLocalVisionSoftness = 90.0f;
	uint64 LastScanFrame = 0;
	int32 LastBoundMaterialSlotCount = INDEX_NONE;
	bool bFogEnabled = false;
	bool bFogMaskReady = false;
	bool bHasCachedState = false;
};
