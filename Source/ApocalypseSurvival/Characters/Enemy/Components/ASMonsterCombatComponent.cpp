#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"

#include "AbilitySystemInterface.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GAS/ASAbilitySystemComponent.h"

namespace
{
	float GetCapsuleRadius(const AActor* Actor)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		return Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
	}
}

UASMonsterCombatComponent::UASMonsterCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UASMonsterCombatComponent::Configure(float InAttackRange, FGameplayTag InAttackAbilityTag)
{
	AttackRange = FMath::Max(0.0f, InAttackRange);
	AttackAbilityTag = InAttackAbilityTag;
}

bool UASMonsterCombatComponent::IsTargetInAttackRange(const AActor* Target) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(Target))
	{
		return false;
	}

	const float CenterDistance = FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation());
	const float SurfaceDistance = CenterDistance - GetCapsuleRadius(Owner) - GetCapsuleRadius(Target);
	return SurfaceDistance <= AttackRange;
}

bool UASMonsterCombatComponent::TryRequestAttack(AActor* Target)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !AttackAbilityTag.IsValid() || !IsTargetInAttackRange(Target))
	{
		return false;
	}

	UASAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent)
		|| AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_HitReact))
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AttackAbilityTag);
	return AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags, false);
}

UASAbilitySystemComponent* UASMonsterCombatComponent::GetOwnerAbilitySystemComponent() const
{
	if (const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		return Cast<UASAbilitySystemComponent>(AbilitySystemOwner->GetAbilitySystemComponent());
	}

	return nullptr;
}
