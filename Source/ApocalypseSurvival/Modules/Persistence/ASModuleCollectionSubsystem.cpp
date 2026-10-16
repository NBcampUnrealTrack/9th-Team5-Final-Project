#include "Modules/Persistence/ASModuleCollectionSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/Data/ASModuleDefinition.h"
#include "Modules/Data/ASModuleSettings.h"
#include "Modules/Persistence/ASModuleSaveGame.h"

void UASModuleCollectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Error;
	if (!LoadProfile(Error)) UE_LOG(LogTemp, Warning, TEXT("[Modules] %s"), *Error);
}

UASModuleCollectionSubsystem* UASModuleCollectionSubsystem::Get(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UASModuleCollectionSubsystem>() : nullptr;
}

FString UASModuleCollectionSubsystem::GetSaveSlotName() const
{
	return FString::Printf(TEXT("%s_%d"), *GetDefault<UASModuleSettings>()->SaveSlotPrefix, GetLocalPlayer()->GetControllerId());
}

bool UASModuleCollectionSubsystem::LoadProfile(FString& OutError)
{
	OutError.Reset();
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	TArray<FName> LoadedOwned;
	TArray<FName> LoadedEquipped;
	LoadedEquipped.SetNum(Settings->GetSlotCount());
	if (UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(), 0))
	{
		const UASModuleSaveGame* Save = Cast<UASModuleSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(), 0));
		if (!Save || Save->DataVersion != 1)
		{
			OutError = TEXT("Module save could not be read or has an unsupported version. Existing data was preserved.");
			bProfileLoaded = false;
			return false;
		}
		// Keep unknown owned IDs: temporarily unregistered content must not erase earned unlocks.
		for (FName Id : Save->OwnedModuleIds)
		{
			if (!Id.IsNone()) LoadedOwned.AddUnique(Id);
		}
		TSet<FName> Seen;
		for (int32 Index = 0; Index < FMath::Min(Save->EquippedModuleIds.Num(), LoadedEquipped.Num()); ++Index)
		{
			const FName Id = Save->EquippedModuleIds[Index];
			FString Error;
			UASModuleDefinition* Definition = Settings->FindModule(Id, Error);
			if (LoadedOwned.Contains(Id) && !Seen.Contains(Id) && Definition && Definition->ValidateDefinition(Error))
			{
				LoadedEquipped[Index] = Id;
				Seen.Add(Id);
			}
		}
	}
	OwnedModuleIds = MoveTemp(LoadedOwned);
	EquippedModuleIds = MoveTemp(LoadedEquipped);
	bProfileLoaded = true;
	OnCollectionChanged.Broadcast();
	return true;
}

bool UASModuleCollectionSubsystem::CommitProfile(const TArray<FName>& NewOwned, const TArray<FName>& NewEquipped, FString& OutError)
{
	OutError.Reset();
	if (!bProfileLoaded)
	{
		OutError = TEXT("Load a valid module profile before modifying progression.");
		return false;
	}
	UASModuleSaveGame* Save = Cast<UASModuleSaveGame>(UGameplayStatics::CreateSaveGameObject(UASModuleSaveGame::StaticClass()));
	if (!Save)
	{
		OutError = TEXT("Could not create module save data.");
		return false;
	}
	Save->OwnedModuleIds = NewOwned;
	Save->EquippedModuleIds = NewEquipped;
	if (!UGameplayStatics::SaveGameToSlot(Save, GetSaveSlotName(), 0))
	{
		OutError = TEXT("Could not save module progression. No in-memory changes were committed.");
		return false;
	}
	OwnedModuleIds = NewOwned;
	EquippedModuleIds = NewEquipped;
	OnCollectionChanged.Broadcast();
	return true;
}

bool UASModuleCollectionSubsystem::GrantModule(FName ModuleId, FString& OutError)
{
	OutError.Reset();
	UASModuleDefinition* Definition = GetDefault<UASModuleSettings>()->FindModule(ModuleId, OutError);
	if (!Definition || !Definition->ValidateDefinition(OutError)) return false;
	if (!bProfileLoaded)
	{
		OutError = TEXT("The module profile is not loaded.");
		return false;
	}
	if (OwnsModule(ModuleId)) return true;
	TArray<FName> NewOwned = OwnedModuleIds;
	NewOwned.Add(ModuleId);
	return CommitProfile(NewOwned, EquippedModuleIds, OutError);
}

bool UASModuleCollectionSubsystem::EquipModule(int32 SlotIndex, FName ModuleId, FString& OutError)
{
	OutError.Reset();
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	if (SlotIndex < 0 || SlotIndex >= Settings->GetSlotCount())
	{
		OutError = TEXT("Invalid module slot.");
		return false;
	}
	if (!ModuleId.IsNone() && !OwnsModule(ModuleId))
	{
		OutError = TEXT("Only owned modules can be equipped.");
		return false;
	}
	TArray<FName> NewEquipped = EquippedModuleIds;
	NewEquipped.SetNum(Settings->GetSlotCount());
	NewEquipped[SlotIndex] = ModuleId;
	TArray<UASModuleDefinition*> Definitions;
	if (!Settings->ResolveLoadout(NewEquipped, Definitions, OutError)) return false;
	return CommitProfile(OwnedModuleIds, NewEquipped, OutError);
}
