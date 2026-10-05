#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASFogDisplayComponent.generated.h"

class APostProcessVolume;
class UASMapDefinition;
class UMaterialInstanceDynamic;
class UTexture2D;

/** 로컬 CPU 시야/탐색 마스크, 안개 텍스처, 월드 포스트 프로세스. */
UCLASS(ClassGroup = (AS))
class APOCALYPSESURVIAL_API UASFogDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASFogDisplayComponent();

	void Configure(UASMapDefinition* Definition);
	UTexture2D* GetFogTexture() const { return FogTexture; }
	bool IsLocationExplored(const FVector& Location) const;
	bool IsLocationVisibleLocally(const FVector& Location) const;
	void ResetExplored();
	void UpdateLocalMask();

	/** 제한 영역이 없는 로컬 전용 포스트 프로세스 볼륨으로 안개 텍스처를 적용한다. */
	void UpdateWorldFog();
	void RemoveWorldFog();

private:
	FVector2D WorldToGrid(const FVector& Location) const;
	int32 GridIndexAt(const FVector& Location) const;
	void RasterizeEllipse(const FVector2D& Center, const FVector2D& Radii);
	void RasterizeTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C);
	void UploadTexture();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "255"))
	int32 ExploredAlpha = 150;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "255"))
	int32 UnexploredAlpha = 235;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FogTexture;

	UPROPERTY(Transient)
	TObjectPtr<UASMapDefinition> MapDefinition;

	/** 팀 시야 밖의 게임 화면을 어둡게 한다 (UASMapDefinition::WorldFogMaterial 필요). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bApplyWorldFog = true;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WorldFogMaterial;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> WorldFogVolume;

	TArray<uint8> VisibleCells;
	TArray<uint8> PreviousVisibleCells;
	TArray<uint8> ExploredCells;
	int32 Resolution = 0;
	bool bMaskDirty = true;
};
