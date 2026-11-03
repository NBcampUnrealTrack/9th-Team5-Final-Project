#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class USheetImportProfile;

/** Import-scoped lookup: indexes referenced DataAsset classes once, retaining staged preview overrides. */
class FSheetAssetIdResolver
{
public:
	FSheetAssetIdResolver(const USheetImportProfile* Profile, const TMap<FString, TStrongObjectPtr<UObject>>* Preview);
	bool ValidateSettings(FString& Error) const;
	/** Returns true when this is an ID reference, including a failed lookup (reported in Error). */
	bool TryImport(FProperty* Property, void* Value, const FString& Text, FString& Error);

private:
	struct FReference
	{
		TStrongObjectPtr<UObject> Asset;
		FString SourcePath;
	};
	using FIndex = TMap<FName, TArray<FReference>>;
	FIndex& GetIndex(UClass* Class);
	void Add(FIndex& Index, UObject* Asset, const FString& Path) const;
	FName IdProperty;
	FString SearchPath;
	const TMap<FString, TStrongObjectPtr<UObject>>* PreviewAssets;
	TMap<UClass*, FIndex> Indices;
};
