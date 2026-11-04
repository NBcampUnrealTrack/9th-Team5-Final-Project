#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "Characters/Enemy/Data/ASMonsterPatternSet.h"
#include "ASBossSpiderPatternSet.generated.h"

class AASMonsterAttackIndicatorActor;
class AASBossSpiderEgg;
class AASBossSpiderWebTrap;
class UGameplayEffect;

/** 보스 스파이더 Director가 사용하는 패턴 선택·페이즈 정책이다. SelectionPolicyConfig에만 저장한다. */
USTRUCT(BlueprintType)
struct FASBossSpiderDirectorConfig : public FASMonsterPatternSelectionConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Timing", meta = (ClampMin = "0.05", Units = "s"))
    float DecisionInterval = 0.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PhaseTwoHealthPercent = 0.60f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Timing", meta = (ClampMin = "0.0", Units = "s"))
    float MajorPatternInterval = 13.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Timing", meta = (ClampMin = "0.0", Units = "s"))
    float WebTrapInterval = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Patterns", meta = (Categories = "Ability.Monster.Attack"))
    FGameplayTag WebTrapPatternId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Patterns", meta = (Categories = "Ability.Monster.Attack"))
    FGameplayTag EggSummonPatternId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Patterns", meta = (Categories = "Ability.Monster.Attack"))
    FGameplayTag CeilingSlamPatternId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Patterns", meta = (Categories = "Ability.Monster.Attack"))
    FGameplayTag CaptureChargePatternId;
};

/** 보스 스파이더 본체와 플레이어 캡슐의 일반 보행 중 겹침을 서버에서 해소하는 설정이다. */
USTRUCT(BlueprintType)
struct FASBossSpiderOverlapResolutionConfig
{
    GENERATED_BODY()

    /** 두 캡슐 반경 합산 뒤에 추가로 남길 수평 여유 거리다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overlap Resolution", meta = (ClampMin = "0.0", Units = "cm"))
    float PlayerSeparationPadding = 0.0f;

    /** 여러 플레이어와 동시에 겹칠 때 한 Tick 안에서 수행할 최대 분리 반복 횟수다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overlap Resolution", meta = (ClampMin = "1", ClampMax = "4"))
    int32 MaxResolutionPasses = 1;
};

/** 보스 스파이더의 일반/포획 돌진에 공통으로 쓰는 서버 권한 설정이다. */
USTRUCT(BlueprintType)
struct FASBossSpiderChargeConfig : public FASMonsterPatternRuntimeConfig
{
    GENERATED_BODY()

    /** 인디케이터가 표시되고 보스가 방향을 잠그는 시간. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.0", Units = "s"))
    float WindupTime = 0.6f;

    /** 이동이 끝난 뒤 방향을 고정한 채 유지하는 Ability 내부 후딜. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.0", Units = "s"))
    float RecoveryTime = 0.35f;

    /** 타깃과 가까워도 유지할 최소 실제 이동 거리. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.0", Units = "cm"))
    float MinTravelDistance = 1100.0f;

    /** 타깃이 멀어도 넘지 않는 최대 실제 이동 거리. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "1.0", Units = "cm"))
    float MaxTravelDistance = 1600.0f;

    /** 잠근 타깃 중심을 얼마나 지나갈지 결정하는 여유 거리. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.0", Units = "cm"))
    float TargetOvershootDistance = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "1.0", Units = "cm/s"))
    float MoveSpeed = 1800.0f;

    /** 보스의 실제 캡슐 반경에 더할 이동 중 타격 여유 반경. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.0", Units = "cm"))
    float HitPadding = 40.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.005", Units = "s"))
    float StepInterval = 0.02f;

    /** 이동 종료로 간주할 남은 전진 거리다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.0", Units = "cm"))
    float CompletionTolerance = 1.0f;

    /** ChargeBlocker만 경로 종료 원인으로 쓰는 전용 Trace Channel이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision")
    TEnumAsByte<ECollisionChannel> BlockerTraceChannel = ECC_GameTraceChannel2;

    /** 바닥 높이 보정에만 쓰는 Trace Channel이다. 이동 차단 판정에는 사용하지 않는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision")
    TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision", meta = (ClampMin = "0.0", Units = "cm"))
    float GroundTraceStartHeight = 1000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision", meta = (ClampMin = "1.0", Units = "cm"))
    float GroundTraceDepth = 5000.0f;

    /** 지면 ImpactPoint에서 캡슐 바닥에 남길 여유 높이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision", meta = (ClampMin = "0.0", Units = "cm"))
    float GroundClearance = 1.5f;

    /** 종료 뒤 플레이어 캡슐과 겹칠 때 보스 캡슐 사이에 남길 최소 여유다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision", meta = (ClampMin = "0.0", Units = "cm"))
    float EndOverlapSafetyPadding = 8.0f;

    /**
     * 기존 BP OverlapGuard처럼 일반 보행 중 겹침을 해소하는 컴포넌트의 이름이다.
     * Charge 이동 중에는 해당 컴포넌트 Tick을 멈추고, 종료 위치에서만 C++가 겹침을 보정한다.
     * None이면 기존 Guard를 건드리지 않는다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision")
    FName OverlapGuardComponentName = TEXT("OverlapGuard");

    /** 외부 이동 보정이 Charge 진행을 막아도 Ability가 고착되지 않도록 예상 이동 시간에 더할 여유다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Collision", meta = (ClampMin = "0.0", Units = "s"))
    float MovementTimeoutGrace = 0.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Damage", meta = (ClampMin = "0.0"))
    float BaseDamage = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Damage", meta = (ClampMin = "0.0"))
    float AttackPowerScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Damage")
    EASImpactPreset ImpactPreset = EASImpactPreset::Heavy;

    /** CaptureCharge에서는 이 Effect로 추가 디버프를 적용한다. 일반 Charge는 비워 둔다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Status")
    TSubclassOf<UGameplayEffect> OnHitStatusEffectClass;

    /** 공용 Charge Indicator가 표시할 레인 폭. 0이면 보스 캡슐 반경과 HitPadding으로 계산한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph", meta = (ClampMin = "0.0", Units = "cm"))
    float IndicatorWidth = 0.0f;

    /** BP_Indicator_Charge 같은 연출 전용 복제 Indicator Class. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph")
    TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;
};

/** 보스 스파이더 전방 부채꼴 공격의 서버 권한 설정이다. */
USTRUCT(BlueprintType)
struct FASBossSpiderFanConfig : public FASMonsterPatternRuntimeConfig
{
    GENERATED_BODY()

