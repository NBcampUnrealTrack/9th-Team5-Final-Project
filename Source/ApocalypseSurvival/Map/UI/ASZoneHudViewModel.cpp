#include "Map/UI/ASZoneHudViewModel.h"

#define LOCTEXT_NAMESPACE "ASZoneHudViewModel"

const FName UASZoneHudViewModel::GlobalName(TEXT("ZoneHud"));

void UASZoneHudViewModel::SetInfo(const FASZoneHudInfo& InInfo)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(Info, InInfo))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetZoneNameText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDangerText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDangerTextColor);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDangerBackgroundColor);
	}
}

FText UASZoneHudViewModel::GetZoneNameText() const
{
	return Info.bInZone ? Info.DisplayName : LOCTEXT("NoZone", "-");
}

FText UASZoneHudViewModel::GetDangerText() const
{
	return Info.bInZone
		? FText::Format(LOCTEXT("Danger", "Danger {0}"), FText::AsNumber(Info.DangerLevel))
		: FText::GetEmpty();
}

FSlateColor UASZoneHudViewModel::GetDangerTextColor() const
{
	return FSlateColor(Info.DangerColor);
}

FLinearColor UASZoneHudViewModel::GetDangerBackgroundColor() const
{
	FLinearColor Tint = Info.DangerColor;
	Tint.A = Info.bInZone ? 0.6f : 0.0f;
	return Tint;
}

#undef LOCTEXT_NAMESPACE
