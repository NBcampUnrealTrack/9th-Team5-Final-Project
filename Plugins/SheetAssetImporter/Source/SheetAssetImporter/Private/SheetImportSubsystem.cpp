#include "SheetImportSubsystem.h"

#include "SheetAssetImport.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
	bool IsIdentifier(const FString& Text, bool bDigitsOnly = false)
	{
		if (Text.IsEmpty() || Text.Len() > 256) return false;
		for (const TCHAR C : Text)
			if (bDigitsOnly ? !FChar::IsDigit(C) : !(FChar::IsAlnum(C) || C == TEXT('-') || C == TEXT('_'))) return false;
		return true;
	}

	FString UrlParameter(FString Url, const FString& Key)
	{
		Url.ReplaceInline(TEXT("#"), TEXT("&"));
		Url.ReplaceInline(TEXT("?"), TEXT("&"));
		TArray<FString> Parts;
		Url.ParseIntoArray(Parts, TEXT("&"));
		FString Result;
		for (const FString& Part : Parts)
		{
			FString Name, Value;
			if (Part.Split(TEXT("="), &Name, &Value) && Name == Key) Result = FGenericPlatformHttp::UrlDecode(Value);
		}
		return Result;
	}

	FString CsvCell(FString Text)
	{
		Text.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return TEXT("\"") + Text + TEXT("\"");
	}

	bool SheetsJsonToCsv(const FString& Body, FString& Csv)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root)) return false;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root->TryGetArrayField(TEXT("values"), Values) || Values->IsEmpty()) return false;
		const TArray<TSharedPtr<FJsonValue>>* Header = nullptr;
		if (!(*Values)[0]->TryGetArray(Header) || Header->IsEmpty()) return false;
		const int32 Width = Header->Num();
		for (const TSharedPtr<FJsonValue>& Record : *Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
			if (!Record->TryGetArray(Cells) || Cells->Num() > Width) return false;
			for (int32 Index = 0; Index < Width; ++Index)
			{
				if (Index) Csv += TEXT(",");
				FString Text;
				if (Index < Cells->Num())
				{
					if ((*Cells)[Index]->Type == EJson::String) Text = (*Cells)[Index]->AsString();
					else if ((*Cells)[Index]->Type == EJson::Boolean) Text = (*Cells)[Index]->AsBool() ? TEXT("true") : TEXT("false");
					else if ((*Cells)[Index]->Type == EJson::Number) Text = FString::Printf(TEXT("%.17g"), (*Cells)[Index]->AsNumber());
					else if ((*Cells)[Index]->Type != EJson::Null) return false;
				}
				Csv += CsvCell(Text);
			}
			Csv += TEXT("\n");
		}
		return true;
	}
}

void USheetImportSubsystem::Deinitialize()
{
	CancelImport();
	Super::Deinitialize();
}

bool USheetImportSubsystem::StartImport(USheetImportProfile* Profile, const bool bPreviewOnly)
{
	if (bRunning || !IsValid(Profile) || !IsInGameThread()) return false;
	// Keep the destinations/mappings stable if the user edits a profile during its download.
	Queue = { DuplicateObject<USheetImportProfile>(Profile, GetTransientPackage()) };
	QueuePaths = { Profile->GetPathName() };
	QueueIndex = 0;
	bPreview = bPreviewOnly;
	bRunning = true;
	LastReport = FSheetImportReport();
	LastReport.bPreviewOnly = bPreview;
	PreviewAssets.Reset();
	RunNext();
	return true;
}

bool USheetImportSubsystem::StartImportPlan(USheetImportPlan* Plan, const bool bPreviewOnly)
{
	if (bRunning || !IsValid(Plan) || !IsInGameThread() || Plan->Profiles.IsEmpty() || Plan->Profiles.Num() > 128) return false;
	TArray<TObjectPtr<USheetImportProfile>> Profiles;
	TArray<FString> Paths;
	for (const auto& Ref : Plan->Profiles)
	{
		USheetImportProfile* Profile = Ref.LoadSynchronous();
		if (!IsValid(Profile)) return false;
		Profiles.Add(DuplicateObject<USheetImportProfile>(Profile, GetTransientPackage()));
		Paths.Add(Profile->GetPathName());
	}
	Queue = MoveTemp(Profiles);
	QueuePaths = MoveTemp(Paths);
	QueueIndex = 0;
	bPreview = bPreviewOnly;
	bRunning = true;
	LastReport = FSheetImportReport();
	LastReport.bPreviewOnly = bPreview;
	PreviewAssets.Reset();
	RunNext();
	return true;
}

