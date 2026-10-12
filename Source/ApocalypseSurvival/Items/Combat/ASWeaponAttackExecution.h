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

struct FASWeaponAttackExecutionContext
{
	APawn* Avatar = nullptr;
	UAbilitySystemComponent* AbilitySystem = nullptr;
	UASInventoryComponent* Inventory = nullptr;
	UASItemInstance* Weapon = nullptr;
	const UASItemDefinition* Definition = nullptr;
	const UASWeaponAttackProfile* Profile = nullptr;
	FVector AimDirection = FVector::ForwardVector;
	/** 서버가 검증한 명시적 타깃. 방향성 투사체는 비어 있다. */
	TWeakObjectPtr<AActor> RequestedTarget;
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
