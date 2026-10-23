#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Items/Data/ASItemTypes.h"
#include "ASItemRarityBackground.generated.h"

class SASItemRarityBackground;

/** Dark rarity gradient and a bottom accent, drawn behind the unmodified item icon. */
UCLASS()
class APOCALYPSESURVIVAL_API UASItemRarityBackground : public UWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Item|Appearance")
	void SetRarity(EASLootRarity InRarity);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	UPROPERTY(EditAnywhere, Category = "Item|Appearance")
	EASLootRarity Rarity = EASLootRarity::Uncommon;
private:
	TSharedPtr<SASItemRarityBackground> Background;
};
