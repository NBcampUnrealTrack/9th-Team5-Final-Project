#include "GAS/Characters/Enemies/Abilities/ASMonsterChargeAbility.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

UASMonsterChargeAbility::UASMonsterChargeAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	IndicatorClass = AASMonsterAttackIndicatorActor::StaticClass();
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
	HitTargets.Reset();

	if (WindupDuration > 0.0f)
	{
		SpawnChargeIndicator();
		Character->GetWorldTimerManager().SetTimer(WindupTimerHandle, this, &UASMonsterChargeAbility::BeginCharge,
			WindupDuration, false);
		return;
	}

	BeginCharge();
}

void UASMonsterChargeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(WindupTimerHandle);
		Character->GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
	}

	DestroyChargeIndicator();
	EndChargePawnCollisionIgnore();
	HitTargets.Reset();
	ChargeElapsedTime = 0.0f;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterChargeAbility::BeginCharge()
{
	if (!IsActive())
	{
		return;
	}

	DestroyChargeIndicator();
	BeginChargePawnCollisionIgnore();
	ApplyChargeHitDamage();

	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &UASMonsterChargeAbility::TickCharge,
			FMath::Max(0.005f, MovementTickInterval), true);
	}
}

void UASMonsterChargeAbility::BeginChargePawnCollisionIgnore()
{
	ACharacter* Character = GetOwnerCharacter();
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || bIsIgnoringPawnCollision)
	{
		return;
	}

	PreviousPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	bIsIgnoringPawnCollision = true;
}

void UASMonsterChargeAbility::EndChargePawnCollisionIgnore()
{
	ACharacter* Character = GetOwnerCharacter();
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !bIsIgnoringPawnCollision)
	{
		return;
	}

	Capsule->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnCollisionResponse);
	bIsIgnoringPawnCollision = false;
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
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!IsValid(Character) || !World)
	{
		return;
	}

	if (UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>())
	{
		FASCombatTargetQuery Query;
		Query.SearchOrigin = Character->GetActorLocation();
		Query.SearchRadius = FMath::Max(1.0f, HitRadius);
		Query.Instigator = Character;
		Query.bCanHitPlayers = bCanHitPlayers;
		const FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
		CombatTargets->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1), &HitTargets);
	}
}

void UASMonsterChargeAbility::SpawnChargeIndicator()
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!IsValid(Character) || !World || !IndicatorClass)
	{
		return;
	}

	const FVector IndicatorLocation = Character->GetActorLocation()
		+ ChargeDirection * (FMath::Max(0.0f, ChargeDistance) * 0.5f);
	const FTransform SpawnTransform(ChargeDirection.Rotation(), IndicatorLocation);
	AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
		IndicatorClass, SpawnTransform, Character, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Indicator)
	{
		return;
	}

	Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Rectangle, 0.0f, ChargeDistance,
		IndicatorWidth, WindupDuration);
	Indicator->FinishSpawning(SpawnTransform);
	ActiveIndicator = Indicator;
}

void UASMonsterChargeAbility::DestroyChargeIndicator()
{
	if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
	{
		Indicator->Destroy();
	}

	ActiveIndicator.Reset();
}
