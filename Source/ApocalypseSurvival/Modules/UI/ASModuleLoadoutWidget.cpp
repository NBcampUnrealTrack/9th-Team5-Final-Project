#include "Modules/UI/ASModuleLoadoutWidget.h"
#include "Modules/UI/ViewModels/ASModuleLoadoutViewModel.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Items/UI/ASMenuStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UASModuleLoadoutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	WidgetTree->RootWidget = Scroll;
	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
}

void UASModuleLoadoutWidget::NativeConstruct()
{
	if (!ViewModel) ViewModel = NewObject<UASModuleLoadoutViewModel>(this);
	ViewModel->Initialize(GetOwningPlayer());
	Super::NativeConstruct();
	ASItemViewBinding::Set(this, TEXT("ModuleLoadoutVM"), ViewModel);
	ViewModel->RemoveAllFieldValueChangedDelegates(this);
	ViewModel->AddFieldValueChangedDelegate(UASModuleLoadoutViewModel::FFieldNotificationClassDescriptor::Presentation,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this, [this](UObject*, UE::FieldNotification::FFieldId) { Refresh(); }));
	Refresh();
}

void UASModuleLoadoutWidget::NativeDestruct()
{
	if (ViewModel) { ViewModel->RemoveAllFieldValueChangedDelegates(this); ViewModel->Shutdown(); }
	Super::NativeDestruct();
}

void UASModuleLoadoutWidget::Refresh()
{
	if (!Rows) return;
	Rows->ClearChildren();
	auto Text = [&](UVerticalBox* Parent, const FText& Value, int32 Size, FLinearColor Color, bool bBold = false)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
		Block->SetText(Value); Block->SetAutoWrapText(true);
		ASMenuStyle::StyleText(Block, Size, Color, bBold);
		Parent->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	};
	Text(Rows, NSLOCTEXT("Modules", "EquipmentTitle", "장착 모듈"), 24, ASMenuStyle::Text, true);
	Text(Rows, NSLOCTEXT("Modules", "LobbyEquipmentHint", "이번 출격에 적용된 모듈입니다. 교체는 로비에서 할 수 있습니다."), 13, ASMenuStyle::Muted);
	UHorizontalBox* Slots = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rows->AddChildToVerticalBox(Slots)->SetPadding(FMargin(0.f, 20.f, 0.f, 0.f));
	if (!ViewModel) return;
	for (const FASModuleSlotPresentation& Module : ViewModel->Presentation.Slots)
	{
		UBorder* Socket = WidgetTree->ConstructWidget<UBorder>();
		Socket->SetBrushColor(ASMenuStyle::Surface); Socket->SetPadding(FMargin(20.f, 24.f));
		UHorizontalBoxSlot* SocketSlot = Slots->AddChildToHorizontalBox(Socket);
		SocketSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); SocketSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(); Socket->SetContent(Body);
		Text(Body, Module.SlotLabel, 12, ASMenuStyle::Accent);
		Text(Body, Module.Name, 20, Module.bEquipped ? ASMenuStyle::Text : ASMenuStyle::Muted, true);
		if (!Module.Description.IsEmpty()) Text(Body, Module.Description, 14, ASMenuStyle::Muted);
		if (!Module.Effects.IsEmpty()) Text(Body, Module.Effects, 13, ASMenuStyle::Text);
	}
}
