#include "Items/GAS/ASItemUseAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"

UASItemUseAbility::UASItemUseAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
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
		|| Item->GetItemDefinition()->ItemType != EASItemType::Consumable) return false;
	if (!bHasBlueprintActivate)
	{
		const UASHealthAttributeSet* Attributes = ASC->GetSet<UASHealthAttributeSet>();
		if (!Attributes || Item->GetItemDefinition()->HealAmount <= 0.0f
			|| Attributes->GetHealth() <= 0.0f || Attributes->GetHealth() >= Attributes->GetMaxHealth()) return false;
	}
	return true;
}

void UASItemUseAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || bItemCostPaid) return;
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	UASItemInstance* Item = Spec ? Cast<UASItemInstance>(Spec->SourceObject.Get()) : nullptr;
	UASInventoryComponent* Inventory = Item ? Item->GetTypedOuter<UASInventoryComponent>() : nullptr;
	bItemCostPaid = Inventory && Inventory->RemoveItemInstance(Item, 1) == 1;
}

void UASItemUseAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bItemCostPaid = false;
	if (bHasBlueprintActivate)
	{
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		return;
	}
	const float HealAmount = GetItemHealAmount();
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UASHealthAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(HealAmount);
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(GetSourceItem());
	ASC->ApplyGameplayEffectSpecToSelf(FGameplayEffectSpec(Effect, Context, 1.0f));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
