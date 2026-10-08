#include "Items/Use/ASItemUseComponent.h"

#include "AbilitySystemComponent.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/GAS/ASItemUseAbility.h"
#include "Items/Inventory/ASInventoryComponent.h"

UASItemUseComponent::UASItemUseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UASItemUseComponent::UseItem(UASInventoryComponent* Inventory, const FGuid InstanceId, APawn* Avatar)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Inventory)
		|| Inventory->GetOwner() != GetOwner()) return false;
	UASItemInstance* Item = Inventory->FindItemById(InstanceId);
	UAbilitySystemComponent* ASC = UASItemAbilityLibrary::ResolveInventoryAbilitySystem(Inventory, Avatar);
	if (!Item || !ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)) return false;
	const UASItemDefinition* Definition = Item->GetItemDefinition();
	if (Definition->ItemType != EASItemType::Consumable || Definition->UseQuantity <= 0
		|| Item->GetQuantity() < Definition->UseQuantity) return false;
	UClass* AbilityClass = Definition->UseAbility.IsNull()
		? UASItemUseAbility::StaticClass() : Definition->UseAbility.LoadSynchronous();
	if (!AbilityClass || AbilityClass->HasAnyClassFlags(CLASS_Abstract)
		|| !AbilityClass->IsChildOf(UASItemUseAbility::StaticClass())) return false;
	// Prevent two delayed uses of the same stack from competing for its last unit.
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.SourceObject.Get() == Item && Spec.IsActive()) return false;
	}
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Item));
	const bool bActivated = ASC->TryActivateAbility(Handle);
	// This also clears a successfully activated ability that already ended synchronously.
	if (bActivated) ASC->SetRemoveAbilityOnEnd(Handle);
	else ASC->ClearAbility(Handle);
	return bActivated;
}
