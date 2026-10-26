#include "Items/GAS/ASItemUseAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "GAS/ASAbilityTags.h"
#include "GameFramework/Pawn.h"

namespace
{
	bool HasMissingResource(const UAbilitySystemComponent& ASC, const EASItemRecoveryResource Resource)
	{
		if (Resource == EASItemRecoveryResource::Health)
		{
			const UASHealthAttributeSet* Attributes = ASC.GetSet<UASHealthAttributeSet>();
			return Attributes && Attributes->GetHealth() > 0.0f
				&& Attributes->GetHealth() < Attributes->GetMaxHealth();
		}
		const UASPlayerAttributeSet* Attributes = ASC.GetSet<UASPlayerAttributeSet>();
		if (!Attributes) return false;
		if (Resource == EASItemRecoveryResource::Mana)
			return Attributes->GetMana() < Attributes->GetMaxMana();
		if (Resource == EASItemRecoveryResource::Stamina)
			return Attributes->GetStamina() < Attributes->GetMaxStamina();
		return false;
	}
}

UASItemUseAbility::UASItemUseAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UASItemUseAbility::RequiresUseEffects() const
{
	return !bHasBlueprintActivate;
}

bool UASItemUseAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTagContainer* OptionalRelevantTags)
{
	if (bItemCostPaid || bApplyingItemCost) return false;
	return Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags) && bItemCostPaid;
}

bool UASItemUseAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) || !ActorInfo || !ActorInfo->IsNetAuthority()) return false;
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UASItemInstance* Item = Spec ? Cast<UASItemInstance>(Spec->SourceObject.Get()) : nullptr;
	const UASInventoryComponent* Inventory = Item ? Item->GetTypedOuter<UASInventoryComponent>() : nullptr;
	if (!IsValid(Item) || !Inventory || Inventory->FindItemById(Item->GetInstanceId()) != Item
		|| UASItemAbilityLibrary::ResolveInventoryAbilitySystem(Inventory, Cast<APawn>(ActorInfo->AvatarActor.Get())) != ASC
		|| ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| Item->GetItemDefinition()->ItemType != EASItemType::Consumable
		|| Item->GetItemDefinition()->UseQuantity <= 0
		|| Item->GetQuantity() < Item->GetItemDefinition()->UseQuantity) return false;
	const UASItemDefinition& Definition = *Item->GetItemDefinition();
	TArray<EASItemRecoveryResource> Resources = Definition.RecoveryResources;
	if (RequiresUseEffects() && Definition.UseEffects.IsEmpty()) return false;
	return Resources.IsEmpty() || Resources.ContainsByPredicate(
		[ASC](const EASItemRecoveryResource Resource) { return HasMissingResource(*ASC, Resource); });
}

void UASItemUseAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || bItemCostPaid || bApplyingItemCost) return;
	TGuardValue<bool> CostGuard(bApplyingItemCost, true);
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	UASItemInstance* Item = Spec ? Cast<UASItemInstance>(Spec->SourceObject.Get()) : nullptr;
	UASInventoryComponent* Inventory = Item ? Item->GetTypedOuter<UASInventoryComponent>() : nullptr;
	bItemCostPaid = Inventory && Inventory->TryConsumeItemInstance(Item, Item->GetItemDefinition()->UseQuantity);
	if (bItemCostPaid) Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

void UASItemUseAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bItemCostPaid = false;
	CacheSourceItem();
	if (bHasBlueprintActivate)
	{
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		return;
	}
	ActivateItemUse(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UASItemUseAbility::ActivateItemUse(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	if (!ASC || !Definition || Definition->UseEffects.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Inventory prepares these asynchronously; direct activations fail without consuming if not ready.
	TArray<UClass*> Classes;
	for (const TSoftClassPtr<UGameplayEffect>& SoftClass : Definition->UseEffects)
	{
		UClass* EffectClass = SoftClass.Get();
		if (!EffectClass)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		Classes.Add(EffectClass);
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(GetSourceItem());
	TArray<FGameplayEffectSpecHandle> Effects;
	for (UClass* EffectClass : Classes)
	{
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
		if (!Spec.IsValid())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		for (const auto& Magnitude : Definition->EffectMagnitudes)
		{
			if (!Magnitude.Key.IsValid() || !FMath::IsFinite(Magnitude.Value))
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
				return;
			}
			Spec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
		}
		Effects.Add(Spec);
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	for (const FGameplayEffectSpecHandle& Spec : Effects) ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
