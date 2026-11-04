#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "ASMonsterDefinition.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UASMonsterHealthBarWidget;
class UASMonsterPatternSet;
class UASMonsterStatDefinition;
class UStateTree;

/** 장판(Telegraph) 인디케이터의 모양 종류. */
UENUM(BlueprintType)
enum class ETelegraphShape : uint8
{
    None        UMETA(DisplayName = "None"),
    Rectangle   UMETA(DisplayName = "Rectangle"),
    Radial      UMETA(DisplayName = "Radial"),
    Sector      UMETA(DisplayName = "Sector")
};

/** 몬스터가 상황에 따라 선택해서 실행하는 공격 패턴 하나. */
USTRUCT(BlueprintType)
struct FASMonsterAttackPattern
{
    GENERATED_BODY()

    /** 패턴 식별자. 쿨다운 추적과 디버그 로그에 사용한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (Categories = "Ability.Monster.Attack"))
    FGameplayTag PatternId;

    /** 이 패턴이 선택됐을 때 서버가 부여·활성화할 Ability. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern")
    TSubclassOf<UGameplayAbility> AbilityClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "cm"))
    float MinRange = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxRange = 200.0f;

    /**
     * 실제 타격/투사체/이동 Ability가 보장하는 최대 타깃 캡슐 표면 거리.
     * 0이면 MaxRange만 사용한다. 0보다 크면 패턴 선택 시 MaxRange와 이 값 중 작은 값을 사용한다.
     * 돌진처럼 Ability 내부 이동 거리가 있는 패턴은, 닿지 않는 거리에서 시전되지 않도록 실제 도달 가능 거리로 설정한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Reach", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxGuaranteedTargetSurfaceDistance = 0.0f;

    /** 사용 가능한 패턴끼리 선택할 때의 상대 확률. 0이면 선택 후보에서 제외된다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0"))
    float SelectionWeight = 1.0f;

    /** 동일 패턴을 다시 선택할 수 있기까지의 시간. Ability 활성화 성공 시점부터 잰다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "s"))
    float Cooldown = 1.0f;

    /**
     * 전역 패턴 간 딜레이 중에도 이 패턴을 시작할 수 있는지.
     * 기본값 false는 일반 패턴이 보스의 숨 고르기 시간을 지키게 한다.
     * Director가 명시적으로 호출하는 긴급/전환 패턴에만 true로 설정한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Timing")
    bool bCanStartDuringInterPatternDelay = false;

    /** 이 패턴을 선택할 수 있는 몬스터 체력 비율의 하한. 0이면 체력 조건이 없다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinOwnerHealthPercent = 0.0f;

    /** 이 패턴을 선택할 수 있는 몬스터 체력 비율의 상한. 1이면 체력 조건이 없다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pattern|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxOwnerHealthPercent = 1.0f;

    /** 장판 인디케이터의 모양 종류. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph")
    ETelegraphShape ShapeType = ETelegraphShape::Rectangle;

    /** 장판의 크기 (Rectangle 기준 X: 전방 길이의 절반, Y: 좌우 폭의 절반, Z: 높이). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph", meta = (EditCondition = "ShapeType == ETelegraphShape::Rectangle", EditConditionHides))
    FVector BoxExtent = FVector(150.0f, 75.0f, 100.0f);

    /** Sector 기준 반경. Rectangle/ Radial은 기존 BoxExtent 값을 계속 사용한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph", meta = (EditCondition = "ShapeType == ETelegraphShape::Sector", EditConditionHides, ClampMin = "0.0", Units = "cm"))
    float SectorRadius = 350.0f;

    /** Sector 기준 전체 각도. 보스 정면을 중심으로 좌우에 균등하게 펼쳐진다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph", meta = (EditCondition = "ShapeType == ETelegraphShape::Sector", EditConditionHides, ClampMin = "1.0", ClampMax = "360.0", Units = "deg"))
    float SectorAngleDegrees = 110.0f;

    /** 예고 장판이 바닥에 지속되는 선딜레이 시간 (초). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph", meta = (ClampMin = "0.0", Units = "s"))
    float TelegraphDuration = 0.8f;

    /** 장판 타격 시 적용될 기본 데미지 수치. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telegraph", meta = (ClampMin = "0.0"))
    float BaseDamage = 50.0f;
};

/** 일반 몬스터 원형의 초기 전투 수치. */
USTRUCT(BlueprintType)
struct FASMonsterInitialAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0.0", Units = "cm/s"))
    float MoveSpeed = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0.0"))
    float AttackPower = 10.0f;
};

