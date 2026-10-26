#include "Items/Loot/ASDroppedItem.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BoxComponent.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ASAbilityTags.h"
#include "InputCoreTypes.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Rendering/DrawElements.h"
#include "UObject/ConstructorHelpers.h"

TSharedRef<SWidget> UASDroppedItemNameWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("NameRoot"));
		NameBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NameBackground"));
		NameBackground->SetPadding(FMargin(12.0f, 5.0f));
		NameBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
		NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemName"));
		FSlateFontInfo Font = NameText->GetFont();
		Font.Size = 14;
		Font.TypefaceFontName = TEXT("Bold");
		Font.OutlineSettings.OutlineSize = 1;
		NameText->SetFont(Font);
		NameText->SetJustification(ETextJustify::Center);
		NameText->SetShadowColorAndOpacity(FLinearColor::Black);
		NameText->SetShadowOffset(FVector2D(1, 1));
		NameBackground->SetContent(NameText);
		UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(NameBackground);
		// Leave a small gap between the background and the hover brackets.
		BackgroundSlot->SetPadding(FMargin(4.0f));
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		WidgetTree->RootWidget = Root;
		SetCursor(EMouseCursor::Hand);
	}
	SetItem(ItemActor.Get());
	return Super::RebuildWidget();
}

void UASDroppedItemNameWidget::SetItem(AASDroppedItem* InItem)
{
	if (ItemActor.Get() != InItem)
	{
		if (UASItemInteractionComponent* Interaction = GetLocalInteraction())
			Interaction->SetHoveredDroppedItemLabel(ItemActor.Get(), false);
		bPointerHovered = false;
		bWorldHovered = false;
	}
	ItemActor = InItem;
	UASItemInstance* Item = IsValid(InItem) ? InItem->GetDroppedItem() : nullptr;
	SetVisibility(Item ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!NameText) return;
	FText Text = Item ? Item->GetItemDefinition()->DisplayName : FText::GetEmpty();
	if (Item && Item->GetQuantity() > 1)
		Text = FText::Format(NSLOCTEXT("Items", "DroppedStackName", "{0} x{1}"), Text, FText::AsNumber(Item->GetQuantity()));
	NameText->SetText(Text);
	NameText->SetColorAndOpacity(Item ? GetLootRarityColor(Item->GetRarity()) : FLinearColor::White);
	RefreshHoverAppearance();
}

UASItemInteractionComponent* UASDroppedItemNameWidget::GetLocalInteraction() const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC && GetWorld()) PC = GetWorld()->GetFirstPlayerController();
	return PC && PC->IsLocalController() ? PC->FindComponentByClass<UASItemInteractionComponent>() : nullptr;
}

void UASDroppedItemNameWidget::SetWorldHovered(const bool bHovered)
{
	if (bWorldHovered == bHovered) return;
	bWorldHovered = bHovered;
	RefreshHoverAppearance();
}

void UASDroppedItemNameWidget::RefreshHoverAppearance()
{
	if (NameBackground)
		NameBackground->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.022f, bPointerHovered || bWorldHovered ? 0.68f : 0.3f));
	InvalidateLayoutAndVolatility();
}

void UASDroppedItemNameWidget::NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Super::NativeOnMouseEnter(Geometry, Event);
	bPointerHovered = true;
	if (UASItemInteractionComponent* Interaction = GetLocalInteraction())
		Interaction->SetHoveredDroppedItemLabel(ItemActor.Get(), true);
	RefreshHoverAppearance();
}

void UASDroppedItemNameWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
	bPointerHovered = false;
	if (UASItemInteractionComponent* Interaction = GetLocalInteraction())
		Interaction->SetHoveredDroppedItemLabel(ItemActor.Get(), false);
	RefreshHoverAppearance();
	Super::NativeOnMouseLeave(Event);
}

void UASDroppedItemNameWidget::NativeDestruct()
{
	if (UASItemInteractionComponent* Interaction = GetLocalInteraction())
		Interaction->SetHoveredDroppedItemLabel(ItemActor.Get(), false);
	bPointerHovered = false;
	Super::NativeDestruct();
}

int32 UASDroppedItemNameWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, const int32 LayerId, const FWidgetStyle& Style, const bool bParentEnabled) const
{
	const int32 ContentLayer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled);
	if ((!bPointerHovered && !bWorldHovered) || !ItemActor.IsValid() || !ItemActor->GetDroppedItem()) return ContentLayer;
	const FVector2f Size(Geometry.GetLocalSize());
	const float Right = Size.X - 1.0f, Bottom = Size.Y - 1.0f, Length = 8.0f;
	const FLinearColor Color = FLinearColor(1,1,1,0.95f) * Style.GetColorAndOpacityTint();
	for (const TArray<FVector2f>& Corner : {
		TArray<FVector2f>{{1,Length},{1,1},{Length,1}},
		TArray<FVector2f>{{Right-Length,1},{Right,1},{Right,Length}},
		TArray<FVector2f>{{1,Bottom-Length},{1,Bottom},{Length,Bottom}},
		TArray<FVector2f>{{Right-Length,Bottom},{Right,Bottom},{Right,Bottom-Length}}})
		FSlateDrawElement::MakeLines(Elements, ContentLayer+1, Geometry.ToPaintGeometry(), Corner, ESlateDrawEffect::None, Color, false, 1.0f);
	return ContentLayer+1;
}

