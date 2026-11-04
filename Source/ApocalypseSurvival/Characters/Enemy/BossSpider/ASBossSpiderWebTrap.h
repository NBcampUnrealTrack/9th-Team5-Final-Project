#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/Data/ASBossSpiderPatternSet.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ASBossSpiderWebTrap.generated.h"

class AASMonsterCharacter;
class AASPlayerCharacter;
class USceneComponent;

/** 서버가 복제하는 거미줄 장판의 명시적 생명주기다. */
UENUM(BlueprintType)
enum class EASBossSpiderWebTrapState : uint8
{
    Arming,
    Armed,
    Triggered,
    Released,
    Expired
};

/**
 * 보스 스파이더의 서버 권한 거미줄 장판. 충돌 Component나 BP Overlap 이벤트에 의존하지 않는다.
 * BP 자식은 복제된 State와 수치로 Decal, SFX, VFX만 표현한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASBossSpiderWebTrap : public AActor
{
    GENERATED_BODY()

public:
    AASBossSpiderWebTrap();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Deferred Spawn 중 서버 Ability가 한 번 호출한다. */
    void InitializeTrap(AASMonsterCharacter* InBossMonster, const FASBossSpiderWebTrapConfig& InConfig);

    /** PlayerCharacter의 서버 RPC가 전달하는 탈출 입력을 검증한다. */
    bool ServerRegisterBreakFreeInput(AASPlayerCharacter* Player);

    /** 플레이어 사망/연결 해제 시 즉시 속박과 Director 예약을 해제한다. */
    void ReleaseCapturedPlayer(AASPlayerCharacter* Player);

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Web Trap")
    EASBossSpiderWebTrapState GetTrapState() const { return TrapState; }

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Web Trap")
    float GetTrapRadius() const { return TrapRadius; }

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Web Trap")
    int32 GetEscapeInputCount() const { return EscapeInputCount; }

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Web Trap")
    int32 GetRequiredEscapeInputs() const { return RequiredEscapeInputs; }

    UFUNCTION(BlueprintPure, Category = "Boss Spider|Web Trap")
    float GetEscapeEndServerTime() const { return EscapeEndServerTime; }

protected:
    /** State가 바뀐 모든 머신에서 Decal/연출만 갱신한다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Web Trap|Cosmetic")
    void OnTrapStateChanged(EASBossSpiderWebTrapState NewState);

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Spider|Web Trap|Cosmetic")
    void OnEscapeProgressChanged(int32 CurrentInputs, int32 RequiredInputs, float EndServerTime);

private:
    UFUNCTION()
    void OnRep_TrapState();

    UFUNCTION()
    void OnRep_EscapeProgress();

    void BeginArmedState();
    void MonitorTrap();
    void ExpireTrap();
    bool TryCapturePlayer(AASPlayerCharacter& Player);
    void ReleaseCapture(bool bRemoveBoundEffect);
    void SetTrapState(EASBossSpiderWebTrapState NewState);
    bool IsPlayerEligible(const AASPlayerCharacter& Player) const;
    double GetWorldTimeSeconds() const;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(ReplicatedUsing = OnRep_TrapState)
    EASBossSpiderWebTrapState TrapState = EASBossSpiderWebTrapState::Arming;

    UPROPERTY(Replicated)
    float TrapRadius = 0.0f;

    UPROPERTY(Replicated)
    float ArmEndServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_EscapeProgress)
    int32 EscapeInputCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_EscapeProgress)
    int32 RequiredEscapeInputs = 0;

    UPROPERTY(ReplicatedUsing = OnRep_EscapeProgress)
    float EscapeEndServerTime = 0.0f;

    FASBossSpiderWebTrapConfig ActiveConfig;
    TWeakObjectPtr<AASMonsterCharacter> BossMonster;
    TWeakObjectPtr<AASPlayerCharacter> CapturedPlayer;
    FActiveGameplayEffectHandle BoundEffectHandle;
    FTimerHandle ArmTimerHandle;
    FTimerHandle MonitorTimerHandle;
    FTimerHandle ExpireTimerHandle;
    bool bInitialized = false;
};
