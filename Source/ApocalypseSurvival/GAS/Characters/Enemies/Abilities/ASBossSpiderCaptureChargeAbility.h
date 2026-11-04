#pragma once

#include "CoreMinimal.h"
#include "GAS/Characters/Enemies/Abilities/ASBossSpiderChargeAbility.h"
#include "ASBossSpiderCaptureChargeAbility.generated.h"

/** WebBound 플레이어를 Director에서 받아 같은 C++ 돌진 실행기를 사용하는 포획 돌진. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASBossSpiderCaptureChargeAbility : public UASBossSpiderChargeAbility
{
    GENERATED_BODY()

protected:
    virtual AActor* ResolveChargeTarget() const override;
    virtual bool IsChargeTargetValid(const AActor* Target) const override;
};
