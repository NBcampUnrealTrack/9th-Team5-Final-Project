#include "Items/Loot/ASLootChest.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Loot/ASLootValidation.h"
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
	if (!HasAuthority() || GetNetMode() == NM_Client || HasActorBegunPlay() || IsActorBeginningPlay()
		|| bInitialLootAttempted) return false;
	FString Error;
	if (!FASLootValidation::ValidateGrants(Items, Error)) return false;
	InitialLoot = Items;
	SpawnInitialLoot = Items;
	bHasSpawnInitialLoot = true;
	return true;
}

bool AASLootChest::InitializeInitialLoot(FString& OutError)
{
	OutError.Reset();
	if (!HasAuthority() || GetNetMode() == NM_Client)
	{
		OutError = TEXT("Initial loot requires server authority.");
		return false;
	}
	if (bInitialLootAttempted)
	{
		OutError = InitialLootError;
		return bInitialLootSucceeded;
	}
	bInitialLootAttempted = true; // Inventory callbacks cannot re-enter initialization.
	auto Fail = [&](const FString& Error)
	{
		InitialLootError = Error;
		OutError = Error;
		return false;
	};
	if (!IsValid(LootInventory) || !LootInventory->GetItems().IsEmpty())
		return Fail(TEXT("Initial loot requires an empty, valid chest inventory."));
	const TArray<FASItemGrant> Grants = bHasSpawnInitialLoot ? SpawnInitialLoot : InitialLoot;
	FString Error;
	if (!FASLootValidation::ValidateGrants(Grants, Error)) return Fail(Error);
	TMap<const UASItemDefinition*, int64> Totals;
	for (const FASItemGrant& Grant : Grants) Totals.FindOrAdd(Grant.ItemDefinition) += Grant.Quantity;
	int64 RequiredSlots = 0;
	for (const auto& Total : Totals)
		RequiredSlots += (Total.Value + Total.Key->MaxStackSize - 1) / Total.Key->MaxStackSize;
	const int32 MaxSlots = LootInventory->GetMaxSlots();
	if (MaxSlots > 0 && RequiredSlots > MaxSlots)
		return Fail(FString::Printf(TEXT("Initial loot requires %lld slots, but the chest allows %d."), RequiredSlots, MaxSlots));

	auto Rollback = [&]()
	{
		const TArray<UASItemInstance*> AddedItems = LootInventory->GetItems();
		for (UASItemInstance* Item : AddedItems)
			if (IsValid(Item)) LootInventory->RemoveItemInstance(Item, Item->GetQuantity());
	};
	for (const FASItemGrant& Grant : Grants)
	{
		if (LootInventory->AddItem(Grant.ItemDefinition, Grant.Quantity) != Grant.Quantity || IsActorBeingDestroyed())
		{
			Rollback();
			return Fail(TEXT("Could not add all requested loot; initial additions were rolled back."));
		}
	}
	for (const auto& Total : Totals)
	{
		if (LootInventory->GetItemCount(Total.Key) != Total.Value)
		{
			Rollback();
			return Fail(TEXT("Initial loot quantity verification failed."));
		}
	}
	bInitialLootSucceeded = true;
	return true;
}

void AASLootChest::BeginPlay()
{
	// Complete initial contents before Blueprint BeginPlay; an incomplete chest is discarded.
	bool bInitializationFailed = false;
	if (HasAuthority())
	{
		FString Error;
		bInitializationFailed = !InitializeInitialLoot(Error);
		if (bInitializationFailed)
			UE_LOG(LogTemp, Warning, TEXT("[LootChest] %s: %s"), *GetName(), *Error);
	}
	Super::BeginPlay();
	if (bInitializationFailed) Destroy();
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
