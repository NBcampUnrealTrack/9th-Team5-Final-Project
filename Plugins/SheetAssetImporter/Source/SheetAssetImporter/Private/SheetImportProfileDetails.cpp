#include "SheetImportEditor.h"

#include "SheetImportProfile.h"
#include "SheetImportSubsystem.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "IPropertyUtilities.h"
#include "IDetailsView.h"
#include "StructViewerModule.h"
#include "StructViewerFilter.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace SheetImportEditor
{
	static TArray<TWeakPtr<IDetailsView>> ProfileViews;

	bool CanRun()
	{
		if (IsEngineExitRequested() || !GEditor) return false;
		USheetImportSubsystem* Service = GEditor->GetEditorSubsystem<USheetImportSubsystem>();
		return Service && !Service->IsImportRunning();
	}

	void ClearProfileViews()
	{
		// Native property widgets may query editor subsystems during Slate destruction.
		// Release our property trees while those subsystems still exist.
		const auto Views = MoveTemp(ProfileViews);
		ProfileViews.Reset();
		for (const auto& WeakView : Views)
			if (const auto View = WeakView.Pin()) View->SetObject(nullptr, true);
	}

	bool Start(USheetImportProfile* Profile, const bool bPreview)
	{
		if (!CanRun()) return false;
		USheetImportSubsystem* Service = GEditor ? GEditor->GetEditorSubsystem<USheetImportSubsystem>() : nullptr;
		return Service && IsValid(Profile) && Service->StartImport(Profile, bPreview);
	}

	FText Status(USheetImportProfile* Profile)
	{
		if (IsEngineExitRequested()) return FText();
		USheetImportSubsystem* Service = GEditor ? GEditor->GetEditorSubsystem<USheetImportSubsystem>() : nullptr;
		if (!Service || !IsValid(Profile)) return FText();
		if (Service->GetActiveProfilePath() == Profile->GetPathName())
			return NSLOCTEXT("SheetAssetImporter", "Loading", "불러오는 중...");
		const FSheetImportReport Report = Service->GetProfileReport(Profile);
		if (Report.Messages.IsEmpty()) return NSLOCTEXT("SheetAssetImporter", "Ready", "아직 실행하지 않았습니다");
		if (!Report.bSuccess) return NSLOCTEXT("SheetAssetImporter", "Failed", "실패 · 아래 결과를 확인해 주세요");
		return FText::FromString(FString::Printf(TEXT("%s 완료 · %d행 · 새 에셋 %d / 갱신 %d"),
			Report.bPreviewOnly ? TEXT("검증") : TEXT("가져오기"), Report.RowCount, Report.CreatedAssetCount, Report.UpdatedAssetCount));
	}

	FText Result(USheetImportProfile* Profile)
	{
		if (IsEngineExitRequested()) return FText();
		USheetImportSubsystem* Service = GEditor ? GEditor->GetEditorSubsystem<USheetImportSubsystem>() : nullptr;
		if (!Service || !IsValid(Profile)) return FText();
		const FSheetImportReport Report = Service->GetProfileReport(Profile);
		return FText::FromString(FString::Join(Report.Messages, TEXT("\n")));
	}
}

namespace
{
	class FTableRowStructFilter : public IStructViewerFilter
	{
	public:
		virtual bool IsStructAllowed(const FStructViewerInitializationOptions&, const UScriptStruct* Struct,
			TSharedRef<FStructViewerFilterFuncs>) override
		{
			return Struct && Struct != FTableRowBase::StaticStruct()
				&& (Struct->IsChildOf(FTableRowBase::StaticStruct()) || Struct->IsA<UUserDefinedStruct>());
		}
		virtual bool IsUnloadedStructAllowed(const FStructViewerInitializationOptions&, const FSoftObjectPath&,
			TSharedRef<FStructViewerFilterFuncs>) override { return true; }
	};

	class FSheetImportProfileDetails : public IDetailCustomization
	{
	public:
		virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override
		{
			TArray<TWeakObjectPtr<UObject>> Objects;
			Builder.GetObjectsBeingCustomized(Objects);
			if (Objects.Num() != 1) return;
			USheetImportProfile* Profile = Cast<USheetImportProfile>(Objects[0].Get());
			if (!Profile) return;
			const auto View = Builder.GetDetailsViewSharedPtr();
			SheetImportEditor::ProfileViews.RemoveAll([](const auto& Weak) { return !Weak.IsValid(); });
			if (View && !SheetImportEditor::ProfileViews.ContainsByPredicate([View](const auto& Weak) { return Weak.Pin() == View; }))
				SheetImportEditor::ProfileViews.Add(View);
			const TWeakObjectPtr<USheetImportProfile> WeakProfile(Profile);
			for (const TCHAR* Category : { TEXT("Sheet"), TEXT("Source"), TEXT("Target"), TEXT("Columns"), TEXT("Import") }) Builder.HideCategory(Category);
			const TWeakPtr<IPropertyUtilities> Utilities = Builder.GetPropertyUtilities();
			auto Refresh = [Utilities]() { if (const auto Ptr = Utilities.Pin()) Ptr->ForceRefresh(); };
			for (const FName Name : { GET_MEMBER_NAME_CHECKED(USheetImportProfile, Target), GET_MEMBER_NAME_CHECKED(USheetImportProfile, Source), GET_MEMBER_NAME_CHECKED(USheetImportProfile, ExistingDataTable) })
				Builder.GetProperty(Name)->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(Refresh));

