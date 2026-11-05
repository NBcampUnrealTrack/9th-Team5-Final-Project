#include "Relics/ASRelicComponent.h"

#include "Core/Framework/ASPlayerState.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

UASRelicComponent::UASRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASRelicComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UASRelicComponent, EquippedRelics, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UASRelicComponent, SlotCount, COND_OwnerOnly);
}

UASAbilitySystemComponent* UASRelicComponent::GetRelicASC() const
{
	const AASPlayerState* PlayerState = Cast<AASPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetASAbilitySystemComponent() : nullptr;
}

bool UASRelicComponent::EquipRelic(UASRelicDefinition* Definition, FString& OutError)
{
	OutError.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutError = TEXT("Relics can only be equipped by the server.");
		return false;
	}
	if (!IsValid(Definition) || Definition->RelicId.IsNone())
	{
		OutError = TEXT("The relic definition is invalid or has no RelicId.");
		return false;
	}

	UASAbilitySystemComponent* ASC = GetRelicASC();
	if (!ASC)
	{
		OutError = TEXT("The owning PlayerState has no Ability System Component.");
		return false;
	}
	if (EquippedRelics.Contains(Definition) || EquippedRelics.ContainsByPredicate(
		[Definition](const TObjectPtr<UASRelicDefinition>& Existing)
		{
			return Existing && Existing->RelicId == Definition->RelicId;
		}))
	{
		OutError = TEXT("This relic is already equipped.");
		return false;
	}
	if (EquippedRelics.Num() >= SlotCount)
	{
		OutError = TEXT("There are no free relic slots.");
		return false;
	}
	const UASAbilitySet* AbilitySet = Definition->AbilitySet;
	if (!AbilitySet || (AbilitySet->GetGrantedAbilities().IsEmpty() && AbilitySet->GetGrantedEffects().IsEmpty()))
	{
		OutError = TEXT("The relic has no GameplayEffects or GameplayAbilities configured in its AbilitySet.");
		return false;
	}

	FASAppliedRelic& Applied = AppliedRelics.AddDefaulted_GetRef();
	Applied.Definition = Definition;
	AbilitySet->GiveToAbilitySystem(ASC, &Applied.GrantedHandles, this);
	if (Applied.GrantedHandles.IsEmpty())
	{
		OutError = FString::Printf(TEXT("AbilitySet for relic %s did not grant any GAS entries."), *Definition->RelicId.ToString());
		AppliedRelics.Pop();
		return false;
	}

	EquippedRelics.Add(Definition);
	GetOwner()->ForceNetUpdate();
	OnRelicLoadoutChanged.Broadcast();
	return true;
}

bool UASRelicComponent::UnequipRelic(const FName RelicId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || RelicId.IsNone()) return false;
	const int32 Index = EquippedRelics.IndexOfByPredicate(
		[RelicId](const TObjectPtr<UASRelicDefinition>& Definition)
		{
			return Definition && Definition->RelicId == RelicId;
		});
	if (Index == INDEX_NONE) return false;

	RemoveAppliedRelic(RelicId);
	EquippedRelics.RemoveAt(Index);
	GetOwner()->ForceNetUpdate();
	OnRelicLoadoutChanged.Broadcast();
	return true;
}

bool UASRelicComponent::SetSlotCount(const int32 NewSlotCount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| NewSlotCount < DefaultSlotCount || NewSlotCount > MaximumSlotCount
		|| NewSlotCount < EquippedRelics.Num()) return false;
	if (SlotCount == NewSlotCount) return true;

	SlotCount = NewSlotCount;
	GetOwner()->ForceNetUpdate();
	OnRelicLoadoutChanged.Broadcast();
	return true;
}

void UASRelicComponent::RemoveAppliedRelic(const FName RelicId)
{
	UASAbilitySystemComponent* ASC = GetRelicASC();
	for (int32 Index = AppliedRelics.Num() - 1; Index >= 0; --Index)
	{
		FASAppliedRelic& Applied = AppliedRelics[Index];
		if (!Applied.Definition || Applied.Definition->RelicId != RelicId) continue;
		if (ASC) Applied.GrantedHandles.TakeFromAbilitySystem(ASC);
		AppliedRelics.RemoveAt(Index);
		return;
	}
}

void UASRelicComponent::ResetForNewMatch()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	if (UASAbilitySystemComponent* ASC = GetRelicASC())
	{
		for (FASAppliedRelic& Applied : AppliedRelics)
		{
			Applied.GrantedHandles.TakeFromAbilitySystem(ASC);
		}
	}
	AppliedRelics.Reset();
	EquippedRelics.Reset();
	SlotCount = DefaultSlotCount;
	GetOwner()->ForceNetUpdate();
	OnRelicLoadoutChanged.Broadcast();
}

TArray<UASRelicDefinition*> UASRelicComponent::GetEquippedRelics() const
{
	TArray<UASRelicDefinition*> Result;
	Result.Reserve(EquippedRelics.Num());
	for (const TObjectPtr<UASRelicDefinition>& Definition : EquippedRelics)
	{
		Result.Add(Definition.Get());
	}
	return Result;
}

void UASRelicComponent::OnRep_EquippedRelics()
{
	OnRelicLoadoutChanged.Broadcast();
}
