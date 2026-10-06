#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Framework/ASGameTypes.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "ASMapGameFlowComponent.generated.h"

class AASFogOfWarManager;
class AASGameState;
class AASMapSystemManager;
class UASMapDefinition;
class UASVisionDefinition;

/**
 * 구역과 시야 기능을 연결하는 서버 전용 통합 지점.
 * AASGameState의 단계를 맵 시스템에 전달한다.
 * GameMode의 기본 서브오브젝트로 추가한 뒤 맵 설정을 구성한다.
 * 매니저를 스폰하고 공유 맵 상태를 초기화하며, 준비되면 맵을 시작한다.
 * 낮/밤 단계는 전달하거나 시작까지 보관한다. 결과 시 맵을 중지한다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASMapGameFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASMapGameFlowComponent();

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	AASMapSystemManager* GetMapManager() const { return MapManager; }

	/** 맵 규칙을 재시작하고 현재 낮/밤 단계를 다시 적용한다. 시드 0은 무작위 시드를 고른다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Map")
	void RestartMap(int32 NewSeed);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UASMapDefinition> MapDefinition;

	UPROPERTY(EditAnywhere, Category = "Vision")
	TObjectPtr<UASVisionDefinition> VisionDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TSubclassOf<AASMapSystemManager> MapManagerClass;

	/** 레벨에 이미 존재하지 않으면 스폰한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TSubclassOf<AASFogOfWarManager> FogOfWarManagerClass;

	/** 0이면 시작 시 무작위 시드를 고른다. 같은 선택을 재현하려면 값을 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	int32 MatchSeed = 0;

private:
	UFUNCTION()
	void HandleMapReady(AASMapSystemManager* Manager);

	UFUNCTION()
	void HandlePhaseChanged(EGamePhase NewPhase);

	void ApplyCurrentPhase();

	UPROPERTY(Transient)
	TObjectPtr<AASMapSystemManager> MapManager;

	UPROPERTY(Transient)
	TWeakObjectPtr<AASGameState> GameState;

	FASMapPhaseContext PendingPhase;
	bool bHasPendingPhase = false;
};
