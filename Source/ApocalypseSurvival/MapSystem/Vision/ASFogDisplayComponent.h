#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASFogDisplayComponent.generated.h"

class APostProcessVolume;
class UASVisionDefinition;
class UASVisionEvaluatorComponent;
class UASVisionSourceComponent;
class UASVisionWorldSubsystem;
class UMaterialInstanceDynamic;
class UTexture2D;

/** 로컬 CPU 시야/탐색 마스크, 안개 텍스처, 월드 포스트 프로세스. */
UCLASS(ClassGroup = (AS))
class APOCALYPSESURVIVAL_API UASFogDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASFogDisplayComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Configure(UASVisionDefinition* Definition);
	UTexture2D* GetFogTexture() const { return FogTexture; }
	/** 로컬 팀 시야 마스크가 적어도 한 번 갱신됐는지 반환한다. */
	bool HasLocalVisionMask() const { return bHasLocalVisionMask; }
	bool IsLocationExplored(const FVector& Location) const;
	bool IsLocationVisibleLocally(const FVector& Location) const;
	/** 보이던 Mass 적을 계속 표시할 수 있는, 밝은 시야 경계보다 안쪽으로 침식된 영역인지 반환한다. */
	bool IsLocationInLocalVisionExitRegion(const FVector& Location) const;
	/** 숨겨진 Mass 적의 재표시를 허용하는, 밝은 시야 경계보다 안쪽으로 침식된 영역인지 반환한다. */
	bool IsLocationInLocalVisionReentryRegion(const FVector& Location) const;
	/** 현재 로컬 팀 시야 주변의 Mass ISM 유지 Buffer에 위치하는지 반환한다. */
	bool IsLocationInLocalVisionBuffer(const FVector& Location) const;
	void ResetExplored();
	void UpdateLocalMask();

	/** 제한 영역이 없는 로컬 전용 포스트 프로세스 볼륨으로 안개 텍스처를 적용한다. */
	void UpdateWorldFog();
	void RemoveWorldFog();

