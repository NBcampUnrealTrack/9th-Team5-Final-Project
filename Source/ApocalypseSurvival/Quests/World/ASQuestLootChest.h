#pragma once
#include "CoreMinimal.h"
#include "Items/Loot/ASLootChest.h"
#include "ASQuestLootChest.generated.h"

class UASItemDefinition;

/** Existing chest interaction/UI, with a personal quest-item entitlement for each eligible player. */
UCLASS()
class APOCALYPSESURVIVAL_API AASQuestLootChest : public AASLootChest
{
	GENERATED_BODY()
public:
	AASQuestLootChest();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TSoftObjectPtr<UASItemDefinition> QuestItem;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "1"))
	int32 QuestItemQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText ChestDisplayName;
	/** One chest per authored group is selected per run. Empty disables selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName RandomSelectionGroup;
protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
private:
	bool bSelectionResolved = false;
};
