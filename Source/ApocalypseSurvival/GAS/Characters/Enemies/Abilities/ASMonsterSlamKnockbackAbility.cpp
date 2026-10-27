#include "GAS/Characters/Enemies/Abilities/ASMonsterSlamKnockbackAbility.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

UASMonsterSlamKnockbackAbility::UASMonsterSlamKnockbackAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	ImpactPreset = EASImpactPreset::Knockdown;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	IndicatorClass = AASMonsterAttackIndicatorActor::StaticClass();
	CompletionEventTag = ASGameplayTags::Event_Monster_ComboStep_SlamFinished;

	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_Slam);
	SetAssetTags(AssetTagsToSet);
}

void UASMonsterSlamKnockbackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!HasSkillAuthority() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishComboStep(true);
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		FinishComboStep(true);
		return;
	}

	Character->GetCharacterMovement()->StopMovementImmediately();
	SpawnIndicator();
	if (SlamWindupDuration <= 0.0f)
	{
		ExecuteSlam();
		return;
	}

	World->GetTimerManager().SetTimer(
		SlamWindupTimerHandle, this, &ThisClass::ExecuteSlam, SlamWindupDuration, false);
}

void UASMonsterSlamKnockbackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(SlamWindupTimerHandle);
	}

	DestroyIndicator();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterSlamKnockbackAbility::ExecuteSlam()
{
	if (!IsActive())
	{
		return;
	}

	DestroyIndicator();

	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!Character || !CombatTargets)
	{
		FinishComboStep(true);
		return;
	}

	const FVector SlamOrigin = Character->GetActorLocation();
	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Radius;
	Query.SearchOrigin = SlamOrigin;
	Query.SearchRadius = FMath::Max(1.0f, SlamRadius);
	Query.Instigator = Character;
	Query.Domain = EASCombatTargetDomain::ActorOnly;
	Query.bCanHitPlayers = bCanHitPlayers;

	FASCombatImpactContext ImpactContext;
	ImpactContext.Preset = ImpactPreset;
	ImpactContext.HitLocation = SlamOrigin;
	ImpactContext.HitDirection = Character->GetActorForwardVector().GetSafeNormal2D();

	const FGameplayEffectSpecHandle DamageSpec = MakeScaledDamageSpec();
	const FASCombatApplyResult Result = CombatTargets->ResolveAndApply(
		Query, MakeArrayView(&DamageSpec, 1), ImpactContext);

	for (AActor* Target : Result.ActorTargets)
	{
		ApplyKnockback(Target, SlamOrigin);
	}

	FinishComboStep(false);
}

FGameplayEffectSpecHandle UASMonsterSlamKnockbackAbility::MakeScaledDamageSpec() const
{
	FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
	if (DamageSpec.IsValid())
	{
		DamageSpec.Data->SetSetByCallerMagnitude(
			ASGameplayTags::Data_Damage,
			-CalculateDamage() * FMath::Max(0.0f, DamageScale));
	}

	return DamageSpec;
}

void UASMonsterSlamKnockbackAbility::SpawnIndicator()
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !IndicatorClass)
	{
		return;
	}

	const FTransform SpawnTransform(Character->GetActorRotation(), Character->GetActorLocation());
	AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
		IndicatorClass, SpawnTransform, Character, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Indicator)
	{
		return;
	}

	Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, SlamRadius, 0.0f, 0.0f,
		FMath::Max(0.01f, SlamWindupDuration));
	Indicator->FinishSpawning(SpawnTransform);
	ActiveIndicator = Indicator;
}

void UASMonsterSlamKnockbackAbility::DestroyIndicator()
{
	if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
	{
		Indicator->Destroy();
	}

	ActiveIndicator.Reset();
}

void UASMonsterSlamKnockbackAbility::ApplyKnockback(AActor* Target, const FVector& SlamOrigin) const
{
	ACharacter* TargetCharacter = Cast<ACharacter>(Target);
	UCharacterMovementComponent* Movement = TargetCharacter ? TargetCharacter->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	FVector Direction = (TargetCharacter->GetActorLocation() - SlamOrigin).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		if (const ACharacter* OwnerCharacter = GetOwnerCharacter())
		{
			Direction = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
		}
		else
		{
			Direction = FVector::ForwardVector;
		}
	}

	TSharedPtr<FRootMotionSource_JumpForce> Knockback = MakeShared<FRootMotionSource_JumpForce>();
	Knockback->InstanceName = TEXT("MonsterSlamKnockback");
	Knockback->AccumulateMode = ERootMotionAccumulateMode::Override;
	Knockback->Priority = 500;
	Knockback->Duration = KnockbackDuration;
	Knockback->Rotation = Direction.Rotation();
	Knockback->Distance = KnockbackDistance;
	Knockback->Height = KnockbackHeight;
	Knockback->bDisableTimeout = false;
	Knockback->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Knockback->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

	if (KnockbackHeight > 0.0f && Movement->IsMovingOnGround())
	{
		Movement->SetMovementMode(MOVE_Falling);
	}

	Movement->ApplyRootMotionSource(Knockback);
}
