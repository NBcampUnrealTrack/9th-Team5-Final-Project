#pragma once
#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Items/UI/ViewModels/ASPlayerMenuTypes.h"
#include "ASPlayerMenuViewModel.generated.h"

class APlayerController;

/** Shared tab state and menu navigation commands. Does not own or mutate gameplay data. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASPlayerMenuViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(APlayerController* Controller, EASPlayerMenuTab Screen);
	UFUNCTION(BlueprintCallable, Category = "UI|Menu") void RequestTab(EASPlayerMenuTab Tab);
	UFUNCTION(BlueprintCallable, Category = "UI|Menu") void RequestClose();
	/** Synchronize a displayed screen without issuing another navigation request. */
	void SetActiveTab(EASPlayerMenuTab Tab);
	EASPlayerMenuTab GetScreen() const { return ScreenTab; }
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "UI|Menu") FASPlayerMenuPresentation Presentation;
private:
	TWeakObjectPtr<APlayerController> Player;
	EASPlayerMenuTab ScreenTab = EASPlayerMenuTab::Inventory;
};
