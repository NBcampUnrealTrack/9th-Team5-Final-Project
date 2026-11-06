#include "Modules/UI/ViewModels/ASModuleLoadoutViewModel.h"
#include "Modules/ASModuleComponent.h"
#include "Modules/Data/ASModuleSettings.h"
#include "Core/Framework/ASPlayerController.h"
#include "GameFramework/PlayerState.h"

void UASModuleLoadoutViewModel::Initialize(APlayerController* Controller)
{
	Shutdown(); Player = Controller;
	ObservedController = Cast<AASPlayerController>(Controller);
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	Refresh();
}

void UASModuleLoadoutViewModel::Shutdown()
{
	if (Modules.IsValid()) Modules->OnLoadoutChanged.RemoveDynamic(this, &ThisClass::Refresh);
	if (ObservedController.IsValid()) ObservedController->OnLocalPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	Modules.Reset(); ObservedController.Reset(); Player.Reset();
}

void UASModuleLoadoutViewModel::BeginDestroy() { Shutdown(); Super::BeginDestroy(); }
void UASModuleLoadoutViewModel::HandlePawnChanged(APawn* Pawn) { Refresh(); }

void UASModuleLoadoutViewModel::Refresh()
{
	APlayerState* State = Player.IsValid() ? Player->PlayerState.Get() : nullptr;
	UASModuleComponent* Source = State ? State->FindComponentByClass<UASModuleComponent>() : nullptr;
	if (Modules.Get() != Source)
	{
		if (Modules.IsValid()) Modules->OnLoadoutChanged.RemoveDynamic(this, &ThisClass::Refresh);
		Modules = Source;
		if (Modules.IsValid()) Modules->OnLoadoutChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	}
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	const auto Equipped = Source ? Source->GetEquippedModules() : TArray<FASModuleInstanceData>();
	FASModuleLoadoutPresentation Value;
	for (int32 Index = 0; Index < Settings->GetSlotCount(); ++Index)
	{
		FASModuleSlotPresentation& SlotData = Value.Slots.AddDefaulted_GetRef();
		SlotData.SlotLabel = FText::Format(NSLOCTEXT("Modules", "Slot", "슬롯 {0}"), FText::AsNumber(Index + 1));
		SlotData.bEquipped = Equipped.IsValidIndex(Index) && !Equipped[Index].IsEmpty();
		FString Error;
		const FASModuleBalanceRow* Definition = SlotData.bEquipped ? Settings->FindModule(Equipped[Index].ModuleId, Error) : nullptr;
		SlotData.Name = Definition ? Definition->DisplayName : SlotData.bEquipped ? NSLOCTEXT("Modules", "Unknown", "모듈 정보 없음") : NSLOCTEXT("Modules", "Empty", "장착 없음");
		if (Definition) SlotData.Description = Definition->Description;
		TArray<FText> Options;
		if (SlotData.bEquipped)
			for (const FASModuleOptionRoll& Option : Equipped[Index].Options)
			{
				const FASModuleOptionBalanceRow* Data = Settings->FindOption(Option.OptionId, Error);
				if (Data) Options.Add(FText::Format(Option.Value >= 0.f ? NSLOCTEXT("Modules", "Option", "{0}  +{1}") : NSLOCTEXT("Modules", "NegativeOption", "{0}  {1}"), Data->DisplayName, FText::AsNumber(Option.Value)));
			}
		SlotData.Effects = FText::Join(FText::FromString(TEXT("\n")), Options);
	}
	UE_MVVM_SET_PROPERTY_VALUE(Presentation, Value);
}
