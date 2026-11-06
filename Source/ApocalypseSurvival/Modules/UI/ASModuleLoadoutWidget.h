#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASModuleLoadoutWidget.generated.h"

/** In-run equipment overview. Lobby equipment selection remains authoritative. */
UCLASS()
class APOCALYPSESURVIVAL_API UASModuleLoadoutWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Modules|ViewModel")
	class UASModuleLoadoutViewModel* GetViewModel() const { return ViewModel; }
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void Refresh();
	UPROPERTY(Transient) TObjectPtr<class UVerticalBox> Rows;
	UPROPERTY(Transient) TObjectPtr<UASModuleLoadoutViewModel> ViewModel;
};
