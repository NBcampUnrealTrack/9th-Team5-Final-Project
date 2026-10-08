#include "Items/GAS/ASItemUseAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/GAS/ASItemEffects.h"
#include "Items/Data/ASItemTags.h"
#include "GAS/ASAbilityTags.h"
#include "GameFramework/Pawn.h"

UASItemUseAbility::UASItemUseAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
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
	if (!bHasBlueprintActivate && Item->GetItemDefinition()->UseEffects.IsEmpty())
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
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	if (!ASC || !Definition)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Load and build every effect before consuming the item.
	TArray<UClass*> Classes;
	for (const TSoftClassPtr<UGameplayEffect>& SoftClass : Definition->UseEffects)
	{
		UClass* EffectClass = SoftClass.LoadSynchronous();
		if (!EffectClass)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		Classes.Add(EffectClass);
	}
	if (Classes.IsEmpty()) Classes.Add(UASItemHealingEffect::StaticClass());
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
		Spec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Item_Heal, FMath::Max(0.0f, Definition->HealAmount));
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
