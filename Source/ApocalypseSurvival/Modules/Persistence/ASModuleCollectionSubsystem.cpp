#include "Modules/Persistence/ASModuleCollectionSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/Data/ASModuleSettings.h"
#include "Modules/Persistence/ASModuleSaveGame.h"

namespace
{
TArray<FASModuleInstanceData> BuildLoadout(const TArray<FASModuleInstanceData>& Owned, const TArray<FGuid>& Equipped)
{
	TArray<FASModuleInstanceData> Result;
	Result.SetNum(Equipped.Num());
	for (int32 Slot = 0; Slot < Equipped.Num(); ++Slot)
	{
		if (!Equipped[Slot].IsValid()) continue;
		if (const FASModuleInstanceData* Found = Owned.FindByPredicate([&Equipped, Slot](const FASModuleInstanceData& Entry)
			{ return Entry.InstanceId == Equipped[Slot]; })) Result[Slot] = *Found;
	}
	return Result;
}

FASModuleInstanceData ResolveSavedStats(FASModuleInstanceData Instance)
{
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	for (FASModuleOptionRoll& Roll : Instance.Options)
	{
		EASModuleStat Stat;
		FString Error;
		if (Settings->ResolveOptionStat(Roll, Stat, Error)) Roll.Stat = Stat;
	}
	return Instance;
}
}

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

TArray<FASModuleInstanceData> UASModuleCollectionSubsystem::GetEquippedModules() const
{
	return BuildLoadout(OwnedModules, EquippedModuleInstanceIds);
}

bool UASModuleCollectionSubsystem::OwnsModuleInstance(FGuid InstanceId) const
{
	return InstanceId.IsValid() && OwnedModules.ContainsByPredicate([InstanceId](const FASModuleInstanceData& Entry) { return Entry.InstanceId == InstanceId; });
}

bool UASModuleCollectionSubsystem::LoadProfile(FString& OutError)
{
	OutError.Reset();
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	TArray<FName> LoadedUnlocked;
	TArray<FASModuleInstanceData> LoadedOwned;
	TArray<FGuid> LoadedEquipped;
	FASQuestSaveData LoadedQuests;
	LoadedEquipped.SetNum(Settings->GetSlotCount());
	const FString SaveSlotName = GetSaveSlotName();
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		const UASModuleSaveGame* Save = Cast<UASModuleSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
		if (!Save || (Save->DataVersion != UASModuleSaveGame::LegacyModuleVersion && Save->DataVersion != UASModuleSaveGame::CurrentVersion))
		{
			OutError = TEXT("Module save is unreadable or has an unsupported version. Existing data was preserved.");
			bProfileLoaded = false;
			return false;
		}
		if (Save->DataVersion == UASModuleSaveGame::CurrentVersion)
		{
			if (!Save->QuestProgress.Validate(OutError))
			{
				bProfileLoaded = false;
				return false;
			}
			LoadedQuests = Save->QuestProgress;
		}
		for (FName Id : Save->UnlockedModuleIds) if (!Id.IsNone()) LoadedUnlocked.AddUnique(Id);
		TSet<FGuid> SeenInstances;
		for (const FASModuleInstanceData& Entry : Save->OwnedModules)
		{
			if (!Entry.InstanceId.IsValid() || Entry.ModuleId.IsNone() || SeenInstances.Contains(Entry.InstanceId)) continue;
			SeenInstances.Add(Entry.InstanceId);
			// Older saves did not capture Stat. Resolve it without changing the rolled value or ID.
			LoadedOwned.Add(ResolveSavedStats(Entry));
		}
		for (int32 Slot = 0; Slot < FMath::Min(Save->EquippedModuleInstanceIds.Num(), LoadedEquipped.Num()); ++Slot)
		{
			const FGuid Id = Save->EquippedModuleInstanceIds[Slot];
			if (!Id.IsValid()) continue;
			const FASModuleInstanceData* Instance = LoadedOwned.FindByPredicate([Id](const FASModuleInstanceData& Entry) { return Entry.InstanceId == Id; });
			if (!Instance || !LoadedUnlocked.Contains(Instance->ModuleId)) continue;
			TArray<FGuid> Candidate = LoadedEquipped;
			Candidate[Slot] = Id;
			FString Error;
			if (Settings->ResolveInstanceLoadout(BuildLoadout(LoadedOwned, Candidate), Error)) LoadedEquipped[Slot] = Id;
		}
	}
	UnlockedModuleIds = MoveTemp(LoadedUnlocked);
	OwnedModules = MoveTemp(LoadedOwned);
	EquippedModuleInstanceIds = MoveTemp(LoadedEquipped);
	QuestProgress = MoveTemp(LoadedQuests);
	bProfileLoaded = true;
	OnCollectionChanged.Broadcast();
	return true;
}

