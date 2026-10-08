#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASItemUseComponent.generated.h"

class APawn;
class UASInventoryComponent;

/** Server-side use orchestration. Inventory owns quantities; GAS owns effects/costs.
 * Existing interaction RPCs call Inventory.UseItem, which creates this component on demand.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASItemUseComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UASItemUseComponent();

	/** True means activation was accepted, not that a delayed use has committed yet. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|Use")
	bool UseItem(UASInventoryComponent* Inventory, FGuid InstanceId, APawn* Avatar);
};
