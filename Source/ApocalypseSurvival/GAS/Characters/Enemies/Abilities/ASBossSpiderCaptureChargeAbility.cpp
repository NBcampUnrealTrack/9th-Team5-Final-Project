#include "GAS/Characters/Enemies/Abilities/ASBossSpiderCaptureChargeAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASBossSpiderDirectorComponent.h"
#include "GAS/ASAbilityTags.h"

AActor* UASBossSpiderCaptureChargeAbility::ResolveChargeTarget() const
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwnerCharacter());
    const UASBossSpiderDirectorComponent* Director = Monster
        ? Monster->FindComponentByClass<UASBossSpiderDirectorComponent>()
        : nullptr;
    return Director ? Director->GetCaptureTarget() : nullptr;
}

bool UASBossSpiderCaptureChargeAbility::IsChargeTargetValid(const AActor* Target) const
{
    if (!Super::IsChargeTargetValid(Target))
    {
        return false;
    }

    const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(Target);
    const UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemOwner
        ? AbilitySystemOwner->GetAbilitySystemComponent()
        : nullptr;
    return AbilitySystemComponent
        && AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_WebBound);
}

