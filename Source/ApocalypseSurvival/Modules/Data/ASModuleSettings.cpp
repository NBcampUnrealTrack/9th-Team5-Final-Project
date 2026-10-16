#include "Modules/Data/ASModuleSettings.h"

namespace
{
template <typename T>
const T* FindEquipmentRow(const TSoftObjectPtr<UDataTable>& Reference, FName Id, FString& OutError)
{
	OutError.Reset();
	const UDataTable* Table = Reference.LoadSynchronous();
	if (!Table || Table->GetRowStruct() != T::StaticStruct())
	{
		OutError = FString::Printf(TEXT("Configure a data table using %s in Project Settings > Modules."), *T::StaticStruct()->GetName());
		return nullptr;
	}
	const T* Row = Id.IsNone() ? nullptr : Table->FindRow<T>(Id, TEXT("Modules"), false);
	if (!Row) OutError = FString::Printf(TEXT("Missing equipment row: %s"), *Id.ToString());
	else if (!Row->Validate(OutError)) return nullptr;
	return Row;
}
}

const FASModuleBalanceRow* UASModuleSettings::FindModule(FName ModuleId, FString& OutError) const
{
	return FindEquipmentRow<FASModuleBalanceRow>(ModuleBalanceTable, ModuleId, OutError);
}

const FASModuleOptionBalanceRow* UASModuleSettings::FindOption(FName OptionId, FString& OutError) const
{
	return FindEquipmentRow<FASModuleOptionBalanceRow>(OptionBalanceTable, OptionId, OutError);
}

bool UASModuleSettings::ResolveOptionStat(const FASModuleOptionRoll& Roll, EASModuleStat& OutStat, FString& OutError) const
{
	OutError.Reset();
	OutStat = Roll.Stat;
	if (OutStat == EASModuleStat::None)
	{
		const FASModuleOptionBalanceRow* Option = FindOption(Roll.OptionId, OutError);
		if (!Option) return false;
		OutStat = Option->Stat;
	}
	if (static_cast<uint8>(OutStat) > static_cast<uint8>(EASModuleStat::MoveSpeed))
	{
		OutError = TEXT("The saved option stat is invalid.");
		return false;
	}
	return true;
}

bool UASModuleSettings::ResolveOptionPool(FName ModuleId, TArray<FASResolvedModuleOption>& OutPool, FString& OutError) const
{
	OutPool.Reset();
	const FASModuleBalanceRow* Module = FindModule(ModuleId, OutError);
	if (!Module) return false;
	if (Module->OptionPoolId.IsNone()) return true;
	const UDataTable* Table = OptionBalanceTable.LoadSynchronous();
	if (!Table || Table->GetRowStruct() != FASModuleOptionBalanceRow::StaticStruct())
	{
		OutError = TEXT("Configure the module option table using ASModuleOptionBalanceRow.");
		return false;
	}
	bool bPoolExists = false;
	for (const auto& Pair : Table->GetRowMap())
	{
		const FASModuleOptionBalanceRow& Option = *reinterpret_cast<const FASModuleOptionBalanceRow*>(Pair.Value);
		if (Option.PoolId != Module->OptionPoolId) continue;
		bPoolExists = true;
		if (!Option.Validate(OutError)) { OutPool.Reset(); return false; }
		if (Option.Weight > 0.0f) OutPool.Add({Pair.Key, Option});
	}
	if (!bPoolExists)
	{
		OutError = FString::Printf(TEXT("Module %s references a missing option pool: %s"), *ModuleId.ToString(), *Module->OptionPoolId.ToString());
		return false;
	}
	// Reimports may change DataTable map order; seeded rolls remain reproducible.
	OutPool.Sort([](const FASResolvedModuleOption& A, const FASResolvedModuleOption& B) { return A.OptionId.LexicalLess(B.OptionId); });
	return true;
}

bool UASModuleSettings::ValidateConfiguration(FString& OutError) const
{
	OutError.Reset();
	const UDataTable* Modules = ModuleBalanceTable.LoadSynchronous();
	const UDataTable* Options = OptionBalanceTable.LoadSynchronous();
	if (!Modules || Modules->GetRowStruct() != FASModuleBalanceRow::StaticStruct()
		|| !Options || Options->GetRowStruct() != FASModuleOptionBalanceRow::StaticStruct())
	{
		OutError = TEXT("Configure the module and option tables in Project Settings > Modules.");
		return false;
	}
	for (const auto& Pair : Options->GetRowMap()) if (!FindOption(Pair.Key, OutError)) return false;
	for (const auto& Pair : Modules->GetRowMap())
	{
		TArray<FASResolvedModuleOption> Pool;
		if (!ResolveOptionPool(Pair.Key, Pool, OutError)) return false;
	}
	return true;
}

