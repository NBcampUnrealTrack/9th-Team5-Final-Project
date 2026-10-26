#include "Core/UI/ASSettingsWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Core/Settings/ASLocalSettingsSubsystem.h"
#include "Core/UI/ASKeyBindingsWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Skills/Settings/ASSkillSettingsSubsystem.h"

namespace
{
	void FillQualityOptions(UComboBoxString* Combo, const bool bAllowCustom)
	{
		if (!Combo) return;
		Combo->ClearOptions();
		for (const TCHAR* Label : { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("매우 높음"), TEXT("시네마틱") }) Combo->AddOption(Label);
		if (bAllowCustom) Combo->AddOption(TEXT("사용자 지정"));
	}

	int32 ReadQuality(const UComboBoxString* Combo, const int32 Current)
	{
		const int32 Index = Combo ? Combo->GetSelectedIndex() : INDEX_NONE;
		return Index >= 0 && Index <= 4 ? Index : Current;
	}
}

void UASSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ApplyButton) ApplyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ApplyGameSettings);
	if (KeyBindingsButton) KeyBindingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenKeyBindings);
	if (BackButton) BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RefreshSettings);
	if (QualityComboBox) QualityComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleOverallQualityChanged);
	for (UComboBoxString* Combo : { ShadowQualityComboBox.Get(), TextureQualityComboBox.Get(), EffectsQualityComboBox.Get(), AntiAliasingQualityComboBox.Get() })
	{
		if (Combo) Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleIndividualQualityChanged);
	}
	if (MasterVolumeSlider) MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleVolumeChanged);
	RefreshSettings();
}

void UASSettingsWidget::NativeDestruct()
{
	if (ApplyButton) ApplyButton->OnClicked.RemoveDynamic(this, &ThisClass::ApplyGameSettings);
	if (KeyBindingsButton) KeyBindingsButton->OnClicked.RemoveDynamic(this, &ThisClass::OpenKeyBindings);
	if (BackButton) BackButton->OnClicked.RemoveDynamic(this, &ThisClass::RefreshSettings);
	if (QualityComboBox) QualityComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleOverallQualityChanged);
	for (UComboBoxString* Combo : { ShadowQualityComboBox.Get(), TextureQualityComboBox.Get(), EffectsQualityComboBox.Get(), AntiAliasingQualityComboBox.Get() })
	{
		if (Combo) Combo->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleIndividualQualityChanged);
	}
	if (MasterVolumeSlider) MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleVolumeChanged);
	if (KeyBindingsWidget) KeyBindingsWidget->RemoveFromParent();
	Super::NativeDestruct();
}

void UASSettingsWidget::PopulateOptions()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;
	if (WindowModeComboBox)
	{
		WindowModeComboBox->ClearOptions();
		for (const TCHAR* Label : { TEXT("전체 화면"), TEXT("테두리 없는 창"), TEXT("창 모드") }) WindowModeComboBox->AddOption(Label);
	}
	Resolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	const FIntPoint Current = Settings->GetScreenResolution();
	if (Current.X > 0 && Current.Y > 0) Resolutions.AddUnique(Current);
	if (Resolutions.IsEmpty()) Resolutions.Add(FIntPoint(1280, 720));
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
	if (ResolutionComboBox)
	{
		ResolutionComboBox->ClearOptions();
		for (const FIntPoint& Resolution : Resolutions) ResolutionComboBox->AddOption(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
	}
	FrameRates = { 30.0f, 60.0f, 90.0f, 120.0f, 144.0f, 165.0f, 240.0f };
	if (Settings->GetFrameRateLimit() > 0.0f) FrameRates.AddUnique(Settings->GetFrameRateLimit());
	FrameRates.Sort();
	FrameRates.Add(0.0f);
	if (MaxFPSComboBox)
	{
		MaxFPSComboBox->ClearOptions();
		for (const float Rate : FrameRates) MaxFPSComboBox->AddOption(Rate == 0.0f ? TEXT("제한 없음") : FString::Printf(TEXT("%g FPS"), Rate));
	}
	FillQualityOptions(QualityComboBox, true);
	for (UComboBoxString* Combo : { ShadowQualityComboBox.Get(), TextureQualityComboBox.Get(), EffectsQualityComboBox.Get(), AntiAliasingQualityComboBox.Get() }) FillQualityOptions(Combo, false);
	if (SmartCastComboBox)
	{
		SmartCastComboBox->ClearOptions();
		for (const TCHAR* Label : { TEXT("즉시 시전"), TEXT("범위 표시 후 키를 놓으면 시전"), TEXT("일반 시전") }) SmartCastComboBox->AddOption(Label);
	}
}

