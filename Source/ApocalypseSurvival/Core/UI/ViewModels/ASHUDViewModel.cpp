#include "Core/UI/ViewModels/ASHUDViewModel.h"

#include "Core/UI/ViewModels/ASPlayerStatusViewModel.h"
#include "Core/UI/ViewModels/ASSkillBarViewModel.h"

void UASHUDViewModel::Initialize()
{
	if (!PlayerStatusViewModel)
	{
		UE_MVVM_SET_PROPERTY_VALUE(PlayerStatusViewModel, NewObject<UASPlayerStatusViewModel>(this));
	}

	if (!SkillBarViewModel)
	{
		UE_MVVM_SET_PROPERTY_VALUE(SkillBarViewModel, NewObject<UASSkillBarViewModel>(this));
	}

	SkillBarViewModel->InitializeSlots();
}
