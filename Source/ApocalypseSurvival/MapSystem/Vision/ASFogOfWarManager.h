#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapSystem/Vision/ASVisionTypes.h"
#include "ASFogOfWarManager.generated.h"

class UASVisionDefinition;
class UASFogDisplayComponent;
class UASVisibilityComponent;
class UASVisionEvaluatorComponent;
class UASVisionSourceComponent;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASOnWallOcclusionChangedSignature, bool, bEnabled);

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
class APOCALYPSESURVIVAL_API AASFogOfWarManager : public AActor
{
	GENERATED_BODY()

public:
	AASFogOfWarManager();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 스폰한 쪽에서 지정하거나 레벨에 배치된 매니저에서 설정한다. */
	UPROPERTY(EditAnywhere, Replicated, Category = "Vision")
	TObjectPtr<UASVisionDefinition> VisionDefinition;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	UASVisionDefinition* GetVisionDefinition() const { return VisionDefinition; }

	/** 서버 전용. 맵 상태와 독립적으로 시야 설정을 연결한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Vision")
	void ServerSetVisionDefinition(UASVisionDefinition* InVisionDefinition);

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsWallOcclusionEnabled() const { return bWallOcclusionEnabled; }

	/** 서버 공유 옵션. 탐색 영역은 유지하고 다음 시야 평가부터 적용한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Vision")
	void SetWallOcclusionEnabled(bool bEnabled);

	/** 서버 전용. 현재 밤 비율에서 새 목표까지 전환하며 즉시 적용할 수도 있다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Vision")
	void SetNight(bool bNight, bool bImmediate = false);

	/** 복제된 서버 시간으로 평가한 현재 밤 비율 (0: 낮, 1: 밤). */
	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	float GetNightAlpha() const;

	UPROPERTY(BlueprintAssignable, Category = "AS|Vision")
	FASOnWallOcclusionChangedSignature OnWallOcclusionChanged;

	/** 서버에서 모든 머신의 탐색 데이터를 초기화한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Vision")
	void ResetExploredForAllPlayers();

	/**
	 * 로컬 팀이 현재 보는 곳은 투명, 탐색한 곳은 ExploredAlpha,
	 * 그 외는 UnexploredAlpha이다. RGB는 검정이다. 시야 정의의 FogWorldMin..Max를 덮는다.
	 */
	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	UTexture2D* GetFogTexture() const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsLocationExplored(const FVector& Location) const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsLocationVisibleLocally(const FVector& Location) const;

	/** 이 머신의 탐색 데이터를 지운다. */
	UFUNCTION(BlueprintCallable, Category = "AS|Vision")
	void ResetExplored();

	/**
	 * 테스트에 공개된 핵심 시야 규칙.
	 * @param bWallOcclusion 시야 매니저의 공유 옵션.
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
	bool EnsureConfigured(bool bLogMissingDefinition = false);

	UFUNCTION()
	void OnRep_WallOcclusionEnabled();

	UFUNCTION()
	void OnRep_ExploredResetCount();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASVisionEvaluatorComponent> VisionEvaluatorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASFogDisplayComponent> FogDisplayComponent;

	UPROPERTY(ReplicatedUsing = OnRep_WallOcclusionEnabled)
	bool bWallOcclusionEnabled = true;

	/** 늦게 참여한 클라이언트도 서버 시간으로 현재 전환 위치를 계산한다. */
	UPROPERTY(Replicated)
	FASVisionNightBlend NightBlend;

	UPROPERTY(ReplicatedUsing = OnRep_ExploredResetCount)
	int32 ExploredResetCount = 0;

	bool bWallOcclusionInitialized = false;
	bool bConfigurationErrorLogged = false;
	float ActiveInterval = 0.0f;
	FTimerHandle VisionTimerHandle;
};
