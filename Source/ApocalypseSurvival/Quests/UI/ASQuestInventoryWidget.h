#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASQuestInventoryWidget.generated.h"

class UASQuestInventoryViewModel;
class UVerticalBox;

/** Quest-tab checklist and personal quest bag. No general-inventory slot, use, trade or drop commands. */
UCLASS()
class APOCALYPSESURVIVAL_API UASQuestInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Quest|ViewModel")
	UASQuestInventoryViewModel* GetViewModel() const { return ViewModel; }
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void Refresh();
	void AddLine(const FText& Text, FLinearColor Color = FLinearColor::White);
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
	UPROPERTY(Transient) TObjectPtr<UASQuestInventoryViewModel> ViewModel;
};