FSheetImportReport USheetImportSubsystem::ImportCsvText(USheetImportProfile* Profile, const FString& CsvText, const bool bPreviewOnly)
{
	if (bRunning)
	{
		FSheetImportReport Report;
		Report.Messages.Add(TEXT("Another import is already running."));
		return Report;
	}
	LastReport = SheetAssetImport::Import(Profile, CsvText, bPreviewOnly);
	if (IsValid(Profile)) ProfileReports.Add(Profile->GetPathName(), LastReport);
	return LastReport;
}

FSheetImportReport USheetImportSubsystem::GetProfileReport(USheetImportProfile* Profile) const
{
	if (IsValid(Profile))
		if (const FSheetImportReport* Report = ProfileReports.Find(Profile->GetPathName())) return *Report;
	return FSheetImportReport();
}

FString USheetImportSubsystem::GetActiveProfilePath() const
{
	return bRunning && QueuePaths.IsValidIndex(QueueIndex) ? QueuePaths[QueueIndex] : FString();
}

FString USheetImportSubsystem::GetDownloadUrl(USheetImportProfile* Profile, FString& OutError) const
{
	OutError.Reset();
	if (!IsValid(Profile)) { OutError = TEXT("가져오기 설정이 없습니다."); return FString(); }
	if (Profile->Source == ESheetImportSource::CsvUrl)
	{
		const FString Url = Profile->CsvUrl.TrimStartAndEnd();
		if (Url.StartsWith(TEXT("https://")) || Url.StartsWith(TEXT("http://"))) return Url;
		OutError = TEXT("CSV URL은 http:// 또는 https://로 시작해야 합니다.");
		return FString();
	}
	if (Profile->Source == ESheetImportSource::LocalCsv) { OutError = TEXT("로컬 CSV에는 다운로드 URL이 없습니다."); return FString(); }
	const FString Input = (Profile->SheetUrl.IsEmpty() ? Profile->SpreadsheetId : Profile->SheetUrl).TrimStartAndEnd();
	FString Id = Input;
	FString Gid = Profile->SheetGid;
	bool bPublished = false;
	if (Input.Contains(TEXT("://")))
	{
		const FString Prefix = TEXT("https://docs.google.com/spreadsheets/d/");
		if (!Input.StartsWith(Prefix)) { OutError = TEXT("구글 스프레드시트의 전체 URL을 붙여 넣어 주세요."); return FString(); }
		FString Tail = Input.Mid(Prefix.Len());
		if (Tail.StartsWith(TEXT("e/"))) { bPublished = true; Tail.RemoveAt(0, 2); }
		int32 End = Tail.Len();
		for (const TCHAR Delimiter : { TEXT('/'), TEXT('?'), TEXT('#') })
		{
			int32 At;
			if (Tail.FindChar(Delimiter, At)) End = FMath::Min(End, At);
		}
		Id = Tail.Left(End);
		const FString UrlGid = UrlParameter(Input, TEXT("gid"));
		if (!UrlGid.IsEmpty()) Gid = UrlGid;
	}
	if (!IsIdentifier(Id)) { OutError = TEXT("올바른 시트 URL 또는 Spreadsheet ID를 입력해 주세요."); return FString(); }
	if (Profile->Source == ESheetImportSource::GoogleSheetsApi)
	{
		if (bPublished) { OutError = TEXT("비공개 API에는 웹 게시 링크가 아닌 원본 시트 URL이 필요합니다."); return FString(); }
		FString Range = Profile->SheetRange.TrimStartAndEnd();
		if (Range.IsEmpty() && !Profile->SheetName.TrimStartAndEnd().IsEmpty())
			Range = TEXT("'") + Profile->SheetName.Replace(TEXT("'"), TEXT("''")) + TEXT("'");
		if (Range.IsEmpty()) { OutError = TEXT("비공개 API를 사용할 때는 탭 이름 또는 A1 범위를 지정해 주세요."); return FString(); }
		return FString::Printf(TEXT("https://sheets.googleapis.com/v4/spreadsheets/%s/values/%s?valueRenderOption=UNFORMATTED_VALUE&majorDimension=ROWS"),
			*Id, *FGenericPlatformHttp::UrlEncode(Range));
	}
	if (!IsIdentifier(Gid, true)) { OutError = TEXT("탭 ID(gid)는 숫자여야 합니다."); return FString(); }
	if (bPublished)
	{
		if (!Profile->SheetName.IsEmpty()) { OutError = TEXT("웹 게시 링크에서는 탭 이름을 비우고 원하는 탭의 링크를 붙여 넣어 주세요."); return FString(); }
		return FString::Printf(TEXT("https://docs.google.com/spreadsheets/d/e/%s/pub?output=csv&single=true&gid=%s"), *Id, *Gid);
	}
	if (!Profile->SheetName.IsEmpty() || !Profile->CsvRange.IsEmpty())
	{
		FString Url = FString::Printf(TEXT("https://docs.google.com/spreadsheets/d/%s/gviz/tq?tqx=out:csv&headers=1"), *Id);
		Url += Profile->SheetName.IsEmpty() ? TEXT("&gid=") + Gid : TEXT("&sheet=") + FGenericPlatformHttp::UrlEncode(Profile->SheetName);
		if (!Profile->CsvRange.IsEmpty()) Url += TEXT("&range=") + FGenericPlatformHttp::UrlEncode(Profile->CsvRange);
		return Url;
	}
	return FString::Printf(TEXT("https://docs.google.com/spreadsheets/d/%s/export?format=csv&gid=%s"), *Id, *Gid);
}

