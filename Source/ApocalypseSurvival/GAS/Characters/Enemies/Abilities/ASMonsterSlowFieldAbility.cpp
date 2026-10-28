#include "GAS/Characters/Enemies/Abilities/ASMonsterSlowFieldAbility.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/ASMonsterSlowFieldActor.h"
#include "Characters/Enemy/ASMonsterSlowFieldProjectile.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Effects/ASMonsterSlowGameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"

UASMonsterSlowFieldAbility::UASMonsterSlowFieldAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	WindupDuration = 0.45f;
	ProjectileClass = AASMonsterSlowFieldProjectile::StaticClass();
	SlowFieldClass = AASMonsterSlowFieldActor::StaticClass();
	LandingIndicatorClass = AASMonsterAttackIndicatorActor::StaticClass();
	SlowEffectClass = UASMonsterSlowGameplayEffect::StaticClass();

	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_SlowField);
	SetAssetTags(AssetTagsToSet);
}

void UASMonsterSlowFieldAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	LockedTarget.Reset();
	LandingLocation = FVector::ZeroVector;
	bHasLandingLocation = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UASMonsterSlowFieldAbility::BeginPatternWindup()
{
	if (!LockTargetAndPredictLandingLocation())
	{
		FinishPattern(true);
		return;
	}

	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	SpawnLandingIndicator();
}

bool UASMonsterSlowFieldAbility::ExecutePatternEffect()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!HasSkillAuthority() || !IsValid(Avatar) || !World || !bHasLandingLocation)
	{
		return true;
	}

	const FVector ToLanding = (LandingLocation - Avatar->GetActorLocation()).GetSafeNormal2D();
	const FVector SpawnLocation = Avatar->GetActorLocation() + ToLanding * 75.0f + FVector(0.0f, 0.0f, 70.0f);
	const FTransform SpawnTransform(ToLanding.IsNearlyZero() ? Avatar->GetActorRotation() : ToLanding.Rotation(), SpawnLocation);
	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : AASMonsterSlowFieldProjectile::StaticClass();

	AASMonsterSlowFieldProjectile* Projectile = World->SpawnActorDeferred<AASMonsterSlowFieldProjectile>(
		ClassToSpawn, SpawnTransform, Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		const FGameplayEffectSpecHandle SlowSpec = SlowEffectClass
			? MakeOutgoingGameplayEffectSpec(SlowEffectClass, GetAbilityLevel())
			: FGameplayEffectSpecHandle();
		Projectile->InitializeProjectile(LandingLocation, ProjectileTravelDuration, ProjectileArcHeight,
			SlowSpec, Avatar, SlowFieldClass, FieldRadius, FieldDuration);
		Projectile->FinishSpawning(SpawnTransform);
	}

	return true;
}

void UASMonsterSlowFieldAbility::EndPattern(const bool bWasCancelled)
{
	if (bWasCancelled)
	{
		DestroyLandingIndicator();
	}

	LockedTarget.Reset();
	bHasLandingLocation = false;
}

bool UASMonsterSlowFieldAbility::LockTargetAndPredictLandingLocation()
{
	const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetAvatarActorFromActorInfo());
	const UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	AActor* Target = Targeting ? Targeting->GetTargetActor() : nullptr;
	if (!IsValid(Target))
	{
		return false;
	}

	LockedTarget = Target;
	FVector CandidateLocation = Target->GetActorLocation();
	const FVector Velocity = Target->GetVelocity();
	if (Velocity.SizeSquared2D() >= FMath::Square(80.0f))
	{
		CandidateLocation += Velocity.GetSafeNormal2D() * Velocity.Size2D() * PredictionLeadTime;
	}

	LandingLocation = ClampLandingLocationToThrowRange(CandidateLocation);
	bHasLandingLocation = true;
	return true;
}

FVector UASMonsterSlowFieldAbility::ClampLandingLocationToThrowRange(const FVector& CandidateLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return FVector::ZeroVector;
	}

	const FVector Origin = Avatar->GetActorLocation();
	FVector Offset = CandidateLocation - Origin;
	Offset.Z = 0.0f;
	const float Distance = Offset.Size();
	if (Distance > MaxThrowDistance && Distance > KINDA_SMALL_NUMBER)
	{
		Offset *= MaxThrowDistance / Distance;
	}

	return FVector(Origin.X + Offset.X, Origin.Y + Offset.Y, CandidateLocation.Z);
}

void UASMonsterSlowFieldAbility::SpawnLandingIndicator()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!HasSkillAuthority() || !World || !LandingIndicatorClass)
	{
		return;
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, LandingLocation);
	AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
		LandingIndicatorClass, SpawnTransform, Avatar, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Indicator)
	{
		Indicator->InitializeIndicator(EASMonsterAttackIndicatorShape::Circle, FieldRadius, 0.0f, 0.0f,
			WindupDuration + ProjectileTravelDuration);
		Indicator->FinishSpawning(SpawnTransform);
		LandingIndicator = Indicator;
	}
}

void UASMonsterSlowFieldAbility::DestroyLandingIndicator()
{
	if (AASMonsterAttackIndicatorActor* Indicator = LandingIndicator.Get())
	{
		Indicator->Destroy();
	}

	LandingIndicator.Reset();
}
