#include "Characters/Player/Components/ASEquipmentComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Input/ASInputTags.h"
#include "GameFramework/Character.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/PBWeaponAttackAbility.h"
#include "Net/UnrealNetwork.h"

UASEquipmentComponent::UASEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 장착 액터 포인터를 클라이언트도 받게 한다.
	SetIsReplicatedByDefault(true);

	PrimaryAttackInputTag = ASGameplayTags::InputTag_Attack_Primary;
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

	// 상태를 건드리기 전에 에셋을 전부 읽는다. 중간에 실패하면 이전 장착을 유지해야 한다.
	UClass* AttackAbilityClass = nullptr;
	if (!Definition->PrimaryAttackAbility.IsNull())
	{
		AttackAbilityClass = Definition->PrimaryAttackAbility.LoadSynchronous();
		if (!AttackAbilityClass)
		{
			return false;
		}
	}
	else if (Definition->WeaponType == EASWeaponType::Melee
		|| Definition->WeaponType == EASWeaponType::Ranged)
	{
		// 무기 데이터에 공격 Ability가 없을 때 기본 일반공격을 자동으로 부여한다.
		AttackAbilityClass = UPBWeaponAttackAbility::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[Equipment] %s has no PrimaryAttackAbility. Using the default weapon attack."),
			*GetNameSafe(Definition));
	}

	// 어빌리티를 받을 ASC 가 아직 없으면 지금은 달 수 없다. 빙의 후 다시 시도하면 된다.
	if (AttackAbilityClass && !IsValid(GetOwnerAbilitySystemComponent()))
	{
		return false;
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
	GrantPrimaryAttack(AttackAbilityClass, Item);
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
	// 회수는 ASC 가 서버 권한일 때만 한다. 실행 중인 어빌리티는 끝난 뒤 빠진다.
	GrantedHandles.TakeFromAbilitySystem(GetOwnerAbilitySystemComponent());

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

	UClass* ActorClass = Definition.EquippedActorClass.LoadSynchronous();
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

void UASEquipmentComponent::GrantPrimaryAttack(UClass* AbilityClass, UASItemInstance* Item)
{
	UASAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!AbilityClass || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	// SourceObject 에 아이템을 넣어 두면 어빌리티가 자기 무기의 정의(피해, 간격, 탄약)를 읽을 수 있다.
	FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, Item);
	if (PrimaryAttackInputTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(PrimaryAttackInputTag);
	}

	GrantedHandles.AddAbilitySpecHandle(AbilitySystemComponent->GiveAbility(AbilitySpec));
}

UASAbilitySystemComponent* UASEquipmentComponent::GetOwnerAbilitySystemComponent() const
{
	return Cast<UASAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
}

void UASEquipmentComponent::SaveEquippedItemRecord(UASItemInstance* Item) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (AASPlayerState* PlayerState = Pawn ? Pawn->GetPlayerState<AASPlayerState>() : nullptr)
	{
		PlayerState->SetEquippedItemRecord(Item);
	}
}
