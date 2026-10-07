#include "Core/UI/ViewModels/ASSkillBarViewModel.h"

#include "Core/UI/ViewModels/ASSkillSlotViewModel.h"

void UASSkillBarViewModel::InitializeSlots()
{
	if (QSlot && WSlot && ESlot && RSlot)
	{
		return;
	}

	UASSkillSlotViewModel* NewQSlot = NewObject<UASSkillSlotViewModel>(this);
	UASSkillSlotViewModel* NewWSlot = NewObject<UASSkillSlotViewModel>(this);
	UASSkillSlotViewModel* NewESlot = NewObject<UASSkillSlotViewModel>(this);
	UASSkillSlotViewModel* NewRSlot = NewObject<UASSkillSlotViewModel>(this);

	NewQSlot->InitializeSlot(FText::FromString(TEXT("Q")));
	NewWSlot->InitializeSlot(FText::FromString(TEXT("W")));
	NewESlot->InitializeSlot(FText::FromString(TEXT("E")));
	NewRSlot->InitializeSlot(FText::FromString(TEXT("R")));

	UE_MVVM_SET_PROPERTY_VALUE(QSlot, NewQSlot);
	UE_MVVM_SET_PROPERTY_VALUE(WSlot, NewWSlot);
	UE_MVVM_SET_PROPERTY_VALUE(ESlot, NewESlot);
	UE_MVVM_SET_PROPERTY_VALUE(RSlot, NewRSlot);
}
