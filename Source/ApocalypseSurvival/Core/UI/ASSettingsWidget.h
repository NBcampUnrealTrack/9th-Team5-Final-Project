#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "ASSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class USlider;
class UTextBlock;
class UInputMappingContext;
class UASKeyBindingsWidget;

UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void RefreshSettings();
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ApplyGameSettings();
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void OpenKeyBindings();
	void CloseKeyBindings();
	FSimpleDelegate OnBackRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Input")
	TObjectPtr<UInputMappingContext> KeyMappingContext;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> WindowModeComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ResolutionComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> MaxFPSComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> QualityComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ShadowQualityComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> TextureQualityComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> EffectsQualityComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> AntiAliasingQualityComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> SmartCastComboBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> VSyncCheckBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterVolumeSlider;
	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterVolumePercentage;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ApplyButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> KeyBindingsButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SettingsStatusText;

private:
	UFUNCTION()
	void HandleBackRequested();
	UFUNCTION()
	void HandleOverallQualityChanged(FString Item, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleIndividualQualityChanged(FString Item, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleVolumeChanged(float Value);
	void PopulateOptions();
	TArray<FIntPoint> Resolutions;
	TArray<float> FrameRates;
	bool bUpdatingUI = false;
	int32 PendingOverallQuality = INDEX_NONE;
	UPROPERTY(Transient)
	TObjectPtr<UASKeyBindingsWidget> KeyBindingsWidget;
};
