#include "Core/Framework/ASCharacterSelectSubsystem.h"

void UASCharacterSelectSubsystem::SetSelectedPawnClass(TSubclassOf<APawn> InPawnClass)
{
	SelectedPawnClass = InPawnClass;
}
