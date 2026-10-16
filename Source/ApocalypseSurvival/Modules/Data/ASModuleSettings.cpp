#include "Modules/Data/ASModuleSettings.h"

#include "Modules/Data/ASModuleDefinition.h"

UASModuleDefinition* UASModuleSettings::FindModule(FName ModuleId, FString& OutError) const
{
	OutError.Reset();
	UASModuleDefinition* Found = nullptr;
	if (!ModuleId.IsNone())
	{
		for (const TSoftObjectPtr<UASModuleDefinition>& Reference : Modules)
		{
			UASModuleDefinition* Definition = Reference.LoadSynchronous();
			if (IsValid(Definition) && Definition->ModuleId == ModuleId)
			{
				if (Found)
				{
					OutError = FString::Printf(TEXT("Duplicate module ID in catalog: %s"), *ModuleId.ToString());
					return nullptr;
				}
				Found = Definition;
			}
		}
	}
	if (!Found)
	{
		OutError = FString::Printf(TEXT("Module is not registered: %s"), *ModuleId.ToString());
	}
	return Found;
}

bool UASModuleSettings::ResolveLoadout(const TArray<FName>& ModuleIds, TArray<UASModuleDefinition*>& OutDefinitions, FString& OutError) const
{
	OutDefinitions.Reset();
	OutError.Reset();
	if (ModuleIds.Num() > GetSlotCount())
	{
		OutError = TEXT("The module loadout exceeds the slot limit.");
		return false;
	}
	TSet<FName> Seen;
	for (FName ModuleId : ModuleIds)
	{
		if (ModuleId.IsNone()) continue;
		if (Seen.Contains(ModuleId))
		{
			OutError = TEXT("The same module cannot occupy multiple slots.");
			OutDefinitions.Reset();
			return false;
		}
		UASModuleDefinition* Definition = FindModule(ModuleId, OutError);
		if (!Definition || !Definition->ValidateDefinition(OutError))
		{
			OutDefinitions.Reset();
			return false;
		}
		Seen.Add(ModuleId);
		OutDefinitions.Add(Definition);
	}
	return true;
}
