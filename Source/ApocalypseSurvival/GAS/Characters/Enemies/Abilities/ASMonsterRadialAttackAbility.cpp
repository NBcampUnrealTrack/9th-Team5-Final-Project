#include "GAS/Characters/Enemies/Abilities/ASMonsterRadialAttackAbility.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UASMonsterRadialAttackAbility::UASMonsterRadialAttackAbility()
{
	Radius = 320.0f;
	ForwardOffset = 0.0f;
	Angle = 360.0f;
	IndicatorClass = AASMonsterAttackIndicatorActor::StaticClass();
	AbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack_Radial);
}

void UASMonsterRadialAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!IsValid(Character) || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 시전 중 원이 실제 공격 위치에서 벗어나지 않도록 즉시 이동을 멈춘다.
	Character->GetCharacterMovement()->StopMovementImmediately();

	if (WindupDuration <= 0.0f)
	{
		BeginRadialAttack();
		return;
	}

	if (IndicatorClass)
	{
		const FTransform SpawnTransform(Character->GetActorRotation(), Character->GetActorLocation());
		AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
			IndicatorClass, SpawnTransform, Character, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Indicator)
		{
			Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, Radius, 0.0f, 0.0f, WindupDuration);
			Indicator->FinishSpawning(SpawnTransform);
			ActiveIndicator = Indicator;
		}
	}

	World->GetTimerManager().SetTimer(WindupTimerHandle, this, &UASMonsterRadialAttackAbility::BeginRadialAttack,
		WindupDuration, false);
}

void UASMonsterRadialAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(WindupTimerHandle);
	}

	DestroyIndicator();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterRadialAttackAbility::BeginRadialAttack()
{
	if (!IsActive())
	{
		return;
	}

	DestroyIndicator();
	OnSkillEffect();

	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UASMonsterRadialAttackAbility::DestroyIndicator()
{
	if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
	{
		Indicator->Destroy();
	}

	ActiveIndicator.Reset();
}
