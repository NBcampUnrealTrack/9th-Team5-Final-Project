#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ASWeaponAttackExecution.generated.h"

class APawn;
class AActor;
class UAbilitySystemComponent;
class UASInventoryComponent;
class UASItemDefinition;
class UASItemInstance;
class UASWeaponAttackProfile;

/** 일반 공격에서 조준 방향을 만드는 방식. 무기의 피해·탄약 규칙과는 독립적이다. */
UENUM()
enum class EASPrimaryAttackMode : uint8
{
	AttackMove,
	Directional
};

struct FASWeaponAttackExecutionContext
{
	APawn* Avatar = nullptr;
	UAbilitySystemComponent* AbilitySystem = nullptr;
	UASInventoryComponent* Inventory = nullptr;
	UASItemInstance* Weapon = nullptr;
	const UASItemDefinition* Definition = nullptr;
	const UASWeaponAttackProfile* Profile = nullptr;
	EASPrimaryAttackMode AttackMode = EASPrimaryAttackMode::AttackMove;
	FVector AimDirection = FVector::ForwardVector;
	/** 서버가 검증한 명시적 타깃. 방향성 투사체는 비어 있다. */
	TWeakObjectPtr<AActor> RequestedTarget;
	/** 클라이언트가 제안하고 서버가 검증한 Mass replication ID. */
	uint32 RequestedMassTargetNetworkId = 0;
	FVector TargetLocation = FVector::ZeroVector;
};

/** Stateless strategy selected by the equipped weapon's attack profile. */
UCLASS(Abstract, Blueprintable, Const)
class APOCALYPSESURVIVAL_API UASWeaponAttackExecution : public UObject
{
	GENERATED_BODY()

public:
	virtual bool CanPayCost(const FASWeaponAttackExecutionContext& Context) const PURE_VIRTUAL(UASWeaponAttackExecution::CanPayCost, return false;);
	virtual bool CommitCost(const FASWeaponAttackExecutionContext& Context) const PURE_VIRTUAL(UASWeaponAttackExecution::CommitCost, return false;);
	virtual void ExecuteHit(const FASWeaponAttackExecutionContext& Context) const PURE_VIRTUAL(UASWeaponAttackExecution::ExecuteHit, );
};

UCLASS()
class APOCALYPSESURVIVAL_API UASMeleeAttackExecution : public UASWeaponAttackExecution
{
	GENERATED_BODY()

public:
	virtual bool CanPayCost(const FASWeaponAttackExecutionContext& Context) const override;
	virtual bool CommitCost(const FASWeaponAttackExecutionContext& Context) const override;
	virtual void ExecuteHit(const FASWeaponAttackExecutionContext& Context) const override;
};

UCLASS()
class APOCALYPSESURVIVAL_API UASRangedAttackExecution : public UASWeaponAttackExecution
{
	GENERATED_BODY()

public:
	virtual bool CanPayCost(const FASWeaponAttackExecutionContext& Context) const override;
	virtual bool CommitCost(const FASWeaponAttackExecutionContext& Context) const override;
	virtual void ExecuteHit(const FASWeaponAttackExecutionContext& Context) const override;
};

/** Starts a server-owned, time-spaced rifle burst. Cost is committed once by the owning ability. */
UCLASS()
class APOCALYPSESURVIVAL_API UASRifleBurstAttackExecution : public UASRangedAttackExecution
{
	GENERATED_BODY()

public:
	virtual void ExecuteHit(const FASWeaponAttackExecutionContext& Context) const override;
};

/** Spawns a server-owned shotgun pellet fan. Each target can receive crowd control once per attack. */
UCLASS()
class APOCALYPSESURVIVAL_API UASShotgunAttackExecution : public UASRangedAttackExecution
{
	GENERATED_BODY()

public:
	virtual void ExecuteHit(const FASWeaponAttackExecutionContext& Context) const override;
};

/** Fires one long-range projectile that applies decreasing damage to unique targets in hit order. */
UCLASS()
class APOCALYPSESURVIVAL_API UASSniperAttackExecution : public UASRangedAttackExecution
{
	GENERATED_BODY()

public:
	virtual void ExecuteHit(const FASWeaponAttackExecutionContext& Context) const override;
};
