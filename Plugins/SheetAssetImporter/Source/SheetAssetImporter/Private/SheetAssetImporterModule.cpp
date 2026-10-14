#include "Modules/ModuleManager.h"
#include "SheetAssetImport.h"
#include "SheetImportProfile.h"
#include "SheetImportSubsystem.h"
#include "SheetImportEditor.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "ToolMenus.h"
#include "PropertyEditorModule.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"

DEFINE_LOG_CATEGORY(LogSheetAssetImporter);

namespace
{
	void StartSelected(const bool bPreview)
	{
		if (!GEditor) return;
		TArray<FAssetData> Selected;
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
		USheetImportSubsystem* Importer = GEditor->GetEditorSubsystem<USheetImportSubsystem>();
		if (Selected.Num() != 1 || !Importer)
		{
			UE_LOG(LogSheetAssetImporter, Warning, TEXT("Select one SheetImportProfile or SheetImportPlan asset in the Content Browser."));
			return;
		}
		UObject* Asset = Selected[0].GetAsset();
		bool bStarted = false;
		if (USheetImportProfile* Profile = Cast<USheetImportProfile>(Asset)) bStarted = Importer->StartImport(Profile, bPreview);
		else if (USheetImportPlan* Plan = Cast<USheetImportPlan>(Asset)) bStarted = Importer->StartImportPlan(Plan, bPreview);
		if (!bStarted) UE_LOG(LogSheetAssetImporter, Warning, TEXT("Could not start. Select an import profile/plan, verify its profile references, and wait for any active import to finish."));
	}

	class FSheetImportAssetActions : public FAssetTypeActions_Base
	{
	public:
		explicit FSheetImportAssetActions(UClass* InClass) : Class(InClass) {}
		virtual FText GetName() const override { return Class == USheetImportProfile::StaticClass() ? NSLOCTEXT("SheetAssetImporter", "Profile", "구글 시트 가져오기 설정") : NSLOCTEXT("SheetAssetImporter", "Plan", "시트 가져오기 계획"); }
		virtual FColor GetTypeColor() const override { return FColor(70, 170, 110); }
		virtual UClass* GetSupportedClass() const override { return Class; }
		virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	private:
		UClass* Class;
	};
}

class FSheetAssetImporterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet()) return;
		if (GEditor) EditorCloseHandle = GEditor->OnEditorClose().AddRaw(this, &FSheetAssetImporterModule::HandleEditorClose);
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.RegisterCustomClassLayout(USheetImportProfile::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&SheetImportEditor::MakeProfileDetails));
		PropertyEditor.NotifyCustomizationModuleChanged();
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SheetImportEditor::DashboardTabId,
			FOnSpawnTab::CreateStatic(&SheetImportEditor::SpawnDashboard))
			.SetDisplayName(NSLOCTEXT("SheetAssetImporter", "DashboardTitle", "구글 시트 가져오기"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		OpenCommand = MakeUnique<FAutoConsoleCommand>(TEXT("SheetAssetImporter.Open"), TEXT("Open the sheet import dashboard."),
			FConsoleCommandDelegate::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(SheetImportEditor::DashboardTabId); }));
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		for (UClass* Class : { USheetImportProfile::StaticClass(), USheetImportPlan::StaticClass() })
		{
			TSharedPtr<IAssetTypeActions> Action = MakeShared<FSheetImportAssetActions>(Class);
			AssetTools.RegisterAssetTypeActions(Action.ToSharedRef());
			Actions.Add(Action);
		}
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSheetAssetImporterModule::RegisterMenus));
	}
	virtual void ShutdownModule() override
	{
		OpenCommand.Reset();
		if (GEditor && !IsEngineExitRequested()) GEditor->OnEditorClose().Remove(EditorCloseHandle);
		SheetImportEditor::ClearProfileViews();
		if (!IsRunningCommandlet())
		{
			if (const auto Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(SheetImportEditor::DashboardTabId)) Tab->RequestCloseTab();
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SheetImportEditor::DashboardTabId);
			if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
				FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).UnregisterCustomClassLayout(USheetImportProfile::StaticClass()->GetFName());
		}
		if (GEditor && !IsEngineExitRequested())
			if (USheetImportSubsystem* Importer = GEditor->GetEditorSubsystem<USheetImportSubsystem>()) Importer->CancelImport();
		if (UToolMenus::IsToolMenuUIEnabled())
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(this);
		}
		if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
			for (const auto& Action : Actions) AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
		Actions.Reset();
	}
private:
	void HandleEditorClose()
	{
		SheetImportEditor::ClearProfileViews();
		if (const auto Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(SheetImportEditor::DashboardTabId)) Tab->RequestCloseTab();
		if (GEditor)
			if (USheetImportSubsystem* Importer = GEditor->GetEditorSubsystem<USheetImportSubsystem>()) Importer->CancelImport();
	}
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("SheetAssetImporter"));
		const FUIAction OpenAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(SheetImportEditor::DashboardTabId); }));
		Section.AddMenuEntry(TEXT("OpenSheetDashboard"), NSLOCTEXT("SheetAssetImporter", "OpenSheets", "시트 가져오기"),
			NSLOCTEXT("SheetAssetImporter", "OpenSheetsTip", "시트 설정을 만들고 개별 또는 일괄로 가져옵니다."), FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Import")), OpenAction);
		UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"))->FindOrAddSection(TEXT("PluginTools"))
			.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("OpenSheetDashboard"), OpenAction,
				NSLOCTEXT("SheetAssetImporter", "Toolbar", "시트 가져오기"),
				NSLOCTEXT("SheetAssetImporter", "ToolbarTip", "구글 시트 가져오기 대시보드"), FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Import"))));
		Section.AddSubMenu(TEXT("SheetAssetImporter"), NSLOCTEXT("SheetAssetImporter", "Menu", "고급: 선택 프로필 / 계획"),
			NSLOCTEXT("SheetAssetImporter", "MenuTip", "Import the selected sheet profile or ordered plan."),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				FToolMenuSection& Entries = SubMenu->AddSection(TEXT("Actions"));
				Entries.AddMenuEntry(TEXT("Preview"), NSLOCTEXT("SheetAssetImporter", "Preview", "Preview Selected Profile / Plan"),
					NSLOCTEXT("SheetAssetImporter", "PreviewTip", "Download and validate without changing destination assets."), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([] { StartSelected(true); })));
				Entries.AddMenuEntry(TEXT("Import"), NSLOCTEXT("SheetAssetImporter", "Import", "Import Selected Profile / Plan"),
					NSLOCTEXT("SheetAssetImporter", "ImportTip", "Create/update destination assets after validation."), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([] { StartSelected(false); })));
				Entries.AddMenuEntry(TEXT("Cancel"), NSLOCTEXT("SheetAssetImporter", "Cancel", "Cancel Import"), FText(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]
					{
						if (GEditor) GEditor->GetEditorSubsystem<USheetImportSubsystem>()->CancelImport();
					})));
			}));
	}
	TArray<TSharedPtr<IAssetTypeActions>> Actions;
	TUniquePtr<FAutoConsoleCommand> OpenCommand;
	FDelegateHandle EditorCloseHandle;
};

IMPLEMENT_MODULE(FSheetAssetImporterModule, SheetAssetImporter)
