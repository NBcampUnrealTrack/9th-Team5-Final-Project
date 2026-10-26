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
#include "EngineUtils.h"
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

void UASDroppedItemNameComponent::SetRaised(const bool bRaised)
{
	const FName NextLayer = bRaised ? FName(TEXT("DroppedItemHoveredNameLayer")) : FName(TEXT("WidgetComponentScreenLayer"));
	if (SharedLayerName == NextLayer) return;
	if (UUserWidget* NameWidget = GetUserWidgetObject()) TransitionWidget = NameWidget->TakeWidget();
	// Remove from the previous layer before changing its name, so it cannot leave a duplicate behind.
	RemoveWidgetFromScreen();
	SetInitialSharedLayerName(NextLayer);
	SetInitialLayerZOrder(bRaised ? -99 : -100);
}

void UASDroppedItemNameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bAddedToScreen) TransitionWidget.Reset();
}

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
	if (AASDroppedItem* Item = ItemActor.Get())
		if (UASDroppedItemNameComponent* NameComponent = Item->FindComponentByClass<UASDroppedItemNameComponent>())
			NameComponent->SetRaised(bPointerHovered || bWorldHovered);
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
	SetReplicateMovement(true);
	bReplicateUsingRegisteredSubObjectList = true;
	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	SetRootComponent(InteractionCollision);
	InteractionCollision->SetBoxExtent(FVector(30));
	// Pickup is handled by the screen-space nameplate; world traces pass through this item.
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
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
	NameLabel = CreateDefaultSubobject<UASDroppedItemNameComponent>(TEXT("NameLabel"));
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

float AASDroppedItem::GetDropFootprintRadius() const
{
	float Radius = FMath::Max(30.0f, MinimumDropSpacing) * 0.5f;
	if (const UStaticMesh* Mesh = ItemMesh->GetStaticMesh())
	{
		const FBox Bounds = Mesh->GetBoundingBox().TransformBy(ItemMesh->GetRelativeTransform());
		// Include any pivot offset so two long weapons cannot cross even when their pivots are apart.
		const FVector Reach = Bounds.GetCenter().GetAbs() + Bounds.GetExtent();
		Radius = FMath::Max(Radius, FVector2D(Reach.X, Reach.Y).Size() + 10.0f);
	}
	return Radius;
}

void AASDroppedItem::SeparateFromNearbyDrops()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !GetDroppedItem()) return;
	const FVector Origin = GetActorLocation();
	const float Radius = GetDropFootprintRadius();
	struct FNearbyDrop
	{
		FVector Location;
		float RequiredDistance;
	};
	TArray<FNearbyDrop> Neighbours;
	for (TActorIterator<AASDroppedItem> It(World); It; ++It)
	{
		const AASDroppedItem* Other = *It;
		if (Other == this || Other->IsActorBeingDestroyed() || !Other->GetDroppedItem()) continue;
		Neighbours.Add({Other->GetActorLocation(), Radius + Other->GetDropFootprintRadius()});
	}
	auto IsFree = [&Neighbours](const FVector& Location)
	{
		for (const FNearbyDrop& Other : Neighbours)
		{
			// Items on separate floors do not occupy the same patch of ground.
			if (FMath::Abs(Location.Z - Other.Location.Z) < 150.0f
				&& FVector::DistSquared2D(Location, Other.Location) < FMath::Square(Other.RequiredDistance))
				return false;
		}
		return true;
	};
	if (IsFree(Origin)) return;

	// Test nearby rings first. Static-world traces ignore pawns, corpses and the other loot actors.
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DroppedItemPlacement), false, this);
	const FCollisionObjectQueryParams GroundObjects(ECC_WorldStatic);
	const float Step = FMath::Max(30.0f, MinimumDropSpacing * 0.5f);
	const int32 RingCount = FMath::CeilToInt(FMath::Max(Step, DropSearchRadius) / Step);
	for (int32 Ring = 1; Ring <= RingCount; ++Ring)
	{
		const float Distance = FMath::Min(Ring * Step, FMath::Max(Step, DropSearchRadius));
		const int32 PointCount = FMath::Max(8, FMath::CeilToInt(2.0f * PI * Distance / Step));
		for (int32 Point = 0; Point < PointCount; ++Point)
		{
			const float Angle = 2.0f * PI * Point / PointCount;
			FVector Candidate = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
			if (!IsFree(Candidate)) continue;
			FHitResult Ground;
			if (!World->LineTraceSingleByObjectType(Ground, Candidate + FVector(0, 0, 100),
				Candidate - FVector(0, 0, 300), GroundObjects, Query)
				|| Ground.ImpactNormal.Z < 0.65f) continue;
			Candidate = Ground.ImpactPoint + FVector(0, 0, 30);
			if (FMath::Abs(Candidate.Z - Origin.Z) > 100.0f || !IsFree(Candidate)) continue;
			FHitResult Obstacle;
			if (World->LineTraceSingleByObjectType(Obstacle, Origin + FVector(0, 0, 20),
				Candidate + FVector(0, 0, 20), GroundObjects, Query)) continue;
			SetActorLocation(Candidate);
			ForceNetUpdate();
			return;
		}
	}
	// Keep the item rather than lose its contents if the surrounding map has no usable ground.
	UE_LOG(LogTemp, Warning, TEXT("[DroppedItem] No free ground within %.0f cm of %s."), DropSearchRadius, *Origin.ToString());
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
		ItemMesh->SetRelativeScale3D(FVector(2.0f));
	}
	SeparateFromNearbyDrops();
	if (UASDroppedItemNameWidget* Label = Cast<UASDroppedItemNameWidget>(NameLabel->GetUserWidgetObject()))
	{
		Label->SetItem(this);
		Label->SetWorldHovered(bWorldHovered);
	}
}
