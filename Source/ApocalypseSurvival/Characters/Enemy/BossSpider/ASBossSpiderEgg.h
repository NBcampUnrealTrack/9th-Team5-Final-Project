#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "ASBossSpiderEgg.generated.h"

class AASBossSpiderEgg;

UENUM(BlueprintType)
enum class EASBossSpiderEggState : uint8
{
    Descending,
    Incubating,
    Hatched,
    Expired
};

/**
 * 보스 알의 서버 Gameplay 수명주기다. BP 자식은 Mesh, 균열, 부화 연출만 구현한다.
 * 체력과 피격/사망은 AASMonsterCharacter의 공통 경로를 그대로 사용한다.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API AASBossSpiderEgg : public AASMonsterCharacter
{
    GENERATED_BODY()

public:
    AASBossSpiderEgg();

    void InitializeEgg(AASMonsterCharacter* InBossMonster, const FASBossSpiderEggSummonConfig& InConfig);

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Egg")
    EASBossSpiderEggState GetEggState() const { return EggState; }

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Egg")
    float GetHatchEndServerTime() const { return HatchEndServerTime; }

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Egg")
    float GetLandingEndServerTime() const { return LandingEndServerTime; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void HandleDeath() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Egg|Cosmetic")
    void OnEggStateChanged(EASBossSpiderEggState NewState);

private:
    UFUNCTION()
    void OnRep_EggState();

    UFUNCTION()
    void OnRep_LandingEndServerTime();

    void LandEgg();
    void HatchEgg();
    void MonitorBossLifetime();
    void ExpireEgg();
    bool SpawnEliteMinion();
    void ReleaseEggActiveTag();
    void SetDescentInvulnerable(bool bInvulnerable);
    void SetEggState(EASBossSpiderEggState NewState);

    UPROPERTY(ReplicatedUsing = OnRep_EggState)
    EASBossSpiderEggState EggState = EASBossSpiderEggState::Descending;

    UPROPERTY(ReplicatedUsing = OnRep_LandingEndServerTime)
    float LandingEndServerTime = 0.0f;

    UPROPERTY(Replicated)
    float HatchEndServerTime = 0.0f;

    FASBossSpiderEggSummonConfig ActiveConfig;
    TWeakObjectPtr<AASMonsterCharacter> BossMonster;
    FTimerHandle LandingTimerHandle;
    FTimerHandle HatchTimerHandle;
    FTimerHandle BossMonitorTimerHandle;
    bool bInitialized = false;
	bool bEggActiveTagRegistered = false;
    bool bDescentInvulnerable = false;
};
