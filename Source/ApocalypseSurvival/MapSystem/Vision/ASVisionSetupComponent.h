#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASVisionSetupComponent.generated.h"

class AASFogOfWarManager;
class UASVisionDefinition;

/**
 * 서버 전용 시야 설정 컴포넌트. GameMode에 추가한다.
 * 안개 매니저를 찾거나 스폰하고 시야 정의를 전달한다.
 * 구역 시스템과 독립적으로 동작한다.
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

	UPROPERTY(EditAnywhere, Category = "Vision")
	TObjectPtr<UASVisionDefinition> VisionDefinition;

	/** 레벨에 이미 존재하지 않으면 스폰한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vision")
	TSubclassOf<AASFogOfWarManager> FogOfWarManagerClass;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AASFogOfWarManager> FogOfWarManager;
};
