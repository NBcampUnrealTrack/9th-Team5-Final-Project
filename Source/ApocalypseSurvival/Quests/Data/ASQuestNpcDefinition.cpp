#include "Quests/Data/ASQuestNpcDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UASQuestNpcDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	if (NpcId.IsNone() || DisplayName.IsEmpty() || IdleDialogue.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("Quests", "InvalidNpc", "NPC ID, display name and idle dialogue are required."));
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
