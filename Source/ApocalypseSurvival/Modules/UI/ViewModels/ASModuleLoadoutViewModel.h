#pragma once
#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ASModuleLoadoutViewModel.generated.h"

class APlayerController;
class APawn;

USTRUCT(BlueprintType)
struct FASModuleSlotPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText SlotLabel;
	UPROPERTY(BlueprintReadOnly) FText Name;
	UPROPERTY(BlueprintReadOnly) FText Description;
	UPROPERTY(BlueprintReadOnly) FText Effects;
	UPROPERTY(BlueprintReadOnly) bool bEquipped = false;
	bool operator==(const FASModuleSlotPresentation& Other) const { return SlotLabel.EqualTo(Other.SlotLabel) && Name.EqualTo(Other.Name) && Description.EqualTo(Other.Description) && Effects.EqualTo(Other.Effects) && bEquipped == Other.bEquipped; }
};

USTRUCT(BlueprintType)
struct FASModuleLoadoutPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FASModuleSlotPresentation> Slots;
	bool operator==(const FASModuleLoadoutPresentation& Other) const { return Slots == Other.Slots; }
};

/** Read-only local projection of the replicated in-run loadout. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASModuleLoadoutViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(APlayerController* Controller);
	void Shutdown();
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category = "Modules|ViewModel") void Refresh();
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Modules|ViewModel") FASModuleLoadoutPresentation Presentation;
private:
	UFUNCTION() void HandlePawnChanged(APawn* Pawn);
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<class AASPlayerController> ObservedController;
	TWeakObjectPtr<class UASModuleComponent> Modules;
};
