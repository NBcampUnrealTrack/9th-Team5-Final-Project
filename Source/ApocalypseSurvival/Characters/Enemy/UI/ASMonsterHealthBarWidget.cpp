#include "Characters/Enemy/UI/ASMonsterHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"

TSharedRef<SWidget> UASMonsterHealthBarWidget::RebuildWidget()
{
    BuildDefaultWidgetTree();
    return Super::RebuildWidget();
}

void UASMonsterHealthBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Screen Space WidgetComponent가 다시 화면 레이어에 추가된 경우,
    // NativeDestruct에서 해제했던 Attribute 구독과 현재 체력 표시를 복구한다.
    if (AASMonsterCharacter* Monster = HealthSource.Get())
    {
       SetHealthSource(Monster);
    }
}

void UASMonsterHealthBarWidget::NativeDestruct()
{
    UnbindHealthSource();
    Super::NativeDestruct();
}

void UASMonsterHealthBarWidget::SetHealthSource(AASMonsterCharacter* InMonster)
{
    HealthSource = InMonster;

    UASAbilitySystemComponent* NewAbilitySystemComponent = IsValid(InMonster)
       ? InMonster->GetASAbilitySystemComponent()
       : nullptr;
    if (AbilitySystemComponent == NewAbilitySystemComponent
       && HealthChangedHandle.IsValid()
       && MaxHealthChangedHandle.IsValid())
    {
       RefreshFromAttributes();
       return;
    }

    UnbindHealthSource();
    AbilitySystemComponent = NewAbilitySystemComponent;
    if (!IsValid(AbilitySystemComponent))
    {
       return;
    }

    HealthChangedHandle = AbilitySystemComponent
       ->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
       .AddUObject(this, &UASMonsterHealthBarWidget::HandleHealthChanged);
    MaxHealthChangedHandle = AbilitySystemComponent
       ->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetMaxHealthAttribute())
       .AddUObject(this, &UASMonsterHealthBarWidget::HandleMaxHealthChanged);

    RefreshFromAttributes();
}

void UASMonsterHealthBarWidget::SetHealthValues(float InHealth, float InMaxHealth)
{
    const float SafeMaxHealth = FMath::Max(0.0f, InMaxHealth);
    const float SafeHealth = FMath::Clamp(InHealth, 0.0f, SafeMaxHealth);
    const float HealthPercent = SafeMaxHealth > UE_SMALL_NUMBER ? SafeHealth / SafeMaxHealth : 0.0f;

    if (IsValid(HealthProgressBar))
    {
       HealthProgressBar->SetPercent(HealthPercent);
    }
    if (IsValid(HealthText))
    {
       HealthText->SetText(FText::AsNumber(FMath::CeilToInt(SafeHealth)));
    }

    // 복제된 Health가 0이 되는 즉시 각 클라이언트에서 사망 체력바를 감춘다.
    SetVisibility(SafeHealth > 0.0f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UASMonsterHealthBarWidget::BuildDefaultWidgetTree()
{
    if (!IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
    {
       return;
    }

    USizeBox* RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSizeBox"));
    RootSizeBox->SetWidthOverride(250.0f);
    RootSizeBox->SetHeightOverride(30.0f);

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
    Background->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.01f, 0.9f));
    Background->SetPadding(FMargin(3.0f));
    RootSizeBox->AddChild(Background);

    UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay"));
    Background->AddChild(Overlay);

    HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
    HealthProgressBar->SetFillColorAndOpacity(FLinearColor(0.85f, 0.02f, 0.04f, 1.0f));
    UOverlaySlot* ProgressSlot = Overlay->AddChildToOverlay(HealthProgressBar);
    ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
    ProgressSlot->SetVerticalAlignment(VAlign_Fill);

    HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
    HealthText->SetJustification(ETextJustify::Center);
    HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    HealthText->SetShadowColorAndOpacity(FLinearColor::Black);
    HealthText->SetShadowOffset(FVector2D(1.0f, 1.0f));
    FSlateFontInfo HealthFont = HealthText->GetFont();
    HealthFont.Size = 14;
    HealthText->SetFont(HealthFont);
    UOverlaySlot* TextSlot = Overlay->AddChildToOverlay(HealthText);
    TextSlot->SetHorizontalAlignment(HAlign_Fill);
    TextSlot->SetVerticalAlignment(VAlign_Center);

    WidgetTree->RootWidget = RootSizeBox;
}

void UASMonsterHealthBarWidget::UnbindHealthSource()
{
    if (IsValid(AbilitySystemComponent))
    {
       if (HealthChangedHandle.IsValid())
       {
          AbilitySystemComponent
             ->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
             .Remove(HealthChangedHandle);
       }
       if (MaxHealthChangedHandle.IsValid())
       {
          AbilitySystemComponent
             ->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetMaxHealthAttribute())
             .Remove(MaxHealthChangedHandle);
       }
    }

    HealthChangedHandle.Reset();
    MaxHealthChangedHandle.Reset();
    AbilitySystemComponent = nullptr;
}

void UASMonsterHealthBarWidget::RefreshFromAttributes()
{
    if (const UASHealthAttributeSet* HealthAttributes = IsValid(AbilitySystemComponent)
       ? AbilitySystemComponent->GetSet<UASHealthAttributeSet>()
       : nullptr)
    {
       SetHealthValues(HealthAttributes->GetHealth(), HealthAttributes->GetMaxHealth());
    }
}

void UASMonsterHealthBarWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    const UASHealthAttributeSet* HealthAttributes = AbilitySystemComponent
       ? AbilitySystemComponent->GetSet<UASHealthAttributeSet>()
       : nullptr;
    SetHealthValues(Data.NewValue, HealthAttributes ? HealthAttributes->GetMaxHealth() : 0.0f);
}

void UASMonsterHealthBarWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
    const UASHealthAttributeSet* HealthAttributes = AbilitySystemComponent
       ? AbilitySystemComponent->GetSet<UASHealthAttributeSet>()
       : nullptr;
    SetHealthValues(HealthAttributes ? HealthAttributes->GetHealth() : 0.0f, Data.NewValue);
}
