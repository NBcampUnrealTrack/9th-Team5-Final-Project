#include "SheetImportEditor.h"

#include "SheetImportProfile.h"
#include "SheetImportProfileFactory.h"
#include "SheetImportSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

const FName SheetImportEditor::DashboardTabId(TEXT("SheetAssetImporterDashboard"));

namespace
{
	struct FProfileItem
	{
		TStrongObjectPtr<USheetImportProfile> Profile;
		bool bChecked = false;
	};

	class SSheetImportDashboard : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSheetImportDashboard) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
			FDetailsViewArgs DetailsArgs;
			DetailsArgs.bAllowSearch = false;
			DetailsArgs.bHideSelectionTip = true;
			DetailsArgs.bUpdatesFromSelection = false;
			DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(12)
				[SNew(STextBlock).Text(NSLOCTEXT("SheetAssetImporter", "DashboardHelp", "설정을 만들고 시트 링크와 생성할 타입을 선택하세요. 참조 에셋이 있으면 먼저 가져오도록 순서를 지정하세요.")).AutoWrapText(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "CreateProfile", "새 설정"))
						.OnClicked(this, &SSheetImportDashboard::CreateProfile)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
					[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "Refresh", "목록 새로고침"))
						.OnClicked_Lambda([this]() { Refresh(); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "ValidateChecked", "선택 검증"))
						.IsEnabled(this, &SSheetImportDashboard::CanRun).OnClicked_Lambda([this]() { RunBatch(false, true); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "ImportChecked", "선택 가져오기"))
						.IsEnabled(this, &SSheetImportDashboard::CanRun).OnClicked_Lambda([this]() { RunBatch(false, false); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "ImportAll", "전체 가져오기"))
						.IsEnabled(this, &SSheetImportDashboard::CanRun).OnClicked_Lambda([this]() { RunBatch(true, false); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().AutoWidth()
					[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "Cancel", "취소"))
						.IsEnabled_Lambda([this]() { return Service() && Service()->IsImportRunning(); })
						.OnClicked_Lambda([this]() { if (Service()) Service()->CancelImport(); return FReply::Handled(); })]
				]
				+ SVerticalBox::Slot().FillHeight(1).Padding(12, 0)
				[
					SNew(SSplitter)
					+ SSplitter::Slot().Value(0.42f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(4)
						[SNew(STextBlock).Text(NSLOCTEXT("SheetAssetImporter", "ListHeading", "선택   순서   가져오기 설정 / 결과"))]
						+ SVerticalBox::Slot().FillHeight(1)
						[
							SAssignNew(List, SListView<TSharedPtr<FProfileItem>>).ListItemsSource(&Items)
								.SelectionMode(ESelectionMode::Single)
								.OnGenerateRow(this, &SSheetImportDashboard::MakeRow)
								.OnSelectionChanged_Lambda([this](TSharedPtr<FProfileItem> Item, ESelectInfo::Type)
								{
									Details->SetObject(Item ? Item->Profile.Get() : nullptr);
								})
						]
					]
					+ SSplitter::Slot().Value(0.58f)
					[SNew(SBorder).Padding(8)[Details.ToSharedRef()]]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12, 8, 12, 0)
				[SNew(SProgressBar).Percent_Lambda([this]() -> TOptional<float>
				{
					if (!Service() || !Service()->IsImportRunning()) return 0.0f;
					return static_cast<float>(Service()->GetCompletedProfileCount()) / FMath::Max(1, Service()->GetQueuedProfileCount());
				})]
				+ SVerticalBox::Slot().AutoHeight().Padding(12, 4, 12, 12)
				[SNew(STextBlock).Text_Lambda([this]()
				{
					if (!Service()) return FText();
					if (Service()->IsImportRunning())
						return FText::FromString(FString::Printf(TEXT("진행 중 · %d / %d 설정 완료"), Service()->GetCompletedProfileCount(), Service()->GetQueuedProfileCount()));
					const FSheetImportReport Report = Service()->GetLastReport();
					if (Report.Messages.IsEmpty()) return FText::FromString(FString::Printf(TEXT("설정 %d개 · 왼쪽 목록에서 선택하면 오른쪽에서 편집할 수 있습니다."), Items.Num()));
					return FText::FromString(FString::Printf(TEXT("%s · %d행 · 새 에셋 %d / 갱신 %d"), Report.bSuccess ? TEXT("완료") : TEXT("실패 또는 취소"), Report.RowCount, Report.CreatedAssetCount, Report.UpdatedAssetCount));
				}).AutoWrapText(true)]
			];
			Refresh();
		}

	private:
		USheetImportSubsystem* Service() const { return GEditor && !IsEngineExitRequested() ? GEditor->GetEditorSubsystem<USheetImportSubsystem>() : nullptr; }
		bool CanRun() const { return Service() && !Service()->IsImportRunning() && !Items.IsEmpty(); }
		void Sort()
		{
			Items.Sort([](const TSharedPtr<FProfileItem>& A, const TSharedPtr<FProfileItem>& B)
			{
				return A->Profile->ImportOrder != B->Profile->ImportOrder ? A->Profile->ImportOrder < B->Profile->ImportOrder
					: A->Profile->GetPathName() < B->Profile->GetPathName();
			});
		}
		void Refresh()
		{
			TSet<FString> Checked;
			for (const auto& Item : Items) if (Item->bChecked) Checked.Add(Item->Profile->GetPathName());
			Items.Reset();
			FARFilter Filter;
			Filter.ClassPaths.Add(USheetImportProfile::StaticClass()->GetClassPathName());
			Filter.bRecursiveClasses = true;
			TArray<FAssetData> Found;
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Found);
			for (const FAssetData& Asset : Found)
			{
				if (USheetImportProfile* Profile = Cast<USheetImportProfile>(Asset.GetAsset()))
				{
					TSharedPtr<FProfileItem> Item = MakeShared<FProfileItem>();
					Item->Profile = TStrongObjectPtr<USheetImportProfile>(Profile);
					Item->bChecked = Checked.Contains(Profile->GetPathName());
					Items.Add(Item);
				}
			}
			Sort();
			List->RequestListRefresh();
		}
		FReply CreateProfile()
		{
			USheetImportProfileFactory* Factory = NewObject<USheetImportProfileFactory>();
			Factory->bEditAfterNew = false;
			UObject* Created = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAssetWithDialog(
				TEXT("SheetImport"), TEXT("/Game/Editor/SheetImports"), USheetImportProfile::StaticClass(), Factory);
			if (Created)
			{
				Refresh();
				for (const auto& Item : Items)
					if (Item->Profile.Get() == Created) { List->SetSelection(Item); break; }
				Details->SetObject(Created);
			}
			return FReply::Handled();
		}
		void RunBatch(const bool bAll, const bool bPreview)
		{
			if (!CanRun()) return;
			Sort();
			USheetImportPlan* Plan = NewObject<USheetImportPlan>();
			for (const auto& Item : Items)
				if (bAll || Item->bChecked) Plan->Profiles.Add(Item->Profile.Get());
			if (!Plan->Profiles.IsEmpty()) Service()->StartImportPlan(Plan, bPreview);
		}
		TSharedRef<ITableRow> MakeRow(TSharedPtr<FProfileItem> Item, const TSharedRef<STableViewBase>& Owner)
		{
			return SNew(STableRow<TSharedPtr<FProfileItem>>, Owner).Padding(6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[SNew(SCheckBox).IsChecked_Lambda([Item]() { return Item->bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([Item](ECheckBoxState State) { Item->bChecked = State == ECheckBoxState::Checked; })]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[SNew(SSpinBox<int32>).MinValue(0).MinDesiredWidth(48).Value_Lambda([Item]() { return Item->Profile->ImportOrder; })
					.OnValueCommitted_Lambda([this, Item](int32 Value, ETextCommit::Type)
					{
						const FScopedTransaction Transaction(NSLOCTEXT("SheetAssetImporter", "OrderTransaction", "Change sheet import order"));
						Item->Profile->Modify();
						Item->Profile->ImportOrder = FMath::Max(0, Value);
						Item->Profile->MarkPackageDirty();
						Sort();
						List->RequestListRefresh();
					})]
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Text(FText::FromString(Item->Profile->GetName())).ToolTipText(FText::FromString(Item->Profile->GetPathName()))]
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Text_Lambda([Item]() { return SheetImportEditor::Status(Item->Profile.Get()); }).AutoWrapText(true)
						.ToolTipText_Lambda([Item]() { return SheetImportEditor::Result(Item->Profile.Get()); })]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
				[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "ImportRow", "가져오기"))
					.IsEnabled(this, &SSheetImportDashboard::CanRun)
					.OnClicked_Lambda([Item]() { SheetImportEditor::Start(Item->Profile.Get(), false); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
				[SNew(SButton).Text(NSLOCTEXT("SheetAssetImporter", "LocateRow", "찾기"))
					.OnClicked_Lambda([Item]()
					{
						TArray<UObject*> Assets { Item->Profile.Get() };
						FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets(Assets);
						return FReply::Handled();
					})]
			];
		}
		TArray<TSharedPtr<FProfileItem>> Items;
		TSharedPtr<SListView<TSharedPtr<FProfileItem>>> List;
		TSharedPtr<IDetailsView> Details;
	};
}

TSharedRef<SDockTab> SheetImportEditor::SpawnDashboard(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SSheetImportDashboard)];
}