			IDetailCategoryBuilder& Sheet = Builder.EditCategory(TEXT("SheetInput"), NSLOCTEXT("SheetAssetImporter", "SourceSection", "1. 구글 시트"), ECategoryPriority::Important);
			if (Profile->Source == ESheetImportSource::GoogleSheetsCsv || Profile->Source == ESheetImportSource::GoogleSheetsApi)
			{
				Sheet.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, SheetUrl));
				Sheet.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, SheetName));
				Sheet.AddCustomRow(FText()).WholeRowContent()[SNew(STextBlock)
					.Text(NSLOCTEXT("SheetAssetImporter", "SheetHelp", "시트 링크를 그대로 붙여 넣으세요. 탭 이름을 비우면 링크의 탭을 사용합니다. 첫 행은 열 이름입니다."))
					.AutoWrapText(true)];
			}
			else if (Profile->Source == ESheetImportSource::LocalCsv) Sheet.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, LocalCsvFile));
			else Sheet.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, CsvUrl));

			IDetailCategoryBuilder& Output = Builder.EditCategory(TEXT("SheetOutput"), NSLOCTEXT("SheetAssetImporter", "OutputSection", "2. 만들 에셋"), ECategoryPriority::Important);
			Output.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, Target));
			if (Profile->Target == ESheetImportTarget::DataTable)
			{
				Output.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, ExistingDataTable));
				if (Profile->ExistingDataTable.IsNull())
				{
					const TSharedRef<IPropertyHandle> StructPath = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, RowStructPath));
					Output.AddCustomRow(NSLOCTEXT("SheetAssetImporter", "RowType", "행 구조체"))
					.NameContent()[SNew(STextBlock).Text(NSLOCTEXT("SheetAssetImporter", "RowType", "행 구조체"))]
					.ValueContent().MinDesiredWidth(250)
					[
						SNew(SComboButton).OnGetMenuContent_Lambda([StructPath]() -> TSharedRef<SWidget>
						{
							FStructViewerInitializationOptions Options;
							Options.StructFilter = MakeShared<FTableRowStructFilter>();
							Options.bShowNoneOption = true;
							Options.DisplayMode = EStructViewerDisplayMode::ListView;
							Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::Dynamic;
							return SNew(SBox).WidthOverride(420).HeightOverride(360)
							[
								FModuleManager::LoadModuleChecked<FStructViewerModule>(TEXT("StructViewer")).CreateStructViewer(Options,
									FOnStructPicked::CreateLambda([StructPath](const UScriptStruct* Struct)
									{
										StructPath->SetValue(Struct ? Struct->GetPathName() : FString());
										FSlateApplication::Get().DismissAllMenus();
									}))
							];
						})
						.ButtonContent()[SNew(STextBlock).Text_Lambda([WeakProfile]()
						{
							UScriptStruct* Struct = WeakProfile.IsValid() && !WeakProfile->RowStructPath.IsEmpty()
								? LoadObject<UScriptStruct>(nullptr, *WeakProfile->RowStructPath) : nullptr;
							return Struct ? Struct->GetDisplayNameText() : NSLOCTEXT("SheetAssetImporter", "ChooseStruct", "검색해서 행 구조체 선택...");
						})]
					];
					const TSharedRef<IPropertyHandle> Destination = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, DataTablePath));
					Output.AddCustomRow(NSLOCTEXT("SheetAssetImporter", "TableDestination", "생성할 테이블"))
					.NameContent()[SNew(STextBlock).Text(NSLOCTEXT("SheetAssetImporter", "TableDestination", "생성할 테이블"))]
					.ValueContent().MinDesiredWidth(250)[SNew(SButton).OnClicked_Lambda([WeakProfile, Destination]()
					{
						if (!WeakProfile.IsValid()) return FReply::Handled();
						FSaveAssetDialogConfig Config;
						Config.DialogTitleOverride = NSLOCTEXT("SheetAssetImporter", "ChooseTableDestination", "데이터 테이블을 만들 위치와 이름");
						Config.DefaultPath = WeakProfile->DataTablePath.IsEmpty() ? TEXT("/Game/ImportedData") : FPackageName::GetLongPackagePath(WeakProfile->DataTablePath);
						Config.DefaultAssetName = WeakProfile->DataTablePath.IsEmpty() ? TEXT("DT_Sheet") : FPackageName::GetLongPackageAssetName(WeakProfile->DataTablePath);
						Config.AssetClassNames.Add(UDataTable::StaticClass()->GetClassPathName());
						const FString Path = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreateModalSaveAssetDialog(Config);
						if (!Path.IsEmpty()) Destination->SetValue(FPackageName::ObjectPathToPackageName(Path));
						return FReply::Handled();
					}).Content()[SNew(STextBlock).Text_Lambda([WeakProfile]()
					{
						return WeakProfile.IsValid() && !WeakProfile->DataTablePath.IsEmpty() ? FText::FromString(WeakProfile->DataTablePath)
							: NSLOCTEXT("SheetAssetImporter", "ChooseTableLocation", "폴더와 테이블 이름 선택...");
					})]];
				}
			}
			else
			{
				Output.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, DataAssetClass)).DisplayName(NSLOCTEXT("SheetAssetImporter", "AssetClass", "PDA / 데이터 에셋 클래스"));
				const TSharedRef<IPropertyHandle> Directory = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, OutputDirectory));
				Output.AddCustomRow(NSLOCTEXT("SheetAssetImporter", "Folder", "생성 폴더"))
				.NameContent()[SNew(STextBlock).Text(NSLOCTEXT("SheetAssetImporter", "Folder", "생성 폴더"))]
				.ValueContent().MinDesiredWidth(250)[SNew(SComboButton).OnGetMenuContent_Lambda([WeakProfile, Directory]() -> TSharedRef<SWidget>
				{
					FPathPickerConfig Config;
					Config.DefaultPath = WeakProfile.IsValid() ? WeakProfile->OutputDirectory : TEXT("/Game");
					Config.bAllowReadOnlyFolders = false;
					Config.OnPathSelected = FOnPathSelected::CreateLambda([Directory](const FString& Path)
					{
						if (Path.StartsWith(TEXT("/Game"))) Directory->SetValue(Path);
						FSlateApplication::Get().DismissAllMenus();
					});
					return SNew(SBox).WidthOverride(380).HeightOverride(300)
						[FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreatePathPicker(Config)];
				}).ButtonContent()[SNew(STextBlock).Text_Lambda([WeakProfile]() { return WeakProfile.IsValid() ? FText::FromString(WeakProfile->OutputDirectory) : FText(); })]];
				Output.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, AssetNamePrefix)).DisplayName(NSLOCTEXT("SheetAssetImporter", "Prefix", "에셋 이름 접두사"));
			}
			Output.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, bSaveAssets));
			Output.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, ReferenceIdProperty));
			Output.AddProperty(GET_MEMBER_NAME_CHECKED(USheetImportProfile, ReferenceSearchPath));
			Output.AddCustomRow(FText()).WholeRowContent()[SNew(STextBlock)
				.Text(NSLOCTEXT("SheetAssetImporter", "ReferenceIdHelp", "아이템을 ID로 참조하려면 에셋 참조 ID 속성에 ItemId를 입력하세요. 시트의 에셋 참조 칸과 JSON 배열 안에도 ID를 쓸 수 있습니다. 빈 값이면 기존 경로 방식을 사용합니다."))
				.AutoWrapText(true)];

			IDetailCategoryBuilder& Run = Builder.EditCategory(TEXT("SheetRun"), NSLOCTEXT("SheetAssetImporter", "RunSection", "3. 실행 / 결과"), ECategoryPriority::Important);
			Run.AddCustomRow(FText()).WholeRowContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "Validate", "검증"))
					.IsEnabled_Static(&SheetImportEditor::CanRun)
					.OnClicked_Lambda([WeakProfile]() { SheetImportEditor::Start(WeakProfile.Get(), true); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "Load", "가져오기"))
					.IsEnabled_Static(&SheetImportEditor::CanRun)
					.OnClicked_Lambda([WeakProfile]() { SheetImportEditor::Start(WeakProfile.Get(), false); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth()
				[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "OpenDashboard", "대시보드"))
					.OnClicked_Lambda([] { FGlobalTabmanager::Get()->TryInvokeTab(SheetImportEditor::DashboardTabId); return FReply::Handled(); })]
			];
			Run.AddCustomRow(FText()).WholeRowContent()[SNew(STextBlock).Text_Lambda([WeakProfile]() { return SheetImportEditor::Status(WeakProfile.Get()); }).AutoWrapText(true)];
			Run.AddCustomRow(FText()).WholeRowContent()[SNew(STextBlock).Text_Lambda([WeakProfile]() { return SheetImportEditor::Result(WeakProfile.Get()); }).AutoWrapText(true)];

			IDetailCategoryBuilder& Help = Builder.EditCategory(TEXT("SheetColumnsHelp"), NSLOCTEXT("SheetAssetImporter", "ColumnsHelp", "시트 열 이름 안내"), ECategoryPriority::Default);
			auto ColumnNames = [WeakProfile]()
			{
				if (IsEngineExitRequested() || !WeakProfile.IsValid()) return FString();
				UStruct* Schema = nullptr;
				if (WeakProfile->Target == ESheetImportTarget::DataAssets) Schema = WeakProfile->DataAssetClass.LoadSynchronous();
				else if (UDataTable* Table = WeakProfile->ExistingDataTable.LoadSynchronous()) Schema = Table->RowStruct;
				else if (!WeakProfile->RowStructPath.IsEmpty()) Schema = LoadObject<UScriptStruct>(nullptr, *WeakProfile->RowStructPath);
				if (!Schema) return FString();
				TArray<FString> Names { TEXT("Name") };
				for (TFieldIterator<FProperty> It(Schema); It; ++It)
					if (!It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_EditConst)
						&& (WeakProfile->Target == ESheetImportTarget::DataTable || It->HasAnyPropertyFlags(CPF_Edit))) Names.AddUnique(It->GetAuthoredName());
				return FString::Join(Names, TEXT(","));
			};
			Help.AddCustomRow(FText()).WholeRowContent()[SNew(STextBlock).Text(NSLOCTEXT("SheetAssetImporter", "ColumnHelp", "Name / ID / ItemId 또는 첫 번째 열이 식별자입니다. Auto Map Columns를 켜면 명시한 매핑을 우선하고 나머지 같은 이름의 열을 자동 연결합니다. 배열·구조체·맵의 JSON을 자동 인식하며, 레시피 Result에는 아이템 ID 하나만 입력할 수도 있습니다.")).AutoWrapText(true)];
			Help.AddCustomRow(FText()).WholeRowContent()[SNew(STextBlock).Text_Lambda([ColumnNames]() { return FText::FromString(ColumnNames()); }).AutoWrapText(true)];
			Help.AddCustomRow(FText()).WholeRowContent()[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "CopyHeaders", "열 이름 복사"))
				.OnClicked_Lambda([ColumnNames]() { FPlatformApplicationMisc::ClipboardCopy(*ColumnNames()); return FReply::Handled(); })];

			IDetailCategoryBuilder& Advanced = Builder.EditCategory(TEXT("SheetAdvanced"), NSLOCTEXT("SheetAssetImporter", "Advanced", "고급 설정"), ECategoryPriority::Uncommon);
			Advanced.InitiallyCollapsed(true);
			for (const FName Name : {
				GET_MEMBER_NAME_CHECKED(USheetImportProfile, Source), GET_MEMBER_NAME_CHECKED(USheetImportProfile, SpreadsheetId),
				GET_MEMBER_NAME_CHECKED(USheetImportProfile, SheetGid), GET_MEMBER_NAME_CHECKED(USheetImportProfile, CsvRange),
				GET_MEMBER_NAME_CHECKED(USheetImportProfile, SheetRange), GET_MEMBER_NAME_CHECKED(USheetImportProfile, AccessTokenEnvironmentVariable),
				GET_MEMBER_NAME_CHECKED(USheetImportProfile, KeyColumn), GET_MEMBER_NAME_CHECKED(USheetImportProfile, ColumnMappings),
				GET_MEMBER_NAME_CHECKED(USheetImportProfile, bAutoMapColumns), GET_MEMBER_NAME_CHECKED(USheetImportProfile, bPreserveUnmappedValues),
				GET_MEMBER_NAME_CHECKED(USheetImportProfile, bRemoveMissingTableRows), GET_MEMBER_NAME_CHECKED(USheetImportProfile, bValidateAssets),
				GET_MEMBER_NAME_CHECKED(USheetImportProfile, bValidateReferences), GET_MEMBER_NAME_CHECKED(USheetImportProfile, ImportOrder) })
				Advanced.AddProperty(Builder.GetProperty(Name), EPropertyLocation::Common);
		}
	};
}

TSharedRef<IDetailCustomization> SheetImportEditor::MakeProfileDetails()
{
	return MakeShared<FSheetImportProfileDetails>();
}
