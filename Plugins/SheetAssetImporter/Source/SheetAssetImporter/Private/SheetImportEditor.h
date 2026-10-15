#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Widgets/Docking/SDockTab.h"

class USheetImportProfile;

namespace SheetImportEditor
{
	extern const FName DashboardTabId;
	TSharedRef<SDockTab> SpawnDashboard(const FSpawnTabArgs& Args);
	TSharedRef<IDetailCustomization> MakeProfileDetails();
	bool Start(USheetImportProfile* Profile, bool bPreview);
	bool CanRun();
	void ClearProfileViews();
	FText Status(USheetImportProfile* Profile);
	FText Result(USheetImportProfile* Profile);
}
