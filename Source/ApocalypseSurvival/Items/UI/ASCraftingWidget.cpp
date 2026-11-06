#include "Items/UI/ASCraftingWidget.h"
#include "Items/UI/ASCraftingEntryWidget.h"
#include "Items/UI/ViewModels/ASCraftingViewModel.h"
#include "Items/UI/ViewModels/ASCraftingEntryViewModel.h"
#include "Items/UI/ASItemViewBinding.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Items/UI/ASMenuStyle.h"
#include "Items/UI/ViewModels/ASPlayerMenuViewModel.h"
#include "Core/Framework/ASPlayerController.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UASCraftingWidget::RebuildWidget()
{
	UBorder* Frame = WidgetTree ? Cast<UBorder>(WidgetTree->FindWidget(TEXT("WindowFrame"))) : nullptr;
	if (!IsDesignTime() && Frame && !MenuTabs && Frame->GetContent())
	{
		ASMenuStyle::StyleAuthoredTree(WidgetTree);
		ASMenuStyle::StyleWindow(Frame);
		TMap<FName, UTextBlock*> BoundText;
		for (FName Name : {FName(TEXT("SelectedNameText")), FName(TEXT("SelectedCategoryText")), FName(TEXT("WeaponStatsText")), FName(TEXT("Details")), FName(TEXT("OwnedResultText")), FName(TEXT("AvailabilityText")), FName(TEXT("Status"))})
			BoundText.Add(Name, Cast<UTextBlock>(WidgetTree->FindWidget(Name)));
		Frame->SetContent(nullptr);
		UVerticalBox* Shell = WidgetTree->ConstructWidget<UVerticalBox>();
		MenuTabs = CreateWidget<UASPlayerMenuTabsWidget>(this);
		MenuTabs->ConfigureScreen(EASPlayerMenuTab::Crafting);
		Shell->AddChildToVerticalBox(MenuTabs);
		UBorder* BodyFrame = WidgetTree->ConstructWidget<UBorder>(); BodyFrame->SetBrushColor(FLinearColor::Transparent); BodyFrame->SetPadding(FMargin(28.f, 24.f));
		Shell->AddChildToVerticalBox(BodyFrame)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(); BodyFrame->SetContent(Body);
		auto Heading = [&](const TCHAR* Label, int32 Size, FLinearColor Color)
		{
			UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(); Block->SetText(FText::FromString(Label)); ASMenuStyle::StyleText(Block, Size, Color, true); return Block;
		};
		auto AddBound = [&](UVerticalBox* Parent, FName Name, int32 Size, FLinearColor Color, FMargin Spacing)
		{
			if (UTextBlock* Block = BoundText.FindRef(Name))
			{
				Block->RemoveFromParent(); Block->SetAutoWrapText(true); ASMenuStyle::StyleText(Block, Size, Color, Size >= 20);
				Parent->AddChildToVerticalBox(Block)->SetPadding(Spacing);
			}
		};
		Body->AddChildToVerticalBox(Heading(TEXT("아이템 제작"), 24, ASMenuStyle::Text))->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
		Body->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		USizeBox* RecipesWidth = WidgetTree->ConstructWidget<USizeBox>(); RecipesWidth->SetWidthOverride(228.f);
		Columns->AddChildToHorizontalBox(RecipesWidth)->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		UVerticalBox* Recipes = WidgetTree->ConstructWidget<UVerticalBox>(); RecipesWidth->SetContent(Recipes);
		Recipes->AddChildToVerticalBox(Heading(TEXT("제작 목록"), 12, ASMenuStyle::Accent))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		UScrollBox* RecipeScroll = WidgetTree->ConstructWidget<UScrollBox>(); Recipes->AddChildToVerticalBox(RecipeScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		if (RecipeList) { RecipeList->RemoveFromParent(); RecipeScroll->AddChild(RecipeList); }
		if (EmptyRecipesText) { EmptyRecipesText->RemoveFromParent(); Recipes->AddChildToVerticalBox(EmptyRecipesText); }
		UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
		Columns->AddChildToHorizontalBox(DetailScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UVerticalBox* Detail = WidgetTree->ConstructWidget<UVerticalBox>(); DetailScroll->AddChild(Detail);
		AddBound(Detail, TEXT("SelectedNameText"), 22, ASMenuStyle::Text, FMargin(0.f, 0.f, 0.f, 6.f));
		AddBound(Detail, TEXT("SelectedCategoryText"), 12, ASMenuStyle::Muted, FMargin(0.f, 0.f, 0.f, 18.f));
		USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>(); PreviewSize->SetHeightOverride(200.f);
		UBorder* Preview = WidgetTree->ConstructWidget<UBorder>(); Preview->SetBrushColor(ASMenuStyle::Surface); Preview->SetPadding(FMargin(18.f)); PreviewSize->SetContent(Preview);
		UOverlay* ImageLayers = WidgetTree->ConstructWidget<UOverlay>(); Preview->SetContent(ImageLayers);
		if (ResultIcon)
		{
			ResultIcon->RemoveFromParent();
			UScaleBox* IconScale = WidgetTree->ConstructWidget<UScaleBox>();
			IconScale->SetStretch(EStretch::ScaleToFit);
			IconScale->SetStretchDirection(EStretchDirection::Both);
			IconScale->SetContent(ResultIcon);
			UOverlaySlot* IconSlot = ImageLayers->AddChildToOverlay(IconScale);
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (ResultFallback) { ResultFallback->RemoveFromParent(); ResultFallback->SetText(NSLOCTEXT("CraftingUI", "NoPreview", "미리보기 없음")); ASMenuStyle::StyleText(ResultFallback, 12, ASMenuStyle::Muted); ImageLayers->AddChild(ResultFallback); }
		Detail->AddChildToVerticalBox(PreviewSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));
		AddBound(Detail, TEXT("WeaponStatsText"), 14, ASMenuStyle::Text, FMargin(0.f, 0.f, 0.f, 18.f));
		AddBound(Detail, TEXT("Details"), 13, ASMenuStyle::Muted, FMargin(0.f, 0.f, 0.f, 18.f));
		AddBound(Detail, TEXT("OwnedResultText"), 12, ASMenuStyle::Muted, FMargin(0.f));
		USizeBox* MaterialsWidth = WidgetTree->ConstructWidget<USizeBox>(); MaterialsWidth->SetWidthOverride(240.f);
		Columns->AddChildToHorizontalBox(MaterialsWidth)->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
		UBorder* MaterialsFrame = WidgetTree->ConstructWidget<UBorder>(); MaterialsFrame->SetBrushColor(ASMenuStyle::Surface); MaterialsFrame->SetPadding(FMargin(16.f)); MaterialsWidth->SetContent(MaterialsFrame);
		UVerticalBox* Materials = WidgetTree->ConstructWidget<UVerticalBox>(); MaterialsFrame->SetContent(Materials);
		Materials->AddChildToVerticalBox(Heading(TEXT("필요 재료"), 12, ASMenuStyle::Accent))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		UScrollBox* MaterialScroll = WidgetTree->ConstructWidget<UScrollBox>(); Materials->AddChildToVerticalBox(MaterialScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		if (MaterialList) { MaterialList->RemoveFromParent(); MaterialScroll->AddChild(MaterialList); }
		AddBound(Materials, TEXT("AvailabilityText"), 12, ASMenuStyle::Muted, FMargin(0.f, 16.f, 0.f, 0.f));
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
		if (UTextBlock* Status = BoundText.FindRef(TEXT("Status"))) { Status->RemoveFromParent(); ASMenuStyle::StyleText(Status, 12, ASMenuStyle::Muted); Footer->AddChildToHorizontalBox(Status)->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
		if (CraftButton) { CraftButton->RemoveFromParent(); USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(); ButtonSize->SetWidthOverride(200.f); ButtonSize->SetContent(CraftButton); Footer->AddChildToHorizontalBox(ButtonSize); }
		Frame->SetContent(Shell);
		if (CloseButton) CloseButton->SetVisibility(ESlateVisibility::Collapsed);
		ASMenuStyle::StyleButton(CraftButton, true);
	}
	return Super::RebuildWidget();
}

void UASCraftingWidget::SelectMenuTab(EASPlayerMenuTab Tab)
{
	if (MenuTabs && MenuTabs->GetViewModel()) MenuTabs->GetViewModel()->RequestTab(Tab);
}

FReply UASCraftingWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const AASPlayerController* PC = Cast<AASPlayerController>(GetOwningPlayer());
	if (Event.GetKey() == EKeys::Escape || (PC && PC->IsInventoryToggleKey(Event.GetKey())))
	{
		if (!Event.IsRepeat()) Close();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

UASCraftingWidget::UASCraftingWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultRecipeEntryClass = FSoftObjectPath(TEXT("/Game/ApocalypseSurvival/Items/UI/WBP_CraftingRecipeEntry.WBP_CraftingRecipeEntry_C"));
	DefaultMaterialEntryClass = FSoftObjectPath(TEXT("/Game/ApocalypseSurvival/Items/UI/WBP_CraftingMaterialEntry.WBP_CraftingMaterialEntry_C"));
}

void UASCraftingWidget::Populate(UPanelWidget* Panel, const TArray<TObjectPtr<UASCraftingEntryViewModel>>& Entries, TSubclassOf<UASCraftingEntryWidget> Class)
{
	if (!Panel || !Class) return;
	Panel->ClearChildren();
	for (UASCraftingEntryViewModel* Entry : Entries)
	{
		UASCraftingEntryWidget* Widget = nullptr;
#if WITH_EDITOR
		if (IsDesignTime())
		{
			Widget = NewObject<UASCraftingEntryWidget>(WidgetTree, Class);
			Widget->SetDesignerFlags(GetDesignerFlags()); Widget->Initialize();
		}
		else
#endif
		Widget = CreateWidget<UASCraftingEntryWidget>(GetOwningPlayer(), Class);
		if (Widget) { Widget->SetViewModel(Entry); Panel->AddChild(Widget); }
	}
}

void UASCraftingWidget::NativePreConstruct()
{
	if (IsDesignTime())
	{
		TArray<TObjectPtr<UASCraftingEntryViewModel>> Recipes, Materials;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			auto* Entry = NewObject<UASCraftingEntryViewModel>(this);
			Entry->Initialize(nullptr, nullptr, NAME_None, NSLOCTEXT("ASCrafting", "PreviewWeapon", "무기 이름"), Index == 0 ? NSLOCTEXT("ASCrafting", "Melee", "근접 무기") : FText::GetEmpty());
			Entry->Update(NSLOCTEXT("ASCrafting", "Available", "제작 가능"), true, true, Index == 0); Recipes.Add(Entry);
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto* Entry = NewObject<UASCraftingEntryViewModel>(this);
			Entry->Initialize(nullptr, nullptr, NAME_None, NSLOCTEXT("ASCrafting", "PreviewMaterial", "재료 이름"));
			Entry->Update(FText::FromString(TEXT("2 / 5")), false, true); Materials.Add(Entry);
		}
		Populate(RecipeList, Recipes, RecipeEntryClass ? RecipeEntryClass.Get() : DefaultRecipeEntryClass.LoadSynchronous());
		Populate(MaterialList, Materials, MaterialEntryClass ? MaterialEntryClass.Get() : DefaultMaterialEntryClass.LoadSynchronous());
		if (ResultIcon) ResultIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (ResultFallback) ResultFallback->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Super::NativePreConstruct();
}

void UASCraftingWidget::NativeConstruct()
{
	SetIsFocusable(true);
	if (!ViewModel) ViewModel = NewObject<UASCraftingViewModel>(this);
	ViewModel->Initialize(GetOwningPlayer());
	Super::NativeConstruct();
	ASItemViewBinding::Set(this, TEXT("CraftingVM"), ViewModel);
	if (CraftButton) CraftButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Craft);
	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Close);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ViewModel->RemoveAllFieldValueChangedDelegates(this);
	using FFields = UASCraftingViewModel::FFieldNotificationClassDescriptor;
	for (UE::FieldNotification::FFieldId Field : {FFields::RecipeEntries, FFields::MaterialEntries, FFields::ResultIcon, FFields::SelectedRecipe})
		ViewModel->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateWeakLambda(this,
			[this](UObject*, UE::FieldNotification::FFieldId) { Refresh(); }));
	RenderedRecipes.Reset(); RenderedMaterials.Reset();
	Refresh();
}

void UASCraftingWidget::NativeDestruct()
{
	if (CraftButton) CraftButton->OnClicked.RemoveDynamic(this, &ThisClass::Craft);
	if (CloseButton) CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::Close);
	if (ViewModel) { ViewModel->RemoveAllFieldValueChangedDelegates(this); ViewModel->Shutdown(); }
	RenderedRecipes.Reset(); RenderedMaterials.Reset();
	Super::NativeDestruct();
}

void UASCraftingWidget::Refresh()
{
	if (!ViewModel) return;
	if (RenderedRecipes != ViewModel->RecipeEntries)
	{
		Populate(RecipeList, ViewModel->RecipeEntries, RecipeEntryClass ? RecipeEntryClass.Get() : DefaultRecipeEntryClass.LoadSynchronous());
		RenderedRecipes = ViewModel->RecipeEntries;
	}
	if (RenderedMaterials != ViewModel->MaterialEntries)
	{
		Populate(MaterialList, ViewModel->MaterialEntries, MaterialEntryClass ? MaterialEntryClass.Get() : DefaultMaterialEntryClass.LoadSynchronous());
		RenderedMaterials = ViewModel->MaterialEntries;
	}
	if (EmptyRecipesText) EmptyRecipesText->SetVisibility(ViewModel->RecipeEntries.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (ResultIcon) { ResultIcon->SetBrushFromTexture(ViewModel->ResultIcon); ResultIcon->SetVisibility(ViewModel->ResultIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (ResultFallback) ResultFallback->SetVisibility(ViewModel->ResultIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UASCraftingWidget::Craft() { if (ViewModel) ViewModel->Craft(); }
void UASCraftingWidget::Close() { if (ViewModel) ViewModel->Close(); }
