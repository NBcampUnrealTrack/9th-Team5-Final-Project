#include "GAS/Characters/Enemies/Abilities/ASMonsterChargeAbility.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UASMonsterChargeAbility::UASMonsterChargeAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack_Charge);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Charging);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_HitReact);
}

FVector UASMonsterChargeAbility::GetAimDirection_Implementation() const
{
	const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetAvatarActorFromActorInfo());
	const UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* Target = Targeting ? Targeting->GetTargetActor() : nullptr;
	if (Monster && IsValid(Target))
	{
		const FVector ToTarget = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			return ToTarget;
		}
	}

	return Super::GetAimDirection_Implementation();
}

void UASMonsterChargeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!HasSkillAuthority() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character) || !Character->GetWorld())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ChargeDirection = GetAimDirection().GetSafeNormal2D();
	if (ChargeDirection.IsNearlyZero())
	{
		FinishCharge(true);
		return;
	}

	Character->SetActorRotation(ChargeDirection.Rotation());
	Character->GetCharacterMovement()->StopMovementImmediately();
	ChargeElapsedTime = 0.0f;
	HitActors.Reset();

	ApplyChargeHitDamage();
	Character->GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &UASMonsterChargeAbility::TickCharge,
		FMath::Max(0.005f, MovementTickInterval), true);
}

void UASMonsterChargeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
	}

	HitActors.Reset();
	ChargeElapsedTime = 0.0f;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterChargeAbility::TickCharge()
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!IsValid(Character) || !World)
	{
		FinishCharge(true);
		return;
	}

	const float SafeDuration = FMath::Max(0.01f, ChargeDuration);
	const float TickInterval = FMath::Max(0.005f, MovementTickInterval);
	const float RemainingTime = FMath::Max(0.0f, SafeDuration - ChargeElapsedTime);
	const float StepTime = FMath::Min(TickInterval, RemainingTime);
	const float Speed = FMath::Max(0.0f, ChargeDistance) / SafeDuration;

	FHitResult SweepHit;
	const FVector NextLocation = Character->GetActorLocation() + ChargeDirection * Speed * StepTime;
	const bool bMoved = Character->SetActorLocation(NextLocation, true, &SweepHit, ETeleportType::None);
	ChargeElapsedTime += StepTime;

	ApplyChargeHitDamage();

	if (ChargeElapsedTime >= SafeDuration || (!bMoved && bStopOnBlockingHit))
	{
		FinishCharge(false);
	}
}

void UASMonsterChargeAbility::FinishCharge(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UASMonsterChargeAbility::ApplyChargeHitDamage()
{
	ACharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character))
	{
		return;
	}

	for (AActor* Target : FindTargetsInRadius(Character->GetActorLocation(), FMath::Max(1.0f, HitRadius)))
	{
		if (!HitActors.Contains(Target))
		{
			HitActors.Add(Target);
			ApplySkillDamage(Target);
		}
	}
}
