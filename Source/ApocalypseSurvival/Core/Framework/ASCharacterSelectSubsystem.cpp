#include "Core/Framework/ASCharacterSelectSubsystem.h"

#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"

void UASCharacterSelectSubsystem::SetSelectedCharacter(const UASCharacterDefinition* InDefinition)
{
	SelectedCharacter = InDefinition;
	UE_LOG(LogTemp, Log, TEXT("[CharacterSelect] 선택 캐릭터: %s"), *GetNameSafe(InDefinition));
}

void UASCharacterSelectSubsystem::ClearSelectedCharacter()
{
	SelectedCharacter = nullptr;
}

const UASCharacterDefinition* UASCharacterSelectSubsystem::GetSelectedCharacter() const
{
	return SelectedCharacter;
}

bool UASCharacterSelectSubsystem::HasSelectedCharacter() const
{
	return SelectedCharacter != nullptr;
}

TSubclassOf<APawn> UASCharacterSelectSubsystem::GetSelectedPawnClass() const
{
	if (!SelectedCharacter)
	{
		return nullptr;
	}
	return SelectedCharacter->CharacterClass;
}
