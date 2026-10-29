#pragma once

#include "CoreMinimal.h"
#include "ASCombatImpactTypes.generated.h"

/**
 * 공격이 전달하는 제어력 프리셋이다.
 * 피해량과 독립적으로 선택하며, 개별 공격에 경직 시간이나 강인도 수치를 추가하지 않는다.
 */
UENUM(BlueprintType)
enum class EASImpactPreset : uint8
{
	None UMETA(DisplayName = "None"),
	Standard UMETA(DisplayName = "Standard"),
	Rapid UMETA(DisplayName = "Rapid"),
	Heavy UMETA(DisplayName = "Heavy"),
	Knockdown UMETA(DisplayName = "Knockdown")
};

/** 몬스터 원형이 가지는 경직 저항 등급이다. 플레이어는 별도 규칙을 사용한다. */
UENUM(BlueprintType)
enum class EASStaggerResistance : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Elite UMETA(DisplayName = "Elite"),
	Boss UMETA(DisplayName = "Boss")
};

/** 프리셋과 저항 등급을 조합해 결정되는 최종 피격 결과다. */
UENUM(BlueprintType)
enum class EASResolvedHitReaction : uint8
{
	None UMETA(DisplayName = "None"),
	VisualOnly UMETA(DisplayName = "Visual Only"),
	WeakStagger UMETA(DisplayName = "Weak Stagger"),
	StrongStagger UMETA(DisplayName = "Strong Stagger"),
	Knockdown UMETA(DisplayName = "Knockdown")
};

/**
 * 피해 Effect와 별개로 한 공격의 경직 의도를 전달하는 런타임 정보다.
 * Actor와 Mass 대상이 동일한 프리셋으로 판정하도록 CombatTargetSubsystem 경로에서 사용한다.
 */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASCombatImpactContext
{
	GENERATED_BODY()

	/** None이면 피해와 피격 표현만 적용하고 실제 경직은 요청하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Impact")
	EASImpactPreset Preset = EASImpactPreset::None;

	/** Ability 활성화 또는 투사체 한 발에 부여하는 0 이상의 식별자. 적중 중복 방지와 디버깅에 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Impact", meta = (ClampMin = "0", UIMin = "0"))
	int32 AttackInstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Impact")
	FVector_NetQuantize HitLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Impact")
	FVector_NetQuantizeNormal HitDirection = FVector::ForwardVector;

	/** Normal 저항 몬스터에만 적용하는 서버 권한 위치 이동 거리. 0이면 물리 넉백 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Impact", meta = (ClampMin = "0.0", Units = "cm"))
	float KnockbackDistance = 0.0f;

	bool RequestsControlReaction() const { return Preset != EASImpactPreset::None; }
};