void USheetImportSubsystem::CancelImport()
{
	if (ActiveRequest)
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
	if (bRunning)
	{
		LastReport.bSuccess = false;
		LastReport.Messages.Add(TEXT("Import cancelled. Profiles already applied remain imported."));
		if (QueuePaths.IsValidIndex(QueueIndex))
		{
			FSheetImportReport Report;
			Report.bPreviewOnly = bPreview;
			Report.Messages.Add(TEXT("가져오기를 취소했습니다."));
			ProfileReports.Add(QueuePaths[QueueIndex], MoveTemp(Report));
		}
	}
	Queue.Reset();
	QueuePaths.Reset();
	PreviewAssets.Reset();
	bRunning = false;
}

void USheetImportSubsystem::RunNext()
{
	if (!bRunning) return;
	if (QueueIndex >= Queue.Num()) { LastReport.bSuccess = true; Finish(); return; }
	USheetImportProfile* Profile = Queue[QueueIndex];
	ProfileReports.Remove(QueuePaths[QueueIndex]);
	UE_LOG(LogSheetAssetImporter, Log, TEXT("%s %s (%d/%d)"), bPreview ? TEXT("Previewing") : TEXT("Importing"), *Profile->GetName(), QueueIndex + 1, Queue.Num());
	if (Profile->Source == ESheetImportSource::LocalCsv)
	{
		FString Path = Profile->LocalCsvFile.FilePath;
		if (FPaths::IsRelative(Path)) Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Path);
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *Path)) { Fail(TEXT("Could not read the configured local CSV file.")); return; }
		ConsumeCsv(Csv);
		return;
	}
	FString UrlError;
	const FString Url = GetDownloadUrl(Profile, UrlError);
	if (Url.IsEmpty()) { Fail(UrlError); return; }
	FString Token;
	if (Profile->Source == ESheetImportSource::GoogleSheetsApi)
	{
		if (Profile->AccessTokenEnvironmentVariable.IsEmpty()) { Fail(TEXT("Access-token environment variable name is empty.")); return; }
		Token = FPlatformMisc::GetEnvironmentVariable(*Profile->AccessTokenEnvironmentVariable);
		if (Token.IsEmpty()) { Fail(TEXT("The configured OAuth access-token environment variable is empty. Restart the editor after setting it.")); return; }
	}
	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetVerb(TEXT("GET"));
	ActiveRequest->SetURL(Url);
	ActiveRequest->SetTimeout(60.0f);
	ActiveRequest->SetHeader(TEXT("Accept"), Profile->Source == ESheetImportSource::GoogleSheetsApi ? TEXT("application/json") : TEXT("text/csv"));
	if (!Token.IsEmpty()) ActiveRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	ActiveRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
	const TWeakObjectPtr<USheetImportSubsystem> WeakThis(this);
	const bool bApi = Profile->Source == ESheetImportSource::GoogleSheetsApi;
	ActiveRequest->OnProcessRequestComplete().BindLambda([WeakThis, bApi](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
	{
		USheetImportSubsystem* Self = WeakThis.Get();
		if (!Self || !Self->bRunning || Self->ActiveRequest != Request) return;
		Self->ActiveRequest.Reset();
		if (!bConnected || !Response) { Self->Fail(TEXT("Download failed or timed out.")); return; }
		if (Response->GetResponseCode() != 200)
		{
			Self->Fail(FString::Printf(TEXT("Download returned HTTP %d. Check sheet sharing, token permissions/expiry, and the source settings."), Response->GetResponseCode()));
			return;
		}
		if (Response->GetContent().Num() > 20 * 1024 * 1024) { Self->Fail(TEXT("Response exceeds the 20 MiB download limit.")); return; }
		FString Csv = Response->GetContentAsString();
		if (bApi)
		{
			FString Converted;
			if (!SheetsJsonToCsv(Csv, Converted)) { Self->Fail(TEXT("Google Sheets API returned no usable rows. Include a header in the A1 range.")); return; }
			Csv = MoveTemp(Converted);
		}
		Self->ConsumeCsv(Csv);
	});
	if (!ActiveRequest->ProcessRequest()) { ActiveRequest.Reset(); Fail(TEXT("Could not start the HTTP request.")); }
}

