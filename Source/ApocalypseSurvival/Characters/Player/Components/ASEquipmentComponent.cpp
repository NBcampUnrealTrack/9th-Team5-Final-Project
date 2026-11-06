#include "Characters/Player/Components/ASEquipmentComponent.h"

#include "Core/Framework/ASPlayerState.h"
#include "GameFramework/Character.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Net/UnrealNetwork.h"

UASEquipmentComponent::UASEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 장착 액터 포인터를 클라이언트도 받게 한다.
	SetIsReplicatedByDefault(true);
}

void UASEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UASEquipmentComponent, EquippedItem);
	DOREPLIFETIME(UASEquipmentComponent, EquippedActor);
}

void UASEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner()->HasAuthority())
	{
		ClearEquipment();
	}

	Super::EndPlay(EndPlayReason);
}

bool UASEquipmentComponent::EquipItem(UASItemInstance* Item)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Item) || !Item->IsValidItemInstance())
	{
		return false;
	}

	const UASItemDefinition* Definition = Item->GetItemDefinition();
	if (!IsValid(Definition) || Definition->ItemType != EASItemType::Weapon)
	{
		return false;
	}

	if (EquippedItem == Item)
	{
		return true;
	}

	bool bSpawnFailed = false;
	AActor* NewActor = SpawnEquippedActor(*Definition, bSpawnFailed);
	if (bSpawnFailed)
	{
		return false;
	}

	ClearEquipment();

	EquippedItem = Item;
	EquippedActor = NewActor;
	SaveEquippedItemRecord(Item);
	return true;
}

void UASEquipmentComponent::UnequipItem()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	ClearEquipment();

	// 사용자가 직접 뗀 것이므로 리스폰 뒤에도 빈손이어야 한다.
	SaveEquippedItemRecord(nullptr);
}

void UASEquipmentComponent::ClearEquipment()
{
	if (IsValid(EquippedActor))
	{
		EquippedActor->Destroy();
	}

	EquippedActor = nullptr;
	EquippedItem = nullptr;
}

bool UASEquipmentComponent::EquipInventoryItem_Implementation(UASInventoryComponent* Inventory, UASItemInstance* Item, APawn* Avatar)
{
	return EquipItem(Item);
}

bool UASEquipmentComponent::UnequipInventoryItem_Implementation(UASInventoryComponent* Inventory, APawn* Avatar)
{
	UnequipItem();
	return true;
}

UASItemInstance* UASEquipmentComponent::GetEquippedInventoryItem_Implementation(const UASInventoryComponent* Inventory) const
{
	return EquippedItem;
}

AActor* UASEquipmentComponent::GetEquippedInventoryActor_Implementation(const UASInventoryComponent* Inventory) const
{
	return EquippedActor;
}

AActor* UASEquipmentComponent::SpawnEquippedActor(const UASItemDefinition& Definition, bool& bOutFailed) const
{
	bOutFailed = false;

	// 장착 액터가 없는 무기(맨손 공격 등)도 있다. 그건 실패가 아니다.
	if (Definition.EquippedActorClass.IsNull())
	{
		return nullptr;
	}

	UClass* ActorClass = Definition.EquippedActorClass.Get();
	AActor* Owner = GetOwner();
	if (!ActorClass)
	{
		bOutFailed = true;
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = GetWorld()->SpawnActor<AActor>(ActorClass, Owner->GetActorTransform(), Params);
	if (!NewActor)
	{
		bOutFailed = true;
		return nullptr;
	}

	// 서버에서 붙이면 부착 정보가 같이 복제되어 클라이언트에서도 손에 들려 보인다.
	NewActor->SetReplicates(true);

	USceneComponent* AttachComponent = Owner->GetRootComponent();
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		AttachComponent = Character->GetMesh();
	}

	if (!AttachComponent || !NewActor->AttachToComponent(AttachComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Definition.EquippedSocketName))
	{
		NewActor->Destroy();
		bOutFailed = true;
		return nullptr;
	}

	return NewActor;
}

void UASEquipmentComponent::SaveEquippedItemRecord(UASItemInstance* Item) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (AASPlayerState* PlayerState = Pawn ? Pawn->GetPlayerState<AASPlayerState>() : nullptr)
	{
		PlayerState->SetEquippedItemRecord(Item);
	}
}