bool UASModuleSettings::ResolveInstanceLoadout(const TArray<FASModuleInstanceData>& Instances, FString& OutError) const
{
	OutError.Reset();
	if (Instances.Num() > GetSlotCount()) { OutError = TEXT("The module loadout exceeds the slot limit."); return false; }
	TSet<FGuid> SeenInstances;
	TSet<FName> SeenCoreEffects;
	for (const FASModuleInstanceData& Instance : Instances)
	{
		if (Instance.IsEmpty()) continue;
		if (!Instance.InstanceId.IsValid() || Instance.ModuleId.IsNone() || Instance.Options.Num() > 3)
		{
			OutError = TEXT("Module instance identity or option count is invalid.");
			return false;
		}
		const FASModuleBalanceRow* Module = FindModule(Instance.ModuleId, OutError);
		if (!Module) return false;
		const FName CoreId = Module->GetCoreEffectIdentity();
		if (SeenInstances.Contains(Instance.InstanceId) || SeenCoreEffects.Contains(CoreId))
		{
			OutError = TEXT("The same module instance or core effect cannot occupy multiple slots.");
			return false;
		}
		TSet<FName> SeenOptions;
		for (const FASModuleOptionRoll& Roll : Instance.Options)
		{
			EASModuleStat Stat;
			if (Roll.OptionId.IsNone() || SeenOptions.Contains(Roll.OptionId) || !FMath::IsFinite(Roll.Value))
			{
				OutError = TEXT("A module has a duplicate option or an invalid rolled value.");
				return false;
			}
			if (!ResolveOptionStat(Roll, Stat, OutError)) return false;
			SeenOptions.Add(Roll.OptionId);
		}
		SeenInstances.Add(Instance.InstanceId);
		SeenCoreEffects.Add(CoreId);
	}
	return true;
}

bool UASModuleSettings::GenerateInstance(FName ModuleId, int32 OptionCount, FRandomStream& Random, FASModuleInstanceData& OutInstance, FString& OutError) const
{
	OutInstance = FASModuleInstanceData(); OutError.Reset();
	if (OptionCount < 0 || OptionCount > 3) { OutError = TEXT("Option count must be between zero and three."); return false; }
	TArray<FASResolvedModuleOption> Available;
	if (!ResolveOptionPool(ModuleId, Available, OutError)) return false;
	if (OptionCount > Available.Num()) { OutError = TEXT("Requested more options than the enabled unique option pool contains."); return false; }
	FASModuleInstanceData Result;
	Result.InstanceId = FGuid::NewGuid(); Result.ModuleId = ModuleId;
	for (int32 Index = 0; Index < OptionCount; ++Index)
	{
		double TotalWeight = 0.0;
		for (const FASResolvedModuleOption& Option : Available) TotalWeight += Option.Data.Weight;
		double Pick = Random.FRand() * TotalWeight;
		int32 ChosenIndex = Available.Num() - 1;
		for (int32 Candidate = 0; Candidate < Available.Num(); ++Candidate)
		{
			Pick -= Available[Candidate].Data.Weight;
			if (Pick < 0.0) { ChosenIndex = Candidate; break; }
		}
		const FASResolvedModuleOption& Chosen = Available[ChosenIndex];
		FASModuleOptionRoll& Roll = Result.Options.AddDefaulted_GetRef();
		Roll.OptionId = Chosen.OptionId;
		Roll.Stat = Chosen.Data.Stat;
		const double Alpha = Random.FRand();
		Roll.Value = static_cast<float>((1.0 - Alpha) * Chosen.Data.MinValue + Alpha * Chosen.Data.MaxValue);
		Available.RemoveAt(ChosenIndex);
	}
	OutInstance = MoveTemp(Result);
	return true;
}

bool UASModuleSettings::GenerateInstanceFromConfiguredWeights(FName ModuleId, FRandomStream& Random, FASModuleInstanceData& OutInstance, FString& OutError) const
{
	OutInstance = FASModuleInstanceData(); OutError.Reset();
	if (OptionCountWeights.Num() != 4) { OutError = TEXT("Configure four option-count weights (for 0, 1, 2, and 3) before weighted generation."); return false; }
	TArray<FASResolvedModuleOption> Pool;
	if (!ResolveOptionPool(ModuleId, Pool, OutError)) return false;
	double Total = 0.0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Weight = OptionCountWeights[Index];
		if (!FMath::IsFinite(Weight) || Weight < 0.0f) { OutError = TEXT("Option-count weights must be finite and non-negative."); return false; }
		if (Weight > 0.0f && Index > Pool.Num()) { OutError = TEXT("Option-count weights request more options than the module pool can provide."); return false; }
		Total += Weight;
	}
	if (Total <= 0.0) { OutError = TEXT("At least one option-count weight must be positive."); return false; }
	double Pick = Random.FRand() * Total;
	int32 ChosenCount = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (OptionCountWeights[Index] <= 0.0f) continue;
		ChosenCount = Index;
		Pick -= OptionCountWeights[Index];
		if (Pick < 0.0) break;
	}
	return GenerateInstance(ModuleId, ChosenCount, Random, OutInstance, OutError);
}