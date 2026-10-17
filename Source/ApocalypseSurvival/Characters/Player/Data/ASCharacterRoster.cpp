#include "ASCharacterRoster.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "UASCharacterRoster"

const UASCharacterDefinition* UASCharacterRoster::GetCharacterAt(int32 Index) const
{
	return Characters.IsValidIndex(Index) ? Characters[Index].Get() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UASCharacterRoster::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	
	TSet<const UASCharacterDefinition*> Seen;
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		const UASCharacterDefinition* Definition = Characters[Index].Get();
		if (!Definition)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyEntry", "Characters[{0}]가 비어 있다."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		
		if (Seen.Contains(Definition))
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateEntry", "Characters[{0}] {1} 가 중복이다."), FText::AsNumber(Index), FText::FromString(Definition->GetName())));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		Seen.Add(Definition);
	}
	
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE