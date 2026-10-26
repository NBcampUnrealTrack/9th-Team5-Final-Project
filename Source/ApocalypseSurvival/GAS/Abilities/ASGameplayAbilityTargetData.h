#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "ASGameplayAbilityTargetData.generated.h"

/**
 * A stable Mass replication ID carried with an ability target-data request.
 * The value is only a client hint; the server resolves and validates it against
 * its own authoritative Mass entities before using it.
 */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASGameplayAbilityTargetData_MassTarget : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 MassNetworkId = 0;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FASGameplayAbilityTargetData_MassTarget::StaticStruct();
	}

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << MassNetworkId;
		bOutSuccess = !Ar.IsError();
		return bOutSuccess;
	}
};

template<>
struct TStructOpsTypeTraits<FASGameplayAbilityTargetData_MassTarget>
	: public TStructOpsTypeTraitsBase2<FASGameplayAbilityTargetData_MassTarget>
{
	enum
	{
		WithNetSerializer = true
	};
};