/**
 * 근접형, 원거리형 등의 차이를 Character 상속 대신 데이터로 정의한다.
 * StateTree는 이 자산을 읽어 타깃 거리와 활성화할 공격 Ability를 결정한다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASMonsterDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster")
    FText DisplayName;

    /** 플레이어가 이 몬스터를 처치했을 때 지급할 기본 경험치. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0"))
    int32 ExperienceReward = 10;

    /** 몬스터 처치 시 추첨할 드랍 그룹 행. 비어 있으면 아이템을 드랍하지 않는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards",
       meta = (RowType = "/Script/ApocalypseSurvival.ASMonsterDropGroupRow"))
    FDataTableRowHandle DropGroup;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster")
    FASMonsterInitialAttributes InitialAttributes;

    /** 이 몬스터가 받는 공격 프리셋을 어떤 최종 경직으로 바꿀지 결정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reaction")
    EASStaggerResistance StaggerResistance = EASStaggerResistance::Normal;

    /**
     * Actor와 Mass가 공유하는 스탯 원본. 비어 있으면 InitialAttributes를 사용해 기존 에셋을 호환한다.
     * 새 몬스터는 이 자산을 지정해 라운드 성장과 공통 계산 규칙을 사용한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Stats")
    TObjectPtr<UASMonsterStatDefinition> StatDefinition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
    float AttackRange = 180.0f;

    /** 0이면 AttackRange를 사용한다. 원거리 몬스터가 공격을 시도하며 유지할 목표 거리다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
    float PreferredCombatDistance = 0.0f;

    /** 0이면 후퇴하지 않는다. 이 거리보다 가까운 타깃에게서 뒤로 이동한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
    float RetreatDistance = 0.0f;

    /** 이 거리 안의 가장 가까운 살아 있는 플레이어를 타깃으로 잡는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
    float DetectionRadius = 1500.0f;

    /** 타깃이 이 거리보다 멀어지면 놓아준다. 탐지 반경보다 작게 설정해도 탐지 반경으로 보정된다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
    float LoseTargetRadius = 2200.0f;

    /** 피해를 받았을 때 같은 공격자에게 어그로를 전파할 주변 몬스터 탐색 반경. 0이면 전파하지 않는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
    float AssistAggroRadius = 900.0f;

    /** 직접 피해를 받았을 때 공격자에게 부여할 어그로. 최대치는 100이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
    float DirectHitAggro = 100.0f;

    /** 타깃이 추적 해제 거리 밖에 있을 때 초당 잃는 어그로. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
    float AggroDecayPerSecond = 25.0f;

    /** 바로 인접한 일반 타깃이 받는 최대 거리 점수. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
    float ProximityScoreMax = 30.0f;

    /** 이 거리에서 거리 점수가 최대치의 절반이 된다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "1.0", Units = "cm"))
    float ProximityHalfScoreDistance = 600.0f;

    /** 새 후보가 현재 타깃보다 이 점수만큼 높아야 일반 전환한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
    float TargetSwitchScoreMargin = 5.0f;

    /** 타깃 변경 뒤 일반 전환을 막는 시간이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "s"))
    float MinTargetLockSeconds = 0.75f;

    /** PrimaryTarget 경로 실패 또는 이동 정체 시 차단 후보를 탐색할 전방 반경이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Navigation|Blocking", meta = (ClampMin = "0.0", Units = "cm"))
    float BlockingTargetSearchRadius = 600.0f;

    /** 차단 후보가 PrimaryTarget 방향과 이루어야 하는 최소 2D 내적이다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Navigation|Blocking", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float BlockingTargetMinForwardDot = 0.25f;

    /** 정상적인 이동 요청 중 이 시간만큼 진행하지 못하면 BlockingTarget 탐색을 시작한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Navigation|Blocking", meta = (ClampMin = "0.0", Units = "s"))
    float BlockingTargetStuckSeconds = 0.75f;

    /** 정체 판정에서 실제 진행으로 인정할 최소 2D 이동 거리다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Navigation|Blocking", meta = (ClampMin = "1.0", Units = "cm"))
    float BlockingTargetMinProgressDistance = 20.0f;

    /** CombatComponent가 활성화할 Ability의 태그. 예: Ability.Monster.Attack.Ranged */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Ability.Monster"))
    FGameplayTag AttackAbilityTag;

    /** 서버에서 이 몬스터 ASC에 한 번 부여할 기본 Ability 목록. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

    /** 모든 패턴의 유일한 데이터 원본. 보스/엘리트 전용 수치는 이 Asset에만 둔다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Pattern Set")
    TObjectPtr<UASMonsterPatternSet> PatternSet;

    /**
     * Legacy 호환 경로. 새 몬스터와 이관 완료 보스는 PatternSet만 사용한다.
     * 기존 Data Asset의 직렬화 데이터를 보존하기 위해 PatternSet 이관 확인 전까지 삭제하지 않는다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Legacy", meta = (TitleProperty = "PatternId", DeprecationMessage = "Use PatternSet instead."))
    TArray<FASMonsterAttackPattern> AttackPatterns;

    /** 사망 시 충돌을 끈 뒤 월드에 남겨 둘 시간. 0이면 즉시 제거한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0.0", Units = "s"))
    float DeathDespawnDelay = 1.5f;

    /** 실제 경직 때 적용할 Duration GameplayEffect. 지속 시간은 Combat Impact 전역 설정이 덮어쓴다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reaction")
    TSubclassOf<UGameplayEffect> HitReactEffect;

    /** 이 몬스터 원형이 머리 위 월드 체력바를 사용할지 결정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    bool bUseWorldHealthBar = false;

    /** 생성 직후부터 체력바를 표시한다. 꺼 두면 전투 진입 시 SetHealthBarVisible로 켤 수 있다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
       meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides))
    bool bHealthBarInitiallyVisible = true;

    /** 비어 있으면 코드에서 제공하는 기본 체력바 Widget을 사용한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
       meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides))
    TSubclassOf<UASMonsterHealthBarWidget> HealthBarWidgetClass;

    /** Character 원점에서 체력바까지의 상대 위치. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
       meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides, Units = "cm"))
    FVector HealthBarOffset = FVector(0.0f, 0.0f, 140.0f);

    /** Widget의 DesiredSize가 계산되기 전 첫 프레임에 사용하는 안전한 초기 크기. 이후에는 WBP 크기가 우선한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
       meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides, ClampMin = "1.0"))
    FVector2D HealthBarDrawSize = FVector2D(250.0f, 30.0f);

    /** 줌아웃할수록 체력바를 제한적으로 축소한다. 몬스터별 Tick은 사용하지 않는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
       meta = (EditCondition = "bUseWorldHealthBar", EditConditionHides))
    bool bScaleHealthBarWithCameraZoom = true;

    /** 카메라가 가장 가까울 때의 체력바 크기 배율. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
       meta = (EditCondition = "bUseWorldHealthBar && bScaleHealthBarWithCameraZoom", EditConditionHides, ClampMin = "0.1"))
    float HealthBarScaleAtMinZoomDistance = 1.0f;

    /** 카메라가 가장 멀 때의 체력바 크기 배율. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI",
       meta = (EditCondition = "bUseWorldHealthBar && bScaleHealthBarWithCameraZoom", EditConditionHides, ClampMin = "0.1"))
    float HealthBarScaleAtMaxZoomDistance = 0.75f;

    /** 몬스터의 전반 행동을 결정하는 StateTree. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
    TObjectPtr<UStateTree> StateTree;

	/**
	 * Main StateTree의 StateTree.Slot.Combat Linked Asset을 교체한다.
	 * 비어 있으면 Main Tree에 지정한 기본 Combat Asset을 그대로 사용하므로 기존 몬스터는 호환된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|StateTree")
	FStateTreeReference CombatStateTree;
};
