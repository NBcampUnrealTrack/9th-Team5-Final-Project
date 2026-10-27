#include "GAS/Characters/Enemies/Abilities/ASMonsterRadialBurstCascadeAbility.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

UASMonsterRadialBurstCascadeAbility::UASMonsterRadialBurstCascadeAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	ImpactPreset = EASImpactPreset::Standard;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	IndicatorClass = AASMonsterAttackIndicatorActor::StaticClass();
	CompletionEventTag = ASGameplayTags::Event_Monster_ComboStep_RadialBurstCascadeFinished;

	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_RadialBurstCascade);
	SetAssetTags(AssetTagsToSet);
}

void UASMonsterRadialBurstCascadeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	CascadeOrigin = Character->GetActorLocation();
	CurrentStageIndex = 0;
	StartAngleDegrees = bRandomStartAngle ? FMath::FRandRange(0.0f, 60.0f) : 0.0f;
	DestroyCurrentStageIndicators();
	BeginCurrentStage();
}

void UASMonsterRadialBurstCascadeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(TelegraphTimerHandle);
		Character->GetWorldTimerManager().ClearTimer(NextStageTimerHandle);
	}

	DestroyCurrentStageIndicators();
	CurrentStageIndex = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterRadialBurstCascadeAbility::BeginCurrentStage()
{
	if (!IsActive())
	{
		return;
	}

	SpawnCurrentStageIndicators();
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().SetTimer(TelegraphTimerHandle, this,
			&ThisClass::ExplodeCurrentStage, FMath::Max(0.05f, TelegraphDuration), false);
	}
	else
	{
		FinishComboStep(true);
	}
}

void UASMonsterRadialBurstCascadeAbility::ExplodeCurrentStage()
{
	if (!IsActive())
	{
		return;
	}

	DestroyCurrentStageIndicators();
	ApplyCurrentStageDamage();

	++CurrentStageIndex;
	if (CurrentStageIndex >= FMath::Max(1, BurstStageCount))
	{
		FinishComboStep(false);
		return;
	}

	if (StageDelay <= 0.0f)
	{
		BeginCurrentStage();
		return;
	}

	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().SetTimer(NextStageTimerHandle, this,
			&ThisClass::BeginCurrentStage, StageDelay, false);
	}
	else
	{
		FinishComboStep(true);
	}
}

void UASMonsterRadialBurstCascadeAbility::SpawnCurrentStageIndicators()
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !IndicatorClass)
	{
		return;
	}

	for (int32 DirectionIndex = 0; DirectionIndex < FMath::Max(1, DirectionCount); ++DirectionIndex)
	{
		const FVector Direction = GetStageDirection(DirectionIndex);
		const FTransform SpawnTransform(Direction.Rotation(), GetStageLocation(DirectionIndex));
		AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
			IndicatorClass, SpawnTransform, Character, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Indicator)
		{
			continue;
		}

		Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, BurstRadius, 0.0f, 0.0f, TelegraphDuration);
		Indicator->FinishSpawning(SpawnTransform);
		ActiveIndicators.Add(Indicator);
	}
}

void UASMonsterRadialBurstCascadeAbility::DestroyCurrentStageIndicators()
{
	for (TWeakObjectPtr<AASMonsterAttackIndicatorActor>& Indicator : ActiveIndicators)
	{
		if (AASMonsterAttackIndicatorActor* IndicatorActor = Indicator.Get())
		{
			IndicatorActor->Destroy();
		}
	}

	ActiveIndicators.Reset();
}

void UASMonsterRadialBurstCascadeAbility::ApplyCurrentStageDamage()
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!Character || !CombatTargets)
	{
		return;
	}

	FASCombatTargetHitSet HitTargets;
	for (int32 DirectionIndex = 0; DirectionIndex < FMath::Max(1, DirectionCount); ++DirectionIndex)
	{
		const FVector Direction = GetStageDirection(DirectionIndex);
		FASCombatTargetQuery Query;
		Query.Shape = EASCombatTargetShape::Radius;
		Query.SearchOrigin = GetStageLocation(DirectionIndex);
		Query.SearchRadius = FMath::Max(1.0f, BurstRadius);
		Query.Instigator = Character;
		Query.Domain = EASCombatTargetDomain::ActorOnly;
		Query.bCanHitPlayers = bCanHitPlayers;

		FASCombatImpactContext ImpactContext;
		ImpactContext.Preset = ImpactPreset;
		ImpactContext.HitLocation = Query.SearchOrigin;
		ImpactContext.HitDirection = Direction;
		const FGameplayEffectSpecHandle DamageSpec = MakeScaledDamageSpec();
		CombatTargets->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1), ImpactContext, &HitTargets);
	}
}

FGameplayEffectSpecHandle UASMonsterRadialBurstCascadeAbility::MakeScaledDamageSpec() const
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

FVector UASMonsterRadialBurstCascadeAbility::GetStageLocation(const int32 DirectionIndex) const
{
	const float Distance = FMath::Max(0.0f, InitialBurstRadius)
		+ FMath::Max(0.0f, BurstStepDistance) * CurrentStageIndex;
	return CascadeOrigin + GetStageDirection(DirectionIndex) * Distance;
}

FVector UASMonsterRadialBurstCascadeAbility::GetStageDirection(const int32 DirectionIndex) const
{
	const int32 SafeDirectionCount = FMath::Max(1, DirectionCount);
	const float AngleDegrees = StartAngleDegrees + 360.0f * DirectionIndex / SafeDirectionCount;
	return FRotator(0.0f, AngleDegrees, 0.0f).Vector();
}
