#pragma once

#include "CoreMinimal.h"
#include "Items/GAS/ASTargetedItemUseAbility.h"
#include "ASBarricadeUseAbility.generated.h"

class AASBarricadeActor;

/** Preview locally, confirm a position, then validate/place and consume on the server. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASBarricadeUseAbility : public UASTargetedItemUseAbility
{
	GENERATED_BODY()

public:
	UASBarricadeUseAbility();

protected:
	virtual void ActivateItemUse(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Barricade")
	TSoftClassPtr<AASBarricadeActor> BarricadeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade", meta = (ClampMin = "1", Units = "s"))
	float TargetingTimeout = 30.0f;

private:
	virtual void BeginTargeting() override;
	virtual float GetTargetRange() const override;
	virtual bool IsTargetLocationValid(const FVector& Location) const override;
	virtual void ExecuteAtLocation(const FVector& Location) override;
	virtual void UpdateTargetingPreview() override;
	virtual void CleanupTargetingVisuals() override;
	void PlaceAtLocation(const FVector& DesiredLocation);
	void UpdatePlacementPreview();

	UPROPERTY(Transient)
	TObjectPtr<AASBarricadeActor> PreviewActor;

};
