#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SheetImportProfile.generated.h"

class UDataTable;

UENUM(BlueprintType)
enum class ESheetImportSource : uint8
{
	GoogleSheetsCsv,
	GoogleSheetsApi,
	CsvUrl,
	LocalCsv
};

UENUM(BlueprintType)
enum class ESheetImportTarget : uint8
{
	DataTable,
	DataAssets
};

UENUM(BlueprintType)
enum class ESheetCellFormat : uint8
{
	UnrealText,
	Json,
	Auto
};

USTRUCT(BlueprintType)
struct SHEETASSETIMPORTER_API FSheetColumnMapping
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FString Column;
	/** Reflected C++ property name, or nested struct path such as LootPreset.RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FString PropertyPath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	ESheetCellFormat Format = ESheetCellFormat::Auto;
	/** Missing column or an empty cell is an error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	bool bRequired = false;
	/** Otherwise an empty cell resets the property to its class/struct default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	bool bSkipEmpty = true;
};

/** Create through Miscellaneous > Data Asset. No dependency on a game's item or spawn types. */
UCLASS(BlueprintType)
class SHEETASSETIMPORTER_API USheetImportProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual bool IsEditorOnly() const override { return true; }

	/** Paste a normal Google Sheets URL, a published CSV URL, or the spreadsheet ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sheet", meta = (DisplayName = "시트 URL", EditCondition = "Source == ESheetImportSource::GoogleSheetsCsv || Source == ESheetImportSource::GoogleSheetsApi", EditConditionHides))
	FString SheetUrl;
	/** Optional: select by tab name instead of the gid embedded in the URL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sheet", meta = (DisplayName = "탭 이름 (선택)", EditCondition = "Source == ESheetImportSource::GoogleSheetsCsv || Source == ESheetImportSource::GoogleSheetsApi", EditConditionHides))
	FString SheetName;
	/** Optional: A1:Z1000, including the header. Empty imports the complete tab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sheet", AdvancedDisplay, meta = (DisplayName = "읽기 범위 (선택)", EditCondition = "Source == ESheetImportSource::GoogleSheetsCsv", EditConditionHides))
	FString CsvRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", AdvancedDisplay, meta = (DisplayName = "원본 방식"))
	ESheetImportSource Source = ESheetImportSource::GoogleSheetsCsv;
	/** ID between /d/ and /edit in a Google Sheets URL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "Source == ESheetImportSource::GoogleSheetsCsv || Source == ESheetImportSource::GoogleSheetsApi", EditConditionHides))
	FString SpreadsheetId;
	/** Tab ID in the URL's gid parameter, for the public CSV export. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "Source == ESheetImportSource::GoogleSheetsCsv", EditConditionHides))
	FString SheetGid = TEXT("0");
	/** A1 range including the header, e.g. Weapons!A1:Z1000. API mode requires an OAuth access token. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "Source == ESheetImportSource::GoogleSheetsApi", EditConditionHides))
	FString SheetRange;
	/** Name of an environment variable, never the token itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "Source == ESheetImportSource::GoogleSheetsApi", EditConditionHides))
	FString AccessTokenEnvironmentVariable = TEXT("GOOGLE_SHEETS_ACCESS_TOKEN");
	/** Published Google Sheets CSV URL or another CSV endpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "Source == ESheetImportSource::CsvUrl", EditConditionHides))
	FString CsvUrl;
	/** Absolute path or path relative to the project directory. UTF-8 CSV. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "Source == ESheetImportSource::LocalCsv", EditConditionHides))
	FFilePath LocalCsvFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (DisplayName = "생성 방식"))
	ESheetImportTarget Target = ESheetImportTarget::DataTable;
	/** Selecting an existing table supplies its row type and destination automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (DisplayName = "기존 테이블 (선택)", EditCondition = "Target == ESheetImportTarget::DataTable", EditConditionHides))
	TSoftObjectPtr<UDataTable> ExistingDataTable;
	/** Native struct object path, e.g. /Script/YourModule.YourRow; user-defined struct paths also work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (EditCondition = "Target == ESheetImportTarget::DataTable", EditConditionHides))
	FString RowStructPath;
	/** Package path without .uasset, e.g. /Game/Data/DT_Weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (EditCondition = "Target == ESheetImportTarget::DataTable", EditConditionHides))
	FString DataTablePath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (EditCondition = "Target == ESheetImportTarget::DataAssets", EditConditionHides))
	TSoftClassPtr<UDataAsset> DataAssetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (EditCondition = "Target == ESheetImportTarget::DataAssets", EditConditionHides))
	FString OutputDirectory = TEXT("/Game/ImportedData");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (EditCondition = "Target == ESheetImportTarget::DataAssets", EditConditionHides))
	FString AssetNamePrefix = TEXT("PDA_");

	/** Empty detects Name, ID, ItemId, RowName, AssetName, then the first header. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Columns", AdvancedDisplay, meta = (DisplayName = "식별자 열 (빈 값이면 자동)"))
	FString KeyColumn;
	/** Automatically map remaining matching headers. Explicit columns and destination paths take priority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Columns", AdvancedDisplay)
	bool bAutoMapColumns = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Columns", AdvancedDisplay)
	TArray<FSheetColumnMapping> ColumnMappings;

	/** Optional FName/FString property on referenced DataAssets, e.g. ItemId. Empty retains path-only imports. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Columns", meta = (DisplayName = "에셋 참조 ID 속성"))
	FName ReferenceIdProperty;
	/** Search this content folder and its descendants for referenced IDs. Asset names are not used as IDs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Columns", meta = (DisplayName = "참조 ID 검색 폴더"))
	FString ReferenceSearchPath = TEXT("/Game");

	/** Dashboard batches run smaller numbers first, then sort by asset path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", meta = (DisplayName = "일괄 가져오기 순서", ClampMin = "0"))
	int32 ImportOrder = 0;

	/** Start updates from existing values; otherwise use class/struct defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", AdvancedDisplay)
	bool bPreserveUnmappedValues = true;
	/** False keeps table rows absent from the sheet. DataAssets are never automatically deleted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", meta = (EditCondition = "Target == ESheetImportTarget::DataTable", EditConditionHides))
	bool bRemoveMissingTableRows = false;
	/** Run the target's native IsDataValid before changing any destination assets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", AdvancedDisplay)
	bool bValidateAssets = true;
	/** Resolve soft references as well as hard references, including references in arrays/structs/maps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", AdvancedDisplay)
	bool bValidateReferences = true;
	/** Default leaves successful imports dirty for review/save in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", meta = (DisplayName = "자동 저장"))
	bool bSaveAssets = false;
};

/** Ordered profiles: import referenced assets before tables/assets that use them. Stops at the first failure. */
UCLASS(BlueprintType)
class SHEETASSETIMPORTER_API USheetImportPlan : public UDataAsset
{
	GENERATED_BODY()
public:
	virtual bool IsEditorOnly() const override { return true; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import")
	TArray<TSoftObjectPtr<USheetImportProfile>> Profiles;
};