FReply UASDroppedItemNameWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton && ItemActor.IsValid())
	{
		if (UASItemInteractionComponent* Interaction = GetLocalInteraction())
		{
			Interaction->RequestPickupDroppedItem(ItemActor.Get());
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

AASDroppedItem::AASDroppedItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bReplicateUsingRegisteredSubObjectList = true;
	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	SetRootComponent(InteractionCollision);
	InteractionCollision->SetBoxExtent(FVector(30));
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionCollision->SetGenerateOverlapEvents(false);
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(InteractionCollision);
	// The fixed top-down camera looks along X; place the mesh's long X axis across the screen.
	ItemMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	ItemMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	ItemMesh->SetRelativeScale3D(FVector(0.2f));
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LegendaryBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LegendaryBeam"));
	LegendaryBeam->SetupAttachment(InteractionCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(
		TEXT("/Game/Resources/Items/Effects/SM_LegendaryItemBeam.SM_LegendaryItemBeam"));
	if (BeamMesh.Succeeded()) LegendaryBeam->SetStaticMesh(BeamMesh.Object);
	LegendaryBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LegendaryBeam->SetGenerateOverlapEvents(false);
	LegendaryBeam->SetCanEverAffectNavigation(false);
	LegendaryBeam->SetCastShadow(false);
	LegendaryBeam->SetVisibility(false);
	NameLabel = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(InteractionCollision);
	NameLabel->SetRelativeLocation(FVector(0, 0, 55));
	NameLabel->SetWidgetSpace(EWidgetSpace::Screen);
	NameLabel->SetWidgetClass(UASDroppedItemNameWidget::StaticClass());
	NameLabel->SetDrawSize(FVector2D(240, 28));
	NameLabel->SetDrawAtDesiredSize(true);
	NameLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DroppedInventory = CreateDefaultSubobject<UASInventoryComponent>(TEXT("DroppedInventory"));
}

void AASDroppedItem::BeginPlay()
{
	Super::BeginPlay();
	DroppedInventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	if (GetNetMode() != NM_DedicatedServer) NameLabel->InitWidget();
	// Server contents arrive after spawning; initial replicated contents may already exist on clients.
	if (!HasAuthority() || GetDroppedItem()) HandleInventoryChanged();
}

void AASDroppedItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MeshAssetRequest.Reset();
	DroppedInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	Super::EndPlay(EndPlayReason);
}

void AASDroppedItem::SetWorldHovered(const bool bHovered)
{
	if (bWorldHovered == bHovered) return;
	bWorldHovered = bHovered;
	if (UASDroppedItemNameWidget* Label = Cast<UASDroppedItemNameWidget>(NameLabel->GetUserWidgetObject()))
		Label->SetWorldHovered(bHovered);
}

UASItemInstance* AASDroppedItem::GetDroppedItem() const
{
	for (UASItemInstance* Item : DroppedInventory->GetItemInstances())
		if (IsValid(Item) && Item->IsValidItemInstance()) return Item;
	return nullptr;
}

bool AASDroppedItem::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) && Interactor->IsA<APawn>() && GetDroppedItem();
}

void AASDroppedItem::Interact_Implementation(AActor* Interactor)
{
	APawn* Pawn = Cast<APawn>(Interactor);
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	UASItemInteractionComponent* Interaction = Controller ? Controller->FindComponentByClass<UASItemInteractionComponent>() : nullptr;
	if (Interaction) Pickup(Pawn, Interaction->GetPlayerInventory());
}

int32 AASDroppedItem::Pickup(APawn* Interactor, UASInventoryComponent* TargetInventory)
{
	if (!HasAuthority() || !IsValid(Interactor) || !IsValid(TargetInventory)
		|| FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) > FMath::Square(GetInteractionRange_Implementation())) return 0;
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(TargetInventory, Interactor);
	UASItemInstance* Item = GetDroppedItem();
	if (!ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead) || !Item) return 0;
	return DroppedInventory->TransferItemTo(TargetInventory, Item, Item->GetQuantity());
}

void AASDroppedItem::HandleInventoryChanged()
{
	UASItemInstance* Item = GetDroppedItem();
	// Derived from the replicated inventory, so initial replication, stack updates and pickup all refresh it.
	LegendaryBeam->SetVisibility(Item && GetNetMode() != NM_DedicatedServer
		&& Item->GetItemDefinition()->Rarity == EASLootRarity::Legendary);
	if (!Item)
	{
		SetWorldHovered(false);
		MeshAssetRequest.Reset();
		ItemMesh->SetVisibility(false);
		if (UASDroppedItemNameWidget* Label = Cast<UASDroppedItemNameWidget>(NameLabel->GetUserWidgetObject())) Label->SetItem(nullptr);
		if (HasAuthority()) Destroy();
		return;
	}
	const UASItemDefinition* Definition = Item->GetItemDefinition();
	MeshAssetRequest.Request(Definition->WorldMesh.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this,
		[this] { if (!IsActorBeingDestroyed()) HandleInventoryChanged(); }));
	UStaticMesh* WorldMesh = Definition->WorldMesh.Get();
	ItemMesh->SetVisibility(WorldMesh || Definition->WorldMesh.IsNull());
	if (WorldMesh)
	{
		ItemMesh->SetStaticMesh(WorldMesh);
		ItemMesh->SetRelativeScale3D(FVector::OneVector);
		// Include the actual item's mesh bounds in the cursor hit area.
		const FBox RotatedBounds = WorldMesh->GetBoundingBox().TransformBy(ItemMesh->GetRelativeTransform());
		InteractionCollision->SetBoxExtent(RotatedBounds.GetExtent().ComponentMax(FVector(30)));
	}
	if (UASDroppedItemNameWidget* Label = Cast<UASDroppedItemNameWidget>(NameLabel->GetUserWidgetObject()))
	{
		Label->SetItem(this);
		Label->SetWorldHovered(bWorldHovered);
	}
}
