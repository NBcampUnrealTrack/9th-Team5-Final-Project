#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "SheetImportProfileFactory.generated.h"

/** A direct Add Asset entry, without selecting a DataAsset base class first. */
UCLASS()
class SHEETASSETIMPORTER_API USheetImportProfileFactory : public UFactory
{
	GENERATED_BODY()
public:
	USheetImportProfileFactory();
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;
};
