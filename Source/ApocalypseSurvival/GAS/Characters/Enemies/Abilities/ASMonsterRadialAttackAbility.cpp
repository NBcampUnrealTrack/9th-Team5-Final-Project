#include "GAS/Characters/Enemies/Abilities/ASMonsterRadialAttackAbility.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UASMonsterRadialAttackAbility::UASMonsterRadialAttackAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	ImpactPreset = EASImpactPreset::Standard;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	IndicatorClass = AASMonsterAttackIndicatorActor::StaticClass();
	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_Radial);
	SetAssetTags(AssetTagsToSet);
	CompletionEventTag = ASGameplayTags::Event_Monster_ComboStep_RadialFinished;
}

void UASMonsterRadialAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	if (!IsValid(Character) || !World)
	{
		FinishComboStep(true);
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
	FinishComboStep(false);
}

void UASMonsterRadialAttackAbility::OnSkillEffect_Implementation()
{
	if (!HasSkillAuthority())
	{
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!Character || !CombatTargets)
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Radius;
	Query.SearchOrigin = Character->GetActorLocation();
	Query.SearchRadius = FMath::Max(1.0f, Radius);
	Query.Instigator = Character;
	Query.Domain = EASCombatTargetDomain::ActorOnly;
	Query.bCanHitPlayers = bCanHitPlayers;

	FASCombatImpactContext ImpactContext;
	ImpactContext.Preset = ImpactPreset;
	ImpactContext.HitLocation = Character->GetActorLocation();
	ImpactContext.HitDirection = Character->GetActorForwardVector().GetSafeNormal2D();

	const FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
	CombatTargets->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1), ImpactContext);
}

void UASMonsterRadialAttackAbility::DestroyIndicator()
{
	if (AASMonsterAttackIndicatorActor* Indicator = ActiveIndicator.Get())
	{
		Indicator->Destroy();
	}

	ActiveIndicator.Reset();
}
