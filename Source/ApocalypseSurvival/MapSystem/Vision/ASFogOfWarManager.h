#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapSystem/Vision/ASVisionTypes.h"
#include "ASFogOfWarManager.generated.h"

class UASVisionDefinition;
class UASFogDisplayComponent;
class UASVisionEvaluatorComponent;
class UTexture2D;
struct FASFogMaterialState;

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
	 * B는 현재 로컬 팀 논리 시야(0: 시야 밖, 255: 현재 시야)다.
	 * G/R/A는 로컬 팀이 현재 보는 곳은 투명, 탐색한 곳은 시야 정의의 ExploredAlpha,
	 * 그 외는 시야 정의의 UnexploredAlpha인 기존 안개 표현이다. 맵 정의의 MapWorldMin..Max를 덮는다.
	 */
	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	UTexture2D* GetFogTexture() const;

	/** FogTexture와 맵 좌표계, 로컬 마스크 준비 상태를 렌더링 시스템에 제공한다. */
	FASFogMaterialState GetFogMaterialState() const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsLocationExplored(const FVector& Location) const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsLocationVisibleLocally(const FVector& Location) const;

	/** 수풀 안의 위치가 같은 수풀의 모든 로컬 팀 시야 소스 반경 밖이면 은폐된다. */
	bool IsLocationConcealedByBushLocally(const FVector& Location) const;

	/** 현재 로컬 팀 시야 주변의 Mass ISM 유지 Buffer에 위치하는지 반환한다. */
	bool IsLocationInLocalVisionBuffer(const FVector& Location) const;

	/** 로컬 팀의 현재 시야 마스크가 준비됐는지 반환한다. 로컬 표시 전용 시스템에서 사용한다. */
	bool HasLocalVisionMask() const;

	/** 이 머신의 탐색 데이터를 지운다. */
	UFUNCTION(BlueprintCallable, Category = "AS|Vision")
	void ResetExplored();

	/** 로컬 안개의 벽 트레이스에 사용하는 공유 평가기. */
	UASVisionEvaluatorComponent* GetVisionEvaluator() const { return VisionEvaluatorComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TickVision();
	bool EnsureConfigured(bool bLogMissingDefinition = false);
	int32 AutoAttachVisionComponents(AActor* Actor);

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

	struct FLocalTeamBushSource
	{
		FVector2D Location;
		float Radius;
	};

	mutable TMap<FName, TArray<FLocalTeamBushSource>> LocalTeamBushSources;
	mutable uint64 LocalTeamBushSourcesFrame = MAX_uint64;
	bool bWallOcclusionInitialized = false;
	bool bConfigurationErrorLogged = false;
	bool bAutoAttachInitialized = false;
	FDelegateHandle ActorSpawnedDelegateHandle;
	float ActiveInterval = 0.0f;
	FTimerHandle VisionTimerHandle;
};
