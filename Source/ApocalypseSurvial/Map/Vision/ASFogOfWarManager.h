#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASFogOfWarManager.generated.h"

class UASMapDefinition;
class UASFogDisplayComponent;
class UASVisibilityComponent;
class UASVisionEvaluatorComponent;
class UASVisionSourceComponent;
class UTexture2D;

/**
 * 팀 시야와 탐색 영역 추적.
 *
 * 서버: 모든 UASVisibilityComponent를 각 팀의 시야 소스에 대해
 * UpdateHz로 평가한다 (원형 반경, 선택적 ASVisionWall 차폐, 수풀,
 * 공격 노출). 결과는 팀 마스크로 복제한다.
 *
 * 로컬 플레이어가 있는 모든 머신: 로컬 팀의 현재 시야를
 * CPU 마스크에 그린다 (활성화 시 부채꼴 광선으로 벽 차폐 반영). 탐색
 * 마스크를 누적하고 둘을 하나의 텍스처에 업로드한다. 탐색 데이터는 지형만
 * 유지한다. 적 위치는 저장하지 않는다.
 *
 * 게임 흐름 컴포넌트가 스폰하거나 (복제, 항상 관련 있음) 레벨에 배치한다.
 * 이 액터는 공유 구역 상태를 변경하지 않는다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIAL_API AASFogOfWarManager : public AActor
{
	GENERATED_BODY()

public:
	AASFogOfWarManager();

	/**
	 * 로컬 팀이 현재 보는 곳은 투명, 탐색한 곳은 ExploredAlpha,
	 * 그 외는 UnexploredAlpha이다. RGB는 검정이다. 맵 정의의 MapWorldMin..Max를 덮는다.
	 */
	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	UTexture2D* GetFogTexture() const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsLocationExplored(const FVector& Location) const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsLocationVisibleLocally(const FVector& Location) const;

	/** 이 머신의 탐색 데이터를 지운다 (새 매치가 시작될 때도 자동으로 수행). */
	UFUNCTION(BlueprintCallable, Category = "AS|Vision")
	void ResetExplored();

	/**
	 * 테스트에 공개된 핵심 시야 규칙.
	 * @param bWallOcclusion MapState의 공유 옵션.
	 */
	bool CanSourceSeeTarget(
		const UASVisionSourceComponent* Source,
		const UASVisibilityComponent* Target,
		double ServerTime,
		bool bWallOcclusion) const;

	/** 로컬 안개의 벽 트레이스에 사용하는 공유 평가기. */
	UASVisionEvaluatorComponent* GetVisionEvaluator() const { return VisionEvaluatorComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TickVision();
	bool EnsureConfigured();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASVisionEvaluatorComponent> VisionEvaluatorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASFogDisplayComponent> FogDisplayComponent;

	UPROPERTY(Transient)
	TObjectPtr<UASMapDefinition> MapDefinition;

	int32 LastMatchId = INDEX_NONE;
	float ActiveInterval = 0.0f;
	FTimerHandle VisionTimerHandle;
};