private:
	struct FLocalMaskContext
	{
		bool bWallOcclusion;
		float NightAlpha;
		int32 LocalTeam;
		FVector2D WorldSize;
		int32 BlurRadius;
		const UASVisionSourceComponent* LocalSource;
	};
	void AdvancePendingFade();
	void ClearPreviousVisibleCells();
	void TraceSource(const UASVisionEvaluatorComponent& Evaluator, const UASVisionSourceComponent* Source,
		const FVector& Origin, float Radius, float DisplayRadius, const UASVisionSourceComponent* LocalSource);
	FIntRect RasterizeTeamSources(const UASVisionWorldSubsystem& Subsystem,
		const UASVisionEvaluatorComponent& Evaluator, const FLocalMaskContext& Context);
	bool RebuildRawAlpha(const FIntRect& PreviousLocalRegion, bool bLocalRegionChanged);
	void CopyLocalAlphaOutsideRegion(TArray<uint8>& Display, TArray<uint8>& Others,
		const TArray<uint8>& Alpha, const FIntRect& Region, const FIntRect& Excluded);
	void BlurLocalAlpha(const FIntRect& Region, int32 BlurRadius);
	void BlurChangedRegions(bool bFullRecompute, bool bLocalRegionChanged, int32 BlurRadius);
	void StartLocalFade(float FadeSeconds);

	FVector2D WorldToGrid(const FVector& Location) const;
	int32 GridIndexAt(const FVector& Location) const;
	void RasterizeEllipse(TArray<uint8>& Cells, const FVector2D& Center, const FVector2D& Radii);
	void RasterizeTriangle(TArray<uint8>& Cells, const FVector2D& A, const FVector2D& B, const FVector2D& C);
	bool HasLocalPlayer() const;
	const UASVisionSourceComponent* GetLocalVisionSource() const;
	void UpdateLocalVisionParameters();
	void UpdateShownAlpha(float Fraction);
	void RebuildMassVisibilityBuffer();
	void UploadTexture(const TArray<FIntRect>& DirtyRegions);
	FIntRect TileRegion(int32 TileIndex) const;
	void MarkVisibleTiles(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY);
	void MarkPendingTiles(const FIntRect& Region);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "255"))
	int32 ExploredAlpha = 150;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "255"))
	int32 UnexploredAlpha = 235;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FogTexture;

	UPROPERTY(Transient)
	TObjectPtr<UASVisionDefinition> VisionDefinition;

	/** 팀 시야 밖의 게임 화면을 어둡게 한다 (UASVisionDefinition::WorldFogMaterial 필요). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bApplyWorldFog = true;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WorldFogMaterial;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> WorldFogVolume;

	TArray<uint8> VisibleCells;
	/** VisibleCells를 MassVisibilityExitInsetDistance만큼 침식한 표시 유지 영역. */
	TArray<uint8> MassVisibilityExitCells;
	/** VisibleCells를 MassVisibilityReentryInsetDistance만큼 침식한 재진입 허용 영역. */
	TArray<uint8> MassVisibilityReentryCells;
	/** VisibleCells를 MassVisibilityBufferDistance만큼 확장한 로컬 표현 유지 영역. */
	TArray<uint8> MassVisibilityBufferCells;
	/** Mass Visibility Buffer를 선형 시간에 확장하기 위한 3-4 chamfer 거리 필드. */
	TArray<int32> MassVisibilityDistanceScratch;
	TArray<uint8> DisplayCells;
	TArray<uint8> OthersCells;
	TArray<uint8> ExploredCells;
	TArray<uint8> RawAlpha;
	TArray<uint8> TargetAlpha;
	TArray<uint8> ShownAlpha;
	TArray<uint8> FadeFromAlpha;
	TArray<uint8> DisplayRawAlpha;
	TArray<uint8> DisplayTargetAlpha;
	TArray<uint8> DisplayShownAlpha;
	TArray<uint8> DisplayFadeFromAlpha;
	TArray<uint8> OthersRawAlpha;
	TArray<uint8> OthersTargetAlpha;
	TArray<uint8> OthersShownAlpha;
	TArray<uint8> OthersFadeFromAlpha;
	TArray<uint8> BlurScratch;
	TArray<FVector2D> OcclusionPoints;
	TArray<FVector2D> ExtendedOcclusionPoints;
	/** 래스터 경계를 기록하고 변경과 페이드를 16셀 단위로 추적한다. */
	static constexpr int32 TileSize = 16;
	int32 TilesX = 0;
	int32 TilesY = 0;
	TArray<uint8> PreviousVisibleTiles;
	TArray<uint8> CurrentVisibleTiles;
	TArray<uint8> PendingFadeTiles;
	/** G/R이 A와 달라지는 로컬 영역이다. */
	FIntRect LocalRegion = FIntRect(0, 0, 0, 0);
	float FadeStartTime = 0.0f;
	bool bHasPendingFade = false;
	/** 타일 안에서 원본 알파가 실제로 바뀐 셀의 최소 사각형을 패스마다 재사용한다. */
	TArray<FIntRect> RawChangedRegions;
	TArray<FIntRect> ShownChangedRegions;
	/** U는 월드 Y를, V는 월드 X를 따른다. */
	int32 ResolutionX = 0;
	int32 ResolutionY = 0;
	FVector2D ConfiguredFogWorldMin = FVector2D::ZeroVector;
	FVector2D ConfiguredFogWorldMax = FVector2D::ZeroVector;
	bool bMaskDirty = true;
	/** 첫 RasterizeTeamSources 전에는 VisibleCells가 모두 0이므로 표현 컬링에 사용하면 안 된다. */
	bool bHasLocalVisionMask = false;
	/** 첫 표시와 텍스처 재생성은 페이드 없이 현재 목표 전체를 업로드한다. */
	bool bInitializeShownAlpha = true;
};