    /** 부채꼴 예고와 방향 잠금 시간이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan", meta = (ClampMin = "0.0", Units = "s"))
    float WindupTime = 0.65f;

    /** 타격 뒤 방향을 유지하는 Ability 내부 후딜이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan", meta = (ClampMin = "0.0", Units = "s"))
    float RecoveryTime = 0.25f;

    /** 실제 Cone 피해 판정과 Sector Indicator의 반경이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan", meta = (ClampMin = "1.0", Units = "cm"))
    float SectorRadius = 350.0f;

    /** 실제 Cone 피해 판정과 Sector Indicator의 전체 각도다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan", meta = (ClampMin = "1.0", ClampMax = "360.0", Units = "deg"))
    float SectorAngleDegrees = 110.0f;

    /** Commit 시 잠그는 Fan 원점의 전방 오프셋이다. Indicator와 Cone 피해가 이 동일한 XY 원점을 쓴다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan|Origin", meta = (Units = "cm"))
    float AttackOriginForwardOffset = 0.0f;

    /** TrackTarget FacingPolicy일 때만 쓰는 방향 갱신 간격이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan", meta = (ClampMin = "0.005", Units = "s"))
    float FacingUpdateInterval = 0.02f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan|Damage", meta = (ClampMin = "0.0"))
    float BaseDamage = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan|Damage", meta = (ClampMin = "0.0"))
    float AttackPowerScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan|Damage")
    EASImpactPreset ImpactPreset = EASImpactPreset::Standard;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fan|Status")
    TSubclassOf<UGameplayEffect> OnHitStatusEffectClass;

    /** Sector 시각화만 담당하는 복제 Indicator Class다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph")
    TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;
};

/** 보스 스파이더의 원형 거미줄 장판과 속박·탈출 규칙의 단일 원본이다. */
USTRUCT(BlueprintType)
struct FASBossSpiderWebTrapConfig : public FASMonsterPatternRuntimeConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap", meta = (ClampMin = "0.0", Units = "s"))
    float CastWindupTime = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap", meta = (ClampMin = "0.0", Units = "s"))
    float RecoveryTime = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap", meta = (ClampMin = "1.0", Units = "cm"))
    float TrapRadius = 260.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap", meta = (ClampMin = "0.0", Units = "s"))
    float ArmTime = 0.9f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap", meta = (ClampMin = "0.1", Units = "s"))
    float Lifetime = 8.0f;

    /** 적용하는 WebBound GameplayEffect의 실제 지속시간이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap|Bound", meta = (ClampMin = "0.1", Units = "s"))
    float BoundDuration = 2.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap|Bound", meta = (ClampMin = "0.05", Units = "s"))
    float EscapeInputWindow = 2.0f;

    /** RPC 지연을 고려해 탈출 입력을 더 받는 서버 시간이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap|Bound", meta = (ClampMin = "0.0", Units = "s"))
    float EscapeInputGrace = 0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap|Bound", meta = (ClampMin = "1"))
    int32 RequiredEscapeInputs = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap|Spawn", meta = (ClampMin = "1", ClampMax = "4"))
    int32 MaxTrapCount = 4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap|Bound")
    TSubclassOf<UGameplayEffect> WebBoundEffectClass;

    /** 복제 및 서버 Gameplay를 담당하는 Native Trap Actor 또는 그 BP Cosmetic 자식이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Web Trap|Spawn")
    TSubclassOf<AASBossSpiderWebTrap> TrapActorClass;
};

/** 보스 스파이더의 알 생성·부화·엘리트 소환 규칙의 단일 원본이다. */
USTRUCT(BlueprintType)
struct FASBossSpiderEggSummonConfig : public FASMonsterPatternRuntimeConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon", meta = (ClampMin = "0.0", Units = "s"))
    float CastWindupTime = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon", meta = (ClampMin = "0.0", Units = "s"))
    float RecoveryTime = 0.25f;

