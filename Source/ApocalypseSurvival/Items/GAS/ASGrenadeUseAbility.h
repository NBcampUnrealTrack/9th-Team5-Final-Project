#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASTargetedItemUseAbility.h"
#include "ASGrenadeUseAbility.generated.h"

class AASGrenadeActor;
class AASGrenadeTrajectoryPreview;
class AASSkillRangeIndicatorActor;

/** Cursor-targeted consumable. Previews the bouncing trajectory and blast, then commits inventory cost on the server. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASGrenadeUseAbility : public UASTargetedItemUseAbility
{
	GENERATED_BODY()

public:
	UASGrenadeUseAbility();

protected:
	virtual void ActivateItemUse(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade")
	TSoftClassPtr<AASGrenadeActor> GrenadeClass;

	/** Optional visual override. Empty reuses the equipped character's E skill indicator, then another configured skill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Targeting")
	TSoftClassPtr<AASSkillRangeIndicatorActor> RangeIndicatorClass;

	/** Ballistic flight time to the selected ground point when unobstructed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade", meta = (ClampMin = "0.05", Units = "s"))
	float ThrowFlightTime = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade|Targeting", meta = (ClampMin = "1.0", Units = "s"))
	float TargetingTimeout = 30.0f;

	/** Offset relative to horizontal facing. Grenades are thrown from near the avatar's upper body. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grenade", meta = (Units = "cm"))
	FVector SpawnOffset = FVector(50.0f, 0.0f, 30.0f);

private:
	virtual void BeginTargeting() override;
	virtual float GetTargetRange() const override;
	virtual void ExecuteAtLocation(const FVector& Location) override;
	virtual void CleanupTargetingVisuals() override;
	void ThrowAtLocation(const FVector& DesiredLocation);

	UPROPERTY(Transient)
	TObjectPtr<AASSkillRangeIndicatorActor> RangeIndicator;

	UPROPERTY(Transient)
	TObjectPtr<AASGrenadeTrajectoryPreview> TrajectoryPreview;

};
