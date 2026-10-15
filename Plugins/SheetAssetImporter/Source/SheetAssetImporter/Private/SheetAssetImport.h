#pragma once

#include "SheetImportSubsystem.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSheetAssetImporter, Log, All);

namespace SheetAssetImport
{
	FSheetImportReport Import(USheetImportProfile* Profile, const FString& Csv, bool bPreview,
		TMap<FString, TStrongObjectPtr<UObject>>* PreviewAssets = nullptr);
}
