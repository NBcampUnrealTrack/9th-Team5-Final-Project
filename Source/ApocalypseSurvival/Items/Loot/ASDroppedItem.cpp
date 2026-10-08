#include "Items/Loot/ASDroppedItem.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BoxComponent.h"
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

TSharedRef<SWidget> UASDroppedItemNameWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		NameText = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = NameText->GetFont();
		Font.Size = 14;
		Font.OutlineSettings.OutlineSize = 1;
		NameText->SetFont(Font);
		NameText->SetJustification(ETextJustify::Center);
		NameText->SetShadowColorAndOpacity(FLinearColor::Black);
		NameText->SetShadowOffset(FVector2D(1, 1));
		WidgetTree->RootWidget = NameText;
	}
	SetItem(ItemActor.Get());
	return Super::RebuildWidget();
}

void UASDroppedItemNameWidget::SetItem(AASDroppedItem* InItem)
{
	ItemActor = InItem;
	UASItemInstance* Item = IsValid(InItem) ? InItem->GetDroppedItem() : nullptr;
	if (!NameText) return;
	FText Text = Item ? Item->GetItemDefinition()->DisplayName : FText::GetEmpty();
	if (Item && Item->GetQuantity() > 1)
		Text = FText::Format(NSLOCTEXT("Items", "DroppedStackName", "{0} x{1}"), Text, FText::AsNumber(Item->GetQuantity()));
	NameText->SetText(Text);
}

FReply UASDroppedItemNameWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton && ItemActor.IsValid())
	{
		APlayerController* PC = GetOwningPlayer();
		if (!PC && GetWorld()) PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->IsLocalController())
		{
			if (UASItemInteractionComponent* Interaction = PC->FindComponentByClass<UASItemInteractionComponent>())
			{
				Interaction->RequestPickupDroppedItem(ItemActor.Get());
				return FReply::Handled();
			}
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
	ItemMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	ItemMesh->SetRelativeScale3D(FVector(0.2f));
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameLabel = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(InteractionCollision);
	NameLabel->SetRelativeLocation(FVector(0, 0, 55));
	NameLabel->SetWidgetSpace(EWidgetSpace::Screen);
	NameLabel->SetWidgetClass(UASDroppedItemNameWidget::StaticClass());
	NameLabel->SetDrawSize(FVector2D(240, 28));
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
	DroppedInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	Super::EndPlay(EndPlayReason);
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
	if (!Item)
	{
		if (HasAuthority()) Destroy();
		return;
	}
	if (UStaticMesh* WorldMesh = Item->GetItemDefinition()->WorldMesh.LoadSynchronous())
	{
		ItemMesh->SetStaticMesh(WorldMesh);
		ItemMesh->SetRelativeScale3D(FVector::OneVector);
		// Include the actual item's mesh bounds in the cursor hit area.
		InteractionCollision->SetBoxExtent(WorldMesh->GetBounds().BoxExtent.ComponentMax(FVector(30)));
	}
	if (UASDroppedItemNameWidget* Label = Cast<UASDroppedItemNameWidget>(NameLabel->GetUserWidgetObject()))
		Label->SetItem(this);
}
