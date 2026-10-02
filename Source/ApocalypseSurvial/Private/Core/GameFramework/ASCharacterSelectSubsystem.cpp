#include "Core/GameFramework/ASCharacterSelectSubsystem.h"

void UASCharacterSelectSubsystem::SetSelectedPawnClass(TSubclassOf<APawn> InPawnClass)
{
	SelectedPawnClass = InPawnClass;
}
