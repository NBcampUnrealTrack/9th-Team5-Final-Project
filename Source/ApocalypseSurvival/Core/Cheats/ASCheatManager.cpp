#include "Core/Cheats/ASCheatManager.h"

#include "Core/Framework/ASPlayerController.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"

#if !UE_BUILD_SHIPPING
namespace ASCheatManagerPrivate
{
	struct FWeaponEntry
	{
		const TCHAR* Name;
		const TCHAR* AssetPath;
	};

	// 서버가 허용한 무기만 지급한다. 클라이언트가 임의 자산 경로를 지정할 수 없다.
	const FWeaponEntry Weapons[] =
	{
		{TEXT("Dagger"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_Dagger.PDA_Item_Dagger")},
		{TEXT("Axe"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_Axe.PDA_Item_Axe")},
		{TEXT("LongSword"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_LongSword.PDA_Item_LongSword")},
		{TEXT("Scythe"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_Scythe.PDA_Item_Scythe")},
		{TEXT("Pistol"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_Pistol.PDA_Item_Pistol")},
		{TEXT("Shotgun"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_Shotgun.PDA_Item_Shotgun")},
		{TEXT("AssaultRifle"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_AssaultRifle.PDA_Item_AssaultRifle")},
		{TEXT("SniperRifle"), TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_SniperRifle.PDA_Item_SniperRifle")}
	};

	void Report(AASPlayerController* PlayerController, const FString& Message)
	{
		UE_LOG(LogTemp, Log, TEXT("%s: %s"), *GetNameSafe(PlayerController), *Message);
		PlayerController->ClientMessage(Message);
	}
}
#endif

void UASCheatManager::DebugGiveItem(FName ItemName, int32 Quantity)
{
#if !UE_BUILD_SHIPPING
	if (AASPlayerController* PlayerController = Cast<AASPlayerController>(GetOuter()))
	{
		PlayerController->ServerDebugGiveItem(ItemName, Quantity);
	}
#endif
}

void UASCheatManager::GiveItemToPlayer(AASPlayerController* PlayerController, FName ItemName, int32 Quantity)
{
#if !UE_BUILD_SHIPPING
	using namespace ASCheatManagerPrivate;
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority()) return;

	// Resolve only known definition names, never an arbitrary client-supplied asset path.
	static const FName ItemNames[] = {
		TEXT("Medkit"), TEXT("ManaPotion"), TEXT("Grenade"), TEXT("Ammo"), TEXT("Barricade"),
		TEXT("Cloth"), TEXT("Gunpowder"), TEXT("Herbs"), TEXT("Polymer"), TEXT("ScrapMetal"), TEXT("WeaponParts")
	};
	FName SelectedName = NAME_None;
	TArray<FString> SupportedNames;
	for (const FName Name : ItemNames)
	{
		SupportedNames.Add(Name.ToString());
		if (Name == ItemName) SelectedName = Name;
	}
	if (SelectedName.IsNone())
	{
		Report(PlayerController, FString::Printf(TEXT("[DebugGiveItem] Usage: DebugGiveItem <Name> <Quantity>. Items: %s"),
			*FString::Join(SupportedNames, TEXT(", "))));
		return;
	}
	if (Quantity <= 0)
	{
		Report(PlayerController, TEXT("[DebugGiveItem] Quantity must be greater than zero. Nothing was granted."));
		return;
	}
	UASInventoryComponent* Inventory = PlayerController->GetInventoryComponent();
	if (!IsValid(Inventory))
	{
		Report(PlayerController, TEXT("[DebugGiveItem] Inventory is not ready. Nothing was granted."));
		return;
	}
	const FString AssetPath = FString::Printf(TEXT("/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_%s.PDA_Item_%s"),
		*SelectedName.ToString(), *SelectedName.ToString());
	UASItemDefinition* Definition = LoadObject<UASItemDefinition>(nullptr, *AssetPath);
	if (!IsValid(Definition) || Definition->ItemType == EASItemType::Weapon)
	{
		Report(PlayerController, TEXT("[DebugGiveItem] Could not load the item. Nothing was granted."));
		return;
	}
	// AddItem also fills existing stacks when every inventory slot is occupied.
	const int32 Added = Inventory->AddItem(Definition, Quantity);
	Report(PlayerController, FString::Printf(TEXT("[DebugGiveItem] Granted %s x%d (requested %d)."),
		*SelectedName.ToString(), Added, Quantity));
#endif
}

void UASCheatManager::DebugGod()
{
#if !UE_BUILD_SHIPPING
	if (AASPlayerController* PlayerController = Cast<AASPlayerController>(GetOuter()))
	{
		PlayerController->ServerDebugGod();
	}
#endif
}

void UASCheatManager::DebugGiveWeapon(FName WeaponName)
{
#if !UE_BUILD_SHIPPING
	if (AASPlayerController* PlayerController = Cast<AASPlayerController>(GetOuter()))
	{
		PlayerController->ServerDebugGiveWeapon(WeaponName);
	}
#endif
}

void UASCheatManager::GiveWeaponToPlayer(AASPlayerController* PlayerController, FName WeaponName)
{
#if !UE_BUILD_SHIPPING
	using namespace ASCheatManagerPrivate;
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority())
	{
		return;
	}

	const FWeaponEntry* SelectedWeapon = nullptr;
	TArray<FString> WeaponNames;
	for (const FWeaponEntry& Weapon : Weapons)
	{
		WeaponNames.Add(Weapon.Name);
		if (WeaponName == FName(Weapon.Name))
		{
			SelectedWeapon = &Weapon;
		}
	}
	if (!SelectedWeapon)
	{
		Report(PlayerController, FString::Printf(TEXT("[DebugGiveWeapon] Usage: DebugGiveWeapon <Name>. Weapons: %s"),
			*FString::Join(WeaponNames, TEXT(", "))));
		return;
	}

	UASInventoryComponent* Inventory = PlayerController->GetInventoryComponent();
	if (!IsValid(Inventory))
	{
		Report(PlayerController, TEXT("[DebugGiveWeapon] Inventory is not ready. Nothing was granted."));
		return;
	}

	const int32 SlotLimit = Inventory->GetMaxSlots() > 0
		? Inventory->GetMaxSlots() : UASInventoryComponent::PlayerSlotCount;
	if (Inventory->GetUsedSlotCount() >= SlotLimit)
	{
		Report(PlayerController, TEXT("[DebugGiveWeapon] Inventory is full. Nothing was granted."));
		return;
	}

	UASItemDefinition* Definition = LoadObject<UASItemDefinition>(nullptr, SelectedWeapon->AssetPath);
	if (!IsValid(Definition) || Definition->ItemType != EASItemType::Weapon)
	{
		Report(PlayerController, FString::Printf(TEXT("[DebugGiveWeapon] Could not load weapon '%s'. Nothing was granted."),
			SelectedWeapon->Name));
		return;
	}

	if (Inventory->AddItem(Definition, 1) != 1)
	{
		Report(PlayerController, TEXT("[DebugGiveWeapon] Could not add the weapon. Nothing was granted."));
		return;
	}

	Report(PlayerController, FString::Printf(TEXT("[DebugGiveWeapon] Granted %s x1."), SelectedWeapon->Name));
#endif
}
