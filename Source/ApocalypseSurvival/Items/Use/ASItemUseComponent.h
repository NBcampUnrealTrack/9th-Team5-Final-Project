#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASItemUseComponent.generated.h"

class APawn;
class UASInventoryComponent;

/** Server-side use orchestration. Weapons delegate to equipment; consumables activate GAS.
 * Existing interaction RPCs call Inventory.UseItem, which creates this component on demand.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASItemUseComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UASItemUseComponent();

	/** Toggles weapon equipment or activates a consumable. True accepts use; delayed GAS may commit later. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|Use")
	bool UseItem(UASInventoryComponent* Inventory, FGuid InstanceId, APawn* Avatar);
};