void USheetImportSubsystem::ConsumeCsv(const FString& Csv)
{
	if (!bRunning || !Queue.IsValidIndex(QueueIndex)) return;
	const FSheetImportReport Report = SheetAssetImport::Import(Queue[QueueIndex], Csv, bPreview, bPreview ? &PreviewAssets : nullptr);
	ProfileReports.Add(QueuePaths[QueueIndex], Report);
	LastReport.RowCount += Report.RowCount;
	LastReport.CreatedAssetCount += Report.CreatedAssetCount;
	LastReport.UpdatedAssetCount += Report.UpdatedAssetCount;
	LastReport.AssetPaths.Append(Report.AssetPaths);
	for (const FString& Message : Report.Messages) LastReport.Messages.Add(Queue[QueueIndex]->GetName() + TEXT(": ") + Message);
	if (!Report.bSuccess) { Fail(TEXT("Stopped at the first failed profile. Earlier applied profiles remain imported.")); return; }
	++QueueIndex;
	RunNext();
}

void USheetImportSubsystem::Fail(const FString& Message)
{
	LastReport.bSuccess = false;
	LastReport.Messages.Add(Message);
	if (QueuePaths.IsValidIndex(QueueIndex))
	{
		FSheetImportReport Report = ProfileReports.FindRef(QueuePaths[QueueIndex]);
		Report.bSuccess = false;
		Report.bPreviewOnly = bPreview;
		Report.Messages.Add(Message);
		ProfileReports.Add(QueuePaths[QueueIndex], MoveTemp(Report));
	}
	Finish();
}

void USheetImportSubsystem::Finish()
{
	for (const FString& Message : LastReport.Messages)
	{
		if (LastReport.bSuccess) { UE_LOG(LogSheetAssetImporter, Log, TEXT("%s"), *Message); }
		else { UE_LOG(LogSheetAssetImporter, Warning, TEXT("%s"), *Message); }
	}
	const FString Summary = FString::Printf(TEXT("Sheet %s %s: %d rows, %d new / %d updated assets. See Output Log (LogSheetAssetImporter)."),
		bPreview ? TEXT("preview") : TEXT("import"), LastReport.bSuccess ? TEXT("passed") : TEXT("failed"),
		LastReport.RowCount, LastReport.CreatedAssetCount, LastReport.UpdatedAssetCount);
	UE_LOG(LogSheetAssetImporter, Log, TEXT("%s"), *Summary);
	if (!IsRunningCommandlet())
	{
		FNotificationInfo Info(FText::FromString(Summary));
		Info.ExpireDuration = 8.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	Queue.Reset();
	QueuePaths.Reset();
	PreviewAssets.Reset();
	bRunning = false;
}
