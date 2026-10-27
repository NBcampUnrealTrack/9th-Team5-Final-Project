#include "GAS/Characters/Enemies/Abilities/ASMonsterRadialProjectileVolleyAbility.h"

#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "TimerManager.h"

UASMonsterRadialProjectileVolleyAbility::UASMonsterRadialProjectileVolleyAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	ImpactPreset = EASImpactPreset::Standard;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	ProjectileClass = AASSkillProjectileActor::StaticClass();
	CompletionEventTag = ASGameplayTags::Event_Monster_ComboStep_RadialVolleyFinished;

	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_RadialVolley);
	SetAssetTags(AssetTagsToSet);
}

void UASMonsterRadialProjectileVolleyAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	if (!Character || !World || !ProjectileClass)
	{
		FinishComboStep(true);
		return;
	}

	Character->GetCharacterMovement()->StopMovementImmediately();
	CurrentVolleyIndex = 0;
	if (RoarWindupDuration <= 0.0f)
	{
		FireNextVolley();
		return;
	}

	World->GetTimerManager().SetTimer(
		RoarWindupTimerHandle, this, &ThisClass::FireNextVolley, RoarWindupDuration, false);
}

void UASMonsterRadialProjectileVolleyAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(RoarWindupTimerHandle);
		Character->GetWorldTimerManager().ClearTimer(VolleyTimerHandle);
	}

	CurrentVolleyIndex = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterRadialProjectileVolleyAbility::FireNextVolley()
{
	if (!IsActive())
	{
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		FinishComboStep(true);
		return;
	}

	const int32 SafeProjectileCount = FMath::Max(1, ProjectileCount);
	const float AngleStep = 360.0f / SafeProjectileCount;
	const float VolleyOffset = CurrentVolleyIndex * VolleyAngleOffset;
	for (int32 ProjectileIndex = 0; ProjectileIndex < SafeProjectileCount; ++ProjectileIndex)
	{
		const FVector Direction = FRotator(0.0f, VolleyOffset + AngleStep * ProjectileIndex, 0.0f).Vector();
		SpawnProjectile(Direction);
	}

	++CurrentVolleyIndex;
	if (CurrentVolleyIndex >= FMath::Max(1, VolleyCount))
	{
		FinishComboStep(false);
		return;
	}

	Character->GetWorldTimerManager().SetTimer(
		VolleyTimerHandle, this, &ThisClass::FireNextVolley, FMath::Max(0.01f, VolleyInterval), false);
}

void UASMonsterRadialProjectileVolleyAbility::SpawnProjectile(const FVector& Direction) const
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !ProjectileClass)
	{
		return;
	}

	const FVector SafeDirection = Direction.GetSafeNormal2D();
	const FVector SpawnLocation = Character->GetActorLocation()
		+ SafeDirection * SpawnForwardOffset
		+ FVector(0.0f, 0.0f, SpawnHeightOffset);
	const FTransform SpawnTransform(SafeDirection.Rotation(), SpawnLocation);
	AASSkillProjectileActor* Projectile = World->SpawnActorDeferred<AASSkillProjectileActor>(
		ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return;
	}

	FASCombatImpactContext ImpactContext;
	ImpactContext.Preset = ImpactPreset;
	ImpactContext.HitLocation = SpawnLocation;
	ImpactContext.HitDirection = SafeDirection;
	Projectile->InitProjectile(MakeScaledDamageSpec(), Character, SafeDirection, ProjectileSpeed,
		ProjectileRange, ProjectilePierceCount, bCanHitPlayers, ImpactContext);
	Projectile->FinishSpawning(SpawnTransform);
}

FGameplayEffectSpecHandle UASMonsterRadialProjectileVolleyAbility::MakeScaledDamageSpec() const
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
