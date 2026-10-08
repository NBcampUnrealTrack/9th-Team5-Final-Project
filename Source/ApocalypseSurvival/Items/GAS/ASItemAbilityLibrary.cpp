#include "Items/GAS/ASItemAbilityLibrary.h"

#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/GAS/ASItemEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Skills/ASSkillTags.h"

UASItemInstance* UASItemAbilityLibrary::GetItemFromSourceObject(UObject* SourceObject)
{
	return IsValid(SourceObject) ? Cast<UASItemInstance>(SourceObject) : nullptr;
}

UASItemDefinition* UASItemAbilityLibrary::GetItemDefinitionFromSourceObject(UObject* SourceObject)
{
	const UASItemInstance* Item = GetItemFromSourceObject(SourceObject);
	UASItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	return IsValid(Definition) ? Definition : nullptr;
}

UASItemDefinition* UASItemAbilityLibrary::GetWeaponDefinitionFromSourceObject(UObject* SourceObject)
{
	UASItemDefinition* Definition = GetItemDefinitionFromSourceObject(SourceObject);
	return Definition && Definition->ItemType == EASItemType::Weapon ? Definition : nullptr;
}

UASInventoryComponent* UASItemAbilityLibrary::GetInventoryFromSourceObject(UObject* SourceObject)
{
	UASItemInstance* Item = GetItemFromSourceObject(SourceObject);
	return Item ? Item->GetTypedOuter<UASInventoryComponent>() : nullptr;
}

UAbilitySystemComponent* UASItemAbilityLibrary::ResolveInventoryAbilitySystem(const UASInventoryComponent* Inventory, APawn* Avatar)
{
	if (!IsValid(Inventory) || !IsValid(Avatar)
		|| (Inventory->GetOwner() != Avatar && Inventory->GetOwner() != Avatar->GetController()
			&& Inventory->GetOwner() != Avatar->GetPlayerState())) return nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Avatar, true);
	if (!ASC && Avatar->GetPlayerState()) ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Avatar->GetPlayerState(), true);
	return IsValid(ASC) && ASC->GetAvatarActor() == Avatar ? ASC : nullptr;
}

bool UASItemAbilityLibrary::CanCommitWeaponAttack(UObject* SourceObject, APawn* Avatar)
{
	UASInventoryComponent* Inventory = GetInventoryFromSourceObject(SourceObject);
	return Inventory && Inventory->CanCommitWeaponAttack(GetItemFromSourceObject(SourceObject), Avatar);
}

bool UASItemAbilityLibrary::TryCommitWeaponAttack(UObject* SourceObject, APawn* Avatar)
{
	UASInventoryComponent* Inventory = GetInventoryFromSourceObject(SourceObject);
	return Inventory && Inventory->TryCommitWeaponAttack(GetItemFromSourceObject(SourceObject), Avatar);
}

TArray<FGameplayEffectSpecHandle> UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(
	UObject* SourceObject, UAbilitySystemComponent* SourceASC, const float AdditionalDamage)
{
	TArray<FGameplayEffectSpecHandle> Result;
	const UASItemDefinition* Definition = GetWeaponDefinitionFromSourceObject(SourceObject);
	UASInventoryComponent* Inventory = GetInventoryFromSourceObject(SourceObject);
	if (!Definition || !Inventory || !IsValid(SourceASC)
		|| !IsValid(SourceASC->GetOwnerActor()) || !SourceASC->GetOwnerActor()->HasAuthority()
		|| ResolveInventoryAbilitySystem(Inventory, Cast<APawn>(SourceASC->GetAvatarActor())) != SourceASC) return Result;
	UClass* DamageClass = Definition->DamageEffectClass.IsNull()
		? UASItemDamageEffect::StaticClass() : Definition->DamageEffectClass.LoadSynchronous();
	if (!DamageClass) return Result;
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(SourceObject);
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageClass, 1.0f, Context);
	if (!Spec.IsValid()) return Result;
	Spec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage,
		-FMath::Max(0.0f, Definition->BaseDamage + AdditionalDamage));
	Result.Add(Spec);
	return Result;
}
