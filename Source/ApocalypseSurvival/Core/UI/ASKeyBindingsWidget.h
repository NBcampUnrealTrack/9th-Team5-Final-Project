#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Commands/InputChord.h"
#include "ASKeyBindingsWidget.generated.h"

class UInputMappingContext;
class UInputKeySelector;
class UTextBlock;
class UVerticalBox;

UCLASS()
class APOCALYPSESURVIVAL_API UASKeyBindingRowWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetMapping(UInputMappingContext* Context, int32 Index);
protected:
	virtual void NativeOnInitialized() override;
private:
	UFUNCTION()
	void HandleKeySelected(FInputChord Key);
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient)
	TObjectPtr<UInputKeySelector> KeySelector;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ErrorLabel;
	int32 MappingIndex = INDEX_NONE;
	bool bUpdatingKey = false;
};

UCLASS()
class APOCALYPSESURVIVAL_API UASKeyBindingsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetMappingContext(UInputMappingContext* Context);
protected:
	virtual void NativeOnInitialized() override;
private:
	void RebuildRows();
	UFUNCTION()
	void ResetKeys();
	UFUNCTION()
	void ClosePanel();
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Rows;
};
