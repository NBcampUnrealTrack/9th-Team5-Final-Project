#include "SheetImportProfileFactory.h"
#include "SheetImportProfile.h"
#include "AssetTypeCategories.h"

USheetImportProfileFactory::USheetImportProfileFactory()
{
	SupportedClass = USheetImportProfile::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

FText USheetImportProfileFactory::GetDisplayName() const
{
	return NSLOCTEXT("SheetAssetImporter", "NewProfile", "구글 시트 가져오기 설정");
}

uint32 USheetImportProfileFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}

UObject* USheetImportProfileFactory::FactoryCreateNew(UClass* Class, UObject* InParent, const FName Name,
	const EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USheetImportProfile* Profile = NewObject<USheetImportProfile>(InParent, Class, Name, Flags);
	Profile->bSaveAssets = true;
	return Profile;
}