    /** 한 명의 플레이어를 상대로 만드는 기본 알 수다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Count", meta = (ClampMin = "1", ClampMax = "6"))
    int32 BaseEggCount = 3;

    /** 두 번째 살아 있는 플레이어부터 한 명당 추가하는 알 수다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Count", meta = (ClampMin = "0", ClampMax = "6"))
    int32 AdditionalEggsPerAlivePlayer = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Count", meta = (ClampMin = "1", ClampMax = "6"))
    int32 MaxEggCount = 6;

    /** 현재 타깃을 중심으로 한 알 생성 반경이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Placement", meta = (ClampMin = "0.0", Units = "cm"))
    float SpawnMinRadius = 350.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Placement", meta = (ClampMin = "0.0", Units = "cm"))
    float SpawnMaxRadius = 700.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float MinEggSpacing = 260.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Hatch", meta = (ClampMin = "0.05", Units = "s"))
    float HatchDelay = 4.0f;

    /** 부화 VFX/SFX를 보여 준 뒤 Native Egg Actor를 제거하기까지의 시간이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Hatch", meta = (ClampMin = "0.0", Units = "s"))
    float HatchDespawnDelay = 0.5f;

    /** Gameplay 수명주기를 담당하는 Native Egg Actor 또는 그 BP Cosmetic 자식이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Spawn")
    TSubclassOf<AASBossSpiderEgg> EggActorClass;

    /** 부화 시 생성하는 엘리트 몬스터 BP/C++ 클래스다. 해당 몬스터의 Definition은 그 클래스가 소유한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Egg Summon|Spawn")
    TSubclassOf<AASMonsterCharacter> EliteMinionClass;
};

/** 보스 스파이더의 상승·추적·착지·Shockwave 규칙의 단일 원본이다. */
USTRUCT(BlueprintType)
struct FASBossSpiderCeilingSlamConfig : public FASMonsterPatternRuntimeConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Timing", meta = (ClampMin = "0.05", Units = "s"))
    float RiseDuration = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Timing", meta = (ClampMin = "0.0", Units = "s"))
    float TargetTrackingDuration = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Timing", meta = (ClampMin = "0.05", Units = "s"))
    float LandingTelegraphDuration = 0.9f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Timing", meta = (ClampMin = "0.0", Units = "s"))
    float RecoveryTime = 0.35f;

    /** 상승 중 보스 Actor root가 실제로 이동하는 높이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Movement", meta = (ClampMin = "0.0", Units = "cm"))
    float RiseHeight = 1800.0f;

    /** 착지 시 보스와 플레이어 Capsule 반경 합에 추가하는 Native 충돌 분리 여유다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Movement", meta = (ClampMin = "0.0", Units = "cm"))
    float LandingCapsuleClearance = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Impact", meta = (ClampMin = "1.0", Units = "cm"))
    float LandingRadius = 600.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Impact", meta = (ClampMin = "0.0"))
    float LandingBaseDamage = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Impact", meta = (ClampMin = "0.0"))
    float LandingAttackPowerScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Shockwave", meta = (ClampMin = "0", ClampMax = "3"))
    int32 ShockwaveCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Shockwave", meta = (ClampMin = "0.0", Units = "s"))
    float ShockwaveInterval = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Shockwave", meta = (ClampMin = "1.0", Units = "cm"))
    float ShockwaveRadius = 850.0f;

    /** 착지 피해 대비 Shockwave 한 번의 피해 비율이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Shockwave", meta = (ClampMin = "0.0"))
    float ShockwaveDamageMultiplier = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Impact")
    EASImpactPreset ImpactPreset = EASImpactPreset::Heavy;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    /** 착지 직격에만 적용하는 상태효과다. Shockwave에는 중복 적용하지 않는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Effects")
    TSubclassOf<UGameplayEffect> OnLandingStatusEffectClass;

    /** 상승부터 착지까지 보스 자신에게 적용하는 Untargetable 효과다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Effects")
    TSubclassOf<UGameplayEffect> CeilingUntargetableEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ceiling Slam|Telegraph")
    TSubclassOf<AASMonsterAttackIndicatorActor> IndicatorClass;
};

/**
 * 보스 스파이더 PatternSet의 타입 검증용 자식이다.
 * 실제 수치와 구성은 PatternEntries 및 이 Asset의 보스 공통 Runtime 설정에만 저장한다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASBossSpiderPatternSet : public UASMonsterPatternSet
{
    GENERATED_BODY()

public:
    /** Pattern이 아닌 보스 본체 충돌 보정의 단일 데이터 원본이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Spider|Overlap Resolution")
    FASBossSpiderOverlapResolutionConfig OverlapResolution;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
