#include "SheetAssetIdResolver.h"

#include "SheetImportProfile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

FSheetAssetIdResolver::FSheetAssetIdResolver(const USheetImportProfile* Profile,
	const TMap<FString, TStrongObjectPtr<UObject>>* Preview)
	: IdProperty(Profile->ReferenceIdProperty), SearchPath(Profile->ReferenceSearchPath.TrimStartAndEnd()), PreviewAssets(Preview)
{
	SearchPath.RemoveFromEnd(TEXT("/"));
}

bool FSheetAssetIdResolver::ValidateSettings(FString& Error) const
{
	if (IdProperty.IsNone()) return true;
	if (SearchPath != TEXT("/Game") && (!SearchPath.StartsWith(TEXT("/Game/"))
		|| !FPackageName::IsValidLongPackageName(SearchPath)))
	{
		Error = TEXT("Reference ID search folder must be /Game or a folder below /Game.");
		return false;
	}
	return true;
}

void FSheetAssetIdResolver::Add(FIndex& Index, UObject* Asset, const FString& Path) const
{
	if (!IsValid(Asset)) return;
	FProperty* Property = FindFProperty<FProperty>(Asset->GetClass(), IdProperty);
	FName Id;
	if (const FNameProperty* Name = CastField<FNameProperty>(Property)) Id = Name->GetPropertyValue_InContainer(Asset);
	else if (const FStrProperty* String = CastField<FStrProperty>(Property)) Id = FName(*String->GetPropertyValue_InContainer(Asset));
	if (!Id.IsNone()) Index.FindOrAdd(Id).Add({TStrongObjectPtr<UObject>(Asset), Path});
}

FSheetAssetIdResolver::FIndex& FSheetAssetIdResolver::GetIndex(UClass* Class)
{
	if (FIndex* Existing = Indices.Find(Class)) return *Existing;
	FIndex& Index = Indices.Add(Class);
	auto InScope = [this](const FString& ObjectPath)
	{
		return FPackageName::ObjectPathToPackageName(ObjectPath).StartsWith(SearchPath + TEXT("/"));
	};
	// A staged asset supersedes the saved asset at the same path, even when its ID has changed.
	if (PreviewAssets) for (const auto& Pair : *PreviewAssets)
		if (InScope(Pair.Key) && Pair.Value->IsA(Class)) Add(Index, Pair.Value.Get(), Pair.Key);

	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.WaitForCompletion();
	FARFilter Filter;
	Filter.ClassPaths.Add(Class->GetClassPathName()); Filter.bRecursiveClasses = true;
	Filter.PackagePaths.Add(FName(*SearchPath)); Filter.bRecursivePaths = true;
	TArray<FAssetData> Assets; Registry.GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		const FString Path = Asset.GetSoftObjectPath().ToString();
		if (!PreviewAssets || !PreviewAssets->Contains(Path)) Add(Index, Asset.GetAsset(), Path);
	}
	return Index;
}

bool FSheetAssetIdResolver::TryImport(FProperty* Property, void* Value, const FString& Text, FString& Error)
{
	const auto* Object = CastField<FObjectPropertyBase>(Property);
	if (IdProperty.IsNone() || !Object || !Object->PropertyClass->IsChildOf(UDataAsset::StaticClass())) return false;
	const FString IdText = Text.TrimStartAndEnd();
	if (IdText.Contains(TEXT("/")) || IdText.IsEmpty() || IdText.Equals(TEXT("None"), ESearchCase::IgnoreCase)
		|| IdText.Equals(TEXT("null"), ESearchCase::IgnoreCase)) return false;
	FProperty* Key = FindFProperty<FProperty>(Object->PropertyClass, IdProperty);
	if (!CastField<FNameProperty>(Key) && !CastField<FStrProperty>(Key))
	{
		Error = FString::Printf(TEXT("%s requires an FName/FString ID property named %s."), *Object->PropertyClass->GetName(), *IdProperty.ToString());
		return true;
	}
	const TArray<FReference>* Matches = GetIndex(Object->PropertyClass).Find(FName(*IdText));
	if (!Matches || Matches->IsEmpty())
	{
		Error = FString::Printf(TEXT("Unknown %s '%s' for %s in %s."), *IdProperty.ToString(), *IdText, *Object->PropertyClass->GetName(), *SearchPath);
		return true;
	}
	if (Matches->Num() != 1)
	{
		TArray<FString> Paths; for (const FReference& Match : *Matches) Paths.Add(Match.SourcePath);
		Paths.Sort();
		Error = FString::Printf(TEXT("Duplicate %s '%s': %s"), *IdProperty.ToString(), *IdText, *FString::Join(Paths, TEXT(", ")));
		return true;
	}
	UObject* Asset = (*Matches)[0].Asset.Get();
	if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property)) Soft->SetPropertyValue(Value, FSoftObjectPtr(Asset));
	else Object->SetObjectPropertyValue(Value, Asset);
	return true;
}