bool UASModuleCollectionSubsystem::CommitProfile(const TArray<FName>& NewUnlocked, const TArray<FASModuleInstanceData>& NewOwned, const TArray<FGuid>& NewEquipped, FString& OutError)
{
	return CommitPlayerProfile(NewUnlocked, NewOwned, NewEquipped, QuestProgress, OutError);
}

bool UASModuleCollectionSubsystem::SaveQuestProgress(const FASQuestSaveData& Progress, FString& OutError)
{
	return CommitPlayerProfile(UnlockedModuleIds, OwnedModules, EquippedModuleInstanceIds, Progress, OutError);
}

bool UASModuleCollectionSubsystem::CommitPlayerProfile(const TArray<FName>& NewUnlocked,
	const TArray<FASModuleInstanceData>& NewOwned, const TArray<FGuid>& NewEquipped,
	const FASQuestSaveData& NewQuests, FString& OutError)
{
	OutError.Reset();
	if (!bProfileLoaded) { OutError = TEXT("Load a valid module profile before modifying progression."); return false; }
	if (!NewQuests.Validate(OutError)) return false;
	UASModuleSaveGame* Save = Cast<UASModuleSaveGame>(UGameplayStatics::CreateSaveGameObject(UASModuleSaveGame::StaticClass()));
	if (!Save) { OutError = TEXT("Could not create module save data."); return false; }
	Save->UnlockedModuleIds = NewUnlocked;
	Save->OwnedModules = NewOwned;
	Save->EquippedModuleInstanceIds = NewEquipped;
	Save->QuestProgress = NewQuests;
	if (!UGameplayStatics::SaveGameToSlot(Save, GetSaveSlotName(), 0)) { OutError = TEXT("Could not save module progression. No in-memory changes were committed."); return false; }
	const bool bModulesChanged = UnlockedModuleIds != NewUnlocked || OwnedModules != NewOwned || EquippedModuleInstanceIds != NewEquipped;
	UnlockedModuleIds = NewUnlocked;
	OwnedModules = NewOwned;
	EquippedModuleInstanceIds = NewEquipped;
	QuestProgress = NewQuests;
	if (bModulesChanged) OnCollectionChanged.Broadcast();
	return true;
}

bool UASModuleCollectionSubsystem::UnlockModule(FName ModuleId, FString& OutError)
{
	OutError.Reset();
	if (!bProfileLoaded) { OutError = TEXT("The module profile is not loaded."); return false; }
	if (!GetDefault<UASModuleSettings>()->FindModule(ModuleId, OutError)) return false;
	if (IsModuleUnlocked(ModuleId)) return true;
	TArray<FName> NewUnlocked = UnlockedModuleIds;
	NewUnlocked.Add(ModuleId);
	return CommitProfile(NewUnlocked, OwnedModules, EquippedModuleInstanceIds, OutError);
}

bool UASModuleCollectionSubsystem::GrantModuleInstance(const FASModuleInstanceData& Instance, FString& OutError)
{
	OutError.Reset();
	if (!bProfileLoaded) { OutError = TEXT("The module profile is not loaded."); return false; }
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	if (Instance.IsEmpty()) { OutError = TEXT("An empty module cannot be granted."); return false; }
	const FASModuleInstanceData Reward = ResolveSavedStats(Instance);
	if (!Settings->ResolveInstanceLoadout({Reward}, OutError)) return false;
	if (const FASModuleInstanceData* Existing = OwnedModules.FindByPredicate([&Reward](const FASModuleInstanceData& Entry) { return Entry.InstanceId == Reward.InstanceId; }))
	{
		if (ResolveSavedStats(*Existing) == Reward) return true;
		OutError = TEXT("A different module reward already uses this instance ID.");
		return false;
	}
	TArray<FASModuleInstanceData> NewOwned = OwnedModules;
	NewOwned.Add(Reward);
	TArray<FName> NewUnlocked = UnlockedModuleIds;
	NewUnlocked.AddUnique(Reward.ModuleId);
	return CommitProfile(NewUnlocked, NewOwned, EquippedModuleInstanceIds, OutError);
}

bool UASModuleCollectionSubsystem::EquipModule(int32 SlotIndex, FGuid InstanceId, FString& OutError)
{
	OutError.Reset();
	if (!bProfileLoaded) { OutError = TEXT("The module profile is not loaded."); return false; }
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	if (SlotIndex < 0 || SlotIndex >= Settings->GetSlotCount()) { OutError = TEXT("Invalid module slot."); return false; }
	if (InstanceId.IsValid() && !OwnsModuleInstance(InstanceId)) { OutError = TEXT("Only owned module instances can be equipped."); return false; }
	TArray<FGuid> NewEquipped = EquippedModuleInstanceIds;
	NewEquipped.SetNum(Settings->GetSlotCount());
	NewEquipped[SlotIndex] = InstanceId;
	if (!Settings->ResolveInstanceLoadout(BuildLoadout(OwnedModules, NewEquipped), OutError)) return false;
	return CommitProfile(UnlockedModuleIds, OwnedModules, NewEquipped, OutError);
}
