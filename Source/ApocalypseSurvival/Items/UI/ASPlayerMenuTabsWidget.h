#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/UI/ViewModels/ASPlayerMenuTypes.h"
#include "ASPlayerMenuTabsWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASMenuTabSelected, EASPlayerMenuTab, Tab);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASMenuCloseRequested);

/** The same navigation is used by inventory, quests, modules and the separate crafting window. */
UCLASS()
class APOCALYPSESURVIVAL_API UASPlayerMenuTabsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ConfigureScreen(EASPlayerMenuTab Screen);
	void SetActiveTab(EASPlayerMenuTab Tab);
	UFUNCTION(BlueprintPure, Category = "UI|ViewModel")
	class UASPlayerMenuViewModel* GetViewModel() const { return ViewModel; }
	UPROPERTY(BlueprintAssignable) FASMenuTabSelected OnTabSelected;
	UPROPERTY(BlueprintAssignable) FASMenuCloseRequested OnCloseRequested;
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void Refresh();
	UPROPERTY(Transient) TObjectPtr<UASPlayerMenuViewModel> ViewModel;
	UFUNCTION() void Inventory();
	UFUNCTION() void Quest();
	UFUNCTION() void Modules();
	UFUNCTION() void Crafting();
	UFUNCTION() void Close();
	UPROPERTY(Transient) TArray<TObjectPtr<class UButton>> Buttons;
	UPROPERTY(Transient) TArray<TObjectPtr<class UTextBlock>> Labels;
	UPROPERTY(Transient) TArray<TObjectPtr<class UBorder>> Indicators;
};
