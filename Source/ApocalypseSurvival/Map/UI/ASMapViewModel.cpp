#include "Map/UI/ASMapViewModel.h"

const FName UASMapViewModel::GlobalName(TEXT("Map"));

void UASMapViewModel::SetExpandedMapOpen(bool bOpen)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(bExpandedMapOpen, bOpen))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetExpandedMapVisibility);
	}
}

ESlateVisibility UASMapViewModel::GetExpandedMapVisibility() const
{
	return bExpandedMapOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}
