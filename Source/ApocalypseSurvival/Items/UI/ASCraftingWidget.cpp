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
