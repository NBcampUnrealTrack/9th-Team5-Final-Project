#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Framework/ASGameTypes.h"
#include "ASVisionSetupComponent.generated.h"

class AASFogOfWarManager;
class AASGameState;
class UASVisionDefinition;

/**
 * 서버 전용 시야 설정 컴포넌트. GameMode에 추가한다.
 * 안개 매니저를 찾거나 스폰하고 시야 정의를 전달한다.
 * 구역 매니저와 별도로 동작하며 AASGameState의 낮/밤 단계를 시야에 연결한다.
 * 구역 액터가 없는 레벨에서는 아무것도 하지 않는다.
 * AASGameState가 없으면 낮 시야를 유지한다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASVisionSetupComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASVisionSetupComponent();

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	AASFogOfWarManager* GetFogOfWarManager() const;

	/** 서버에서 모든 플레이어의 탐색 영역을 초기화한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Vision")
	void ResetExploredForAllPlayers();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Vision")
	TObjectPtr<UASVisionDefinition> VisionDefinition;

	/** 레벨에 이미 존재하지 않으면 스폰한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision")
	TSubclassOf<AASFogOfWarManager> FogOfWarManagerClass;

private:
	UFUNCTION()
	void HandlePhaseChanged(EGamePhase NewPhase);

	void ApplyPhase(EGamePhase NewPhase, bool bImmediate);

	UPROPERTY(Transient)
	TWeakObjectPtr<AASGameState> GameState;

	UPROPERTY(Transient)
	TWeakObjectPtr<AASFogOfWarManager> FogOfWarManager;
};
