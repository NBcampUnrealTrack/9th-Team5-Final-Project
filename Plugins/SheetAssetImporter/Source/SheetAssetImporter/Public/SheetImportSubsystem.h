#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "SheetImportProfile.h"
#include "UObject/StrongObjectPtr.h"
#include "SheetImportSubsystem.generated.h"

class IHttpRequest;

USTRUCT(BlueprintType)
struct SHEETASSETIMPORTER_API FSheetImportReport
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Import")
	bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Import")
	bool bPreviewOnly = false;
	UPROPERTY(BlueprintReadOnly, Category = "Import")
	int32 RowCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Import")
	int32 CreatedAssetCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Import")
	int32 UpdatedAssetCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Import")
	TArray<FString> AssetPaths;
	UPROPERTY(BlueprintReadOnly, Category = "Import")
	TArray<FString> Messages;
};

/** Editor scripting API and async source loader; all asset writes occur on the game/editor thread. */
UCLASS()
class SHEETASSETIMPORTER_API USheetImportSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable, Category = "Sheet Import")
	bool StartImport(USheetImportProfile* Profile, bool bPreviewOnly = true);
	UFUNCTION(BlueprintCallable, Category = "Sheet Import")
	bool StartImportPlan(USheetImportPlan* Plan, bool bPreviewOnly = true);
	/** Synchronous offline entry point, also suitable for editor Python/automation. */
	UFUNCTION(BlueprintCallable, Category = "Sheet Import")
	FSheetImportReport ImportCsvText(USheetImportProfile* Profile, const FString& CsvText, bool bPreviewOnly = true);
	UFUNCTION(BlueprintPure, Category = "Sheet Import")
	bool IsImportRunning() const { return bRunning; }
	UFUNCTION(BlueprintPure, Category = "Sheet Import")
	FSheetImportReport GetLastReport() const { return LastReport; }
	UFUNCTION(BlueprintPure, Category = "Sheet Import")
	FSheetImportReport GetProfileReport(USheetImportProfile* Profile) const;
	UFUNCTION(BlueprintPure, Category = "Sheet Import")
	FString GetActiveProfilePath() const;
	UFUNCTION(BlueprintPure, Category = "Sheet Import")
	FString GetDownloadUrl(USheetImportProfile* Profile, FString& OutError) const;
	int32 GetCompletedProfileCount() const { return QueueIndex; }
	int32 GetQueuedProfileCount() const { return Queue.Num(); }
	UFUNCTION(BlueprintCallable, Category = "Sheet Import")
	void CancelImport();

private:
	void RunNext();
	void ConsumeCsv(const FString& Csv);
	void Fail(const FString& Message);
	void Finish();
	UPROPERTY(Transient)
	TArray<TObjectPtr<USheetImportProfile>> Queue;
	TArray<FString> QueuePaths;
	TMap<FString, FSheetImportReport> ProfileReports;
	UPROPERTY(Transient)
	FSheetImportReport LastReport;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	/** Planned object paths resolve to transient candidates during a plan preview. */
	TMap<FString, TStrongObjectPtr<UObject>> PreviewAssets;
	int32 QueueIndex = 0;
	bool bRunning = false;
	bool bPreview = true;
};
