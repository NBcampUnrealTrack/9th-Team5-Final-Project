#pragma once
#include "CoreMinimal.h"
#include "ASPlayerMenuTypes.generated.h"

UENUM(BlueprintType)
enum class EASPlayerMenuTab : uint8 { Inventory, Quest, Modules, Crafting };

USTRUCT(BlueprintType)
struct FASPlayerMenuTabPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EASPlayerMenuTab Tab = EASPlayerMenuTab::Inventory;
	UPROPERTY(BlueprintReadOnly) FText Label;
	UPROPERTY(BlueprintReadOnly) bool bActive = false;
	bool operator==(const FASPlayerMenuTabPresentation& Other) const { return Tab == Other.Tab && Label.EqualTo(Other.Label) && bActive == Other.bActive; }
};

USTRUCT(BlueprintType)
struct FASPlayerMenuPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EASPlayerMenuTab ActiveTab = EASPlayerMenuTab::Inventory;
	UPROPERTY(BlueprintReadOnly) int32 ContentIndex = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FASPlayerMenuTabPresentation> Tabs;
	bool operator==(const FASPlayerMenuPresentation& Other) const { return ActiveTab == Other.ActiveTab && ContentIndex == Other.ContentIndex && Tabs == Other.Tabs; }
};
