#pragma once

#include "CoreMinimal.h"
#include "ASMonsterTargetTypes.generated.h"

class AActor;

/** 서버 런타임에서 범용 몬스터 타깃을 식별하는 Handle. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASTargetHandle
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass|Monster|Targeting")
	int32 Id = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass|Monster|Targeting")
	int32 Generation = 0;

	bool IsValid() const { return Id != 0; }

	friend bool operator==(const FASTargetHandle& Left, const FASTargetHandle& Right)
	{
		return Left.Id == Right.Id && Left.Generation == Right.Generation;
	}

	friend bool operator!=(const FASTargetHandle& Left, const FASTargetHandle& Right)
	{
		return !(Left == Right);
	}
};

FORCEINLINE uint32 GetTypeHash(const FASTargetHandle& Handle)
{
	return HashCombine(::GetTypeHash(Handle.Id), ::GetTypeHash(Handle.Generation));
}

/**
 * 구체 Actor 클래스가 아니라 몬스터 AI가 해당 대상을 어떤 목적으로 사용할 수 있는지를 나타낸다.
 * 여러 Capability를 함께 가질 수 있다.
 */
UENUM(BlueprintType, meta = (Bitflags))
enum class EASMonsterTargetCapability : uint8
{
	None = 0 UMETA(Hidden),
	PrimaryTarget = 1 << 0,
	BlockingTarget = 1 << 1,
	ThreatSource = 1 << 2,
	GlobalCandidate = 1 << 3
};
ENUM_CLASS_FLAGS(EASMonsterTargetCapability);

/** Target Registry가 Processor에 제공하는 읽기 전용 타깃 상태다. */
struct APOCALYPSESURVIVAL_API FASTargetSnapshot
{
	FASTargetHandle Handle;
	FVector Location = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Actor;
	EASMonsterTargetCapability Capabilities = EASMonsterTargetCapability::None;
	float BaseScore = 0.0f;
	float HitRadius = 0.0f;
	bool bAlive = false;
	bool bAttackable = false;
};

FORCEINLINE bool ASHasAllTargetCapabilities(
	const EASMonsterTargetCapability Available,
	const EASMonsterTargetCapability Required)
{
	const uint8 AvailableBits = static_cast<uint8>(Available);
	const uint8 RequiredBits = static_cast<uint8>(Required);
	return (AvailableBits & RequiredBits) == RequiredBits;
}

