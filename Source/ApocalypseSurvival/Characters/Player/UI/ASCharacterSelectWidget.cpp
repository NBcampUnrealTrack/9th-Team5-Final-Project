#include "Characters/Player/UI/ASCharacterSelectWidget.h"

#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Core/Framework/ASCharacterSelectSubsystem.h"
#include "Engine/GameInstance.h"

void UASCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// ESC 를 받으려면 포커스가 있어야 한다
	SetIsFocusable(true);
	SetKeyboardFocus();
	
	if (!Roster || GetCharacterCount() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] %s 에 Roster 가 비어 있다."), *GetName());
		return;
	}
	
	// 열릴 때 이전 선택을 미리 골라 둔다 (없으면 0번)
	SelectCharacter(FindPreviousSelectionIndex());
}

FReply UASCharacterSelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == BackKey)
	{
		RequestBack();
		return FReply::Handled();
	}
	
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UASCharacterSelectWidget::GetCharacterCount() const
{
	return Roster ? Roster->Characters.Num() : 0;
}

bool UASCharacterSelectWidget::GetCharacterInfo(int32 Index, FASCharacterDisplayInfo& OutInfo) const
{
	const UASCharacterDefinition* Definition = GetDefinitionAt(Index);
	if (!Definition)
	{
		return false;
	}
	
	OutInfo.DisplayName = Definition->DisplayName;
	OutInfo.Title = Definition->Title;
	OutInfo.AbilityName = Definition->AbilityName;
	OutInfo.Role = Definition->Role;
	OutInfo.CombatStyle = Definition->CombatStyle;
	OutInfo.DifficultyText = UEnum::GetDisplayValueAsText(Definition->Difficulty);
	OutInfo.RepresentativeWeapons = Definition->RepresentativeWeapons;
	OutInfo.Description = Definition->Description;
	OutInfo.Icon = Definition->Icon;
	OutInfo.SkillDisplays = Definition->SkillDisplays;
	return true;
}

void UASCharacterSelectWidget::SelectCharacter(int32 Index)
{
	FASCharacterDisplayInfo Info;
	if (!GetCharacterInfo(Index, Info))
	{
		return;
	}
	
	SelectedIndex = Index;
	OnSelectionChanged(SelectedIndex, Info);
}

void UASCharacterSelectWidget::RequestConfirm()
{
	if (ConfirmSelection())
	{
		OnConfirmed.Broadcast();
	}
}

void UASCharacterSelectWidget::RequestBack()
{
	OnBackRequested.Broadcast();
}

bool UASCharacterSelectWidget::ConfirmSelection()
{
	const UASCharacterDefinition* Definition = GetDefinitionAt(SelectedIndex);
	UGameInstance* GameInstance = GetGameInstance();
	UASCharacterSelectSubsystem* SelectSubsystem = GameInstance ? GameInstance->GetSubsystem<UASCharacterSelectSubsystem>() : nullptr;
	if (!Definition || !SelectSubsystem)
	{
		return false;
	}
	
	SelectSubsystem->SetSelectedCharacter(Definition);
	return true;
}

const UASCharacterDefinition* UASCharacterSelectWidget::GetDefinitionAt(int32 Index) const
{
	return Roster ? Roster->GetCharacterAt(Index) : nullptr;
}

int32 UASCharacterSelectWidget::FindPreviousSelectionIndex() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UASCharacterSelectSubsystem* SelectSubsystem = GameInstance ? GameInstance->GetSubsystem<UASCharacterSelectSubsystem>() : nullptr;
	const UASCharacterDefinition* Previous = SelectSubsystem ? SelectSubsystem->GetSelectedCharacter() : nullptr;
	if (!Previous || !Roster)
	{
		return 0;
	}
	
	const int32 Found = Roster->Characters.IndexOfByKey(Previous);
	return Found != INDEX_NONE ? Found : 0;
}