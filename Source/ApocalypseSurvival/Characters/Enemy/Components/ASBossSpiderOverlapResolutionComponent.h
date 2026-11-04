#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "ASBossSpiderOverlapResolutionComponent.generated.h"

class AASMonsterCharacter;

/**
 * 보스 스파이더의 일반 이동 중 플레이어 캡슐 겹침을 서버에서만 해소한다.
 * Charge는 이 Component의 Tick을 중지하고, CeilingSlam은 자체 착지 여유 거리로 이 보정을 넘긴다.
 */
UCLASS(ClassGroup = (Enemy), BlueprintType, meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASBossSpiderOverlapResolutionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UASBossSpiderOverlapResolutionComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    bool TryInitializeConfig();
    bool ResolvePlayerOverlaps(AASMonsterCharacter& Monster);

    FASBossSpiderOverlapResolutionConfig ActiveConfig;
    bool bHasActiveConfig = false;
    bool bReportedInvalidOwner = false;
};