void UASSettingsWidget::RefreshSettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;
	bUpdatingUI = true;
	PopulateOptions();
	PendingOverallQuality = INDEX_NONE;
	if (WindowModeComboBox) WindowModeComboBox->SetSelectedIndex(static_cast<int32>(Settings->GetFullscreenMode()));
	if (ResolutionComboBox) ResolutionComboBox->SetSelectedIndex(Resolutions.IndexOfByKey(Settings->GetScreenResolution()));
	if (MaxFPSComboBox) MaxFPSComboBox->SetSelectedIndex(FrameRates.IndexOfByKey(Settings->GetFrameRateLimit()));
	const int32 Overall = Settings->GetOverallScalabilityLevel();
	if (QualityComboBox) QualityComboBox->SetSelectedIndex(Overall >= 0 && Overall <= 4 ? Overall : 5);
	if (ShadowQualityComboBox) ShadowQualityComboBox->SetSelectedIndex(Settings->GetShadowQuality());
	if (TextureQualityComboBox) TextureQualityComboBox->SetSelectedIndex(Settings->GetTextureQuality());
	if (EffectsQualityComboBox) EffectsQualityComboBox->SetSelectedIndex(Settings->GetVisualEffectQuality());
	if (AntiAliasingQualityComboBox) AntiAliasingQualityComboBox->SetSelectedIndex(Settings->GetAntiAliasingQuality());
	if (VSyncCheckBox) VSyncCheckBox->SetIsChecked(Settings->IsVSyncEnabled());
	if (const UASSkillSettingsSubsystem* Skills = UASSkillSettingsSubsystem::Get(GetOwningPlayer()))
	{
		if (SmartCastComboBox) SmartCastComboBox->SetSelectedIndex(static_cast<int32>(Skills->GetCastMode()));
	}
	if (const ULocalPlayer* Player = GetOwningLocalPlayer())
	{
		if (const UASLocalSettingsSubsystem* Local = Player->GetSubsystem<UASLocalSettingsSubsystem>())
		{
			if (MasterVolumeSlider)
			{
				MasterVolumeSlider->SetMinValue(0.0f);
				MasterVolumeSlider->SetMaxValue(1.0f);
				MasterVolumeSlider->SetValue(Local->GetMasterVolume());
				HandleVolumeChanged(Local->GetMasterVolume());
			}
		}
	}
	bUpdatingUI = false;
}

void UASSettingsWidget::HandleOverallQualityChanged(FString Item, ESelectInfo::Type SelectionType)
{
	if (bUpdatingUI || !QualityComboBox) return;
	const int32 Quality = QualityComboBox->GetSelectedIndex();
	if (Quality < 0 || Quality > 4) return;
	PendingOverallQuality = Quality;
	bUpdatingUI = true;
	for (UComboBoxString* Combo : { ShadowQualityComboBox.Get(), TextureQualityComboBox.Get(), EffectsQualityComboBox.Get(), AntiAliasingQualityComboBox.Get() })
	{
		if (Combo) Combo->SetSelectedIndex(Quality);
	}
	bUpdatingUI = false;
}

void UASSettingsWidget::HandleIndividualQualityChanged(FString Item, ESelectInfo::Type SelectionType)
{
	if (!bUpdatingUI && QualityComboBox)
	{
		bUpdatingUI = true;
		QualityComboBox->SetSelectedIndex(5);
		bUpdatingUI = false;
	}
}

void UASSettingsWidget::HandleVolumeChanged(const float Value)
{
	if (MasterVolumePercentage)
	{
		MasterVolumePercentage->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(Value, 0.0f, 1.0f) * 100.0f))));
	}
}

void UASSettingsWidget::ApplyGameSettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;
	if (WindowModeComboBox && WindowModeComboBox->GetSelectedIndex() >= 0)
	{
		Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(WindowModeComboBox->GetSelectedIndex()));
	}
	if (ResolutionComboBox && Resolutions.IsValidIndex(ResolutionComboBox->GetSelectedIndex())) Settings->SetScreenResolution(Resolutions[ResolutionComboBox->GetSelectedIndex()]);
	if (MaxFPSComboBox && FrameRates.IsValidIndex(MaxFPSComboBox->GetSelectedIndex())) Settings->SetFrameRateLimit(FrameRates[MaxFPSComboBox->GetSelectedIndex()]);
	if (PendingOverallQuality >= 0) Settings->SetOverallScalabilityLevel(PendingOverallQuality);
	Settings->SetShadowQuality(ReadQuality(ShadowQualityComboBox, Settings->GetShadowQuality()));
	Settings->SetTextureQuality(ReadQuality(TextureQualityComboBox, Settings->GetTextureQuality()));
	Settings->SetVisualEffectQuality(ReadQuality(EffectsQualityComboBox, Settings->GetVisualEffectQuality()));
	Settings->SetAntiAliasingQuality(ReadQuality(AntiAliasingQualityComboBox, Settings->GetAntiAliasingQuality()));
	if (VSyncCheckBox) Settings->SetVSyncEnabled(VSyncCheckBox->IsChecked());
	Settings->ApplySettings(false);
	Settings->ConfirmVideoMode();
	Settings->SaveSettings();
	if (UASSkillSettingsSubsystem* Skills = UASSkillSettingsSubsystem::Get(GetOwningPlayer()))
	{
		if (SmartCastComboBox && SmartCastComboBox->GetSelectedIndex() >= 0) Skills->SetCastMode(static_cast<EASSkillCastMode>(SmartCastComboBox->GetSelectedIndex()));
	}
	if (ULocalPlayer* Player = GetOwningLocalPlayer())
	{
		if (UASLocalSettingsSubsystem* Local = Player->GetSubsystem<UASLocalSettingsSubsystem>())
		{
			if (MasterVolumeSlider) Local->SetMasterVolume(MasterVolumeSlider->GetValue());
		}
	}
	RefreshSettings();
	if (SettingsStatusText) SettingsStatusText->SetText(FText::FromString(TEXT("설정을 적용하고 저장했습니다.")));
}

void UASSettingsWidget::OpenKeyBindings()
{
	if (!KeyMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Settings] Set KeyMappingContext on the settings widget."));
		if (SettingsStatusText) SettingsStatusText->SetText(FText::FromString(TEXT("키 바인딩에 사용할 입력 매핑을 지정해 주세요.")));
		return;
	}
	if (!KeyBindingsWidget) KeyBindingsWidget = CreateWidget<UASKeyBindingsWidget>(GetOwningPlayer());
	if (!KeyBindingsWidget) return;
	KeyBindingsWidget->SetMappingContext(KeyMappingContext);
	if (!KeyBindingsWidget->IsInViewport()) KeyBindingsWidget->AddToViewport(100);
}
