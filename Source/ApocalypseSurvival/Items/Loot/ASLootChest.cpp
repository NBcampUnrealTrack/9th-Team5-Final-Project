#include "Items/Loot/ASLootChest.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/ASItemInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

AASLootChest::AASLootChest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bReplicateUsingRegisteredSubObjectList = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
	ChestMesh->SetupAttachment(SceneRoot);

	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitBoxExtent(FVector(75.0f, 75.0f, 75.0f));
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionCollision->SetGenerateOverlapEvents(false);

	LootInventory = CreateDefaultSubobject<UASInventoryComponent>(TEXT("LootInventory"));
}

bool AASLootChest::SetInitialLoot(const TArray<FASItemGrant>& Items)
{
	if (!HasAuthority() || GetNetMode() == NM_Client || HasActorBegunPlay() || IsActorBeginningPlay()) return false;
	for (const FASItemGrant& Grant : Items)
	{
		if (!IsValid(Grant.ItemDefinition) || Grant.Quantity <= 0 || Grant.ItemDefinition->MaxStackSize <= 0)
			return false;
	}
	InitialLoot = Items;
	return true;
}

void AASLootChest::BeginPlay()
{
	// Initialize before Super dispatches Blueprint BeginPlay so presentation sees the contents.
	if (HasAuthority())
	{
		for (const FASItemGrant& Grant : InitialLoot)
		{
			if (!IsValid(Grant.ItemDefinition) || Grant.Quantity <= 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LootChest] %s has an invalid InitialLoot entry."), *GetName());
				continue;
			}
			const int32 Added = LootInventory->AddItem(Grant.ItemDefinition, Grant.Quantity);
			if (Added != Grant.Quantity)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LootChest] %s added %d/%d of %s; check inventory capacity."),
					*GetName(), Added, Grant.Quantity, *GetNameSafe(Grant.ItemDefinition));
			}
		}
	}
	Super::BeginPlay();
}

bool AASLootChest::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) && IsValid(LootInventory) && LootInventory->GetUsedSlotCount() > 0;
}

void AASLootChest::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !IASInteractable::Execute_CanInteract(this, Interactor) || !IsWithinInteractionRange(Interactor))
	{
		return;
	}

	OnLootInteractionRequested.Broadcast(this, Interactor);
	BP_OnLootInteractionRequested(Interactor);
}

FVector AASLootChest::GetInteractionLocation_Implementation() const
{
	return IsValid(InteractionCollision)
		? InteractionCollision->GetComponentLocation()
		: GetActorLocation();
}

float AASLootChest::GetInteractionRange_Implementation() const
{
	return InteractionRange;
}

bool AASLootChest::IsWithinInteractionRange(const AActor* Interactor) const
{
	return IsValid(Interactor)
		&& FVector::DistSquared(Interactor->GetActorLocation(), GetInteractionLocation_Implementation())
		<= FMath::Square(InteractionRange);
}

int32 AASLootChest::TakeLoot(APawn* Interactor, UASInventoryComponent* TargetInventory,
	const FGuid InstanceId, const int32 Quantity)
{
	if (!HasAuthority() || !IsValid(Interactor) || !IsValid(TargetInventory) || !InstanceId.IsValid() || Quantity <= 0
		|| (TargetInventory->GetOwner() != Interactor
			&& TargetInventory->GetOwner() != Interactor->GetPlayerState()
			&& TargetInventory->GetOwner() != Interactor->GetController())
		|| !IsWithinInteractionRange(Interactor) || !IASInteractable::Execute_CanInteract(this, Interactor)) return 0;
	UASItemInstance* Item = LootInventory->FindItemById(InstanceId);
	return Item ? LootInventory->TransferItemTo(TargetInventory, Item, Quantity) : 0;
}
