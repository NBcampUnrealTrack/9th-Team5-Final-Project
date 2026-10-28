#include "GAS/Characters/Enemies/Abilities/ASMonsterSniperBeamAbility.h"

#include "AbilitySystemComponent.h"
#include "Characters/Enemy/ASMonsterBeamTelegraphActor.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "CollisionQueryParams.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Effects/ASMonsterDamageGameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

UASMonsterSniperBeamAbility::UASMonsterSniperBeamAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	bStopMovementWhileCasting = true;
	ImpactPreset = EASImpactPreset::Standard;
	BaseDamage = 0.0f;
	AttackPowerScale = 2.25f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	// BP에서 DamageEffectClass를 따로 지정하지 않아도 빔이 항상 유효한 피해 Spec을 만든다.
	DamageEffectClass = UASMonsterDamageGameplayEffect::StaticClass();
	TelegraphClass = AASMonsterBeamTelegraphActor::StaticClass();

	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_SniperBeam);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	SetAssetTags(AssetTagsToSet);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Aiming);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_HitReact);
}

void UASMonsterSniperBeamAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!HasSkillAuthority() || !CommitAbility(Handle, ActorInfo, ActivationInfo) || !LockTarget())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		FinishBeam(true);
		return;
	}

	Character->GetCharacterMovement()->StopMovementImmediately();
	if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(Character))
	{
		Monster->SetFaceControllerFocus(true);
	}

	UpdateTrackingAim();
	if (TelegraphClass)
	{
		const FTransform SpawnTransform(LockedDirection.Rotation(), LockedStartLocation);
		AASMonsterBeamTelegraphActor* NewTelegraph = World->SpawnActorDeferred<AASMonsterBeamTelegraphActor>(
			TelegraphClass, SpawnTransform, Character, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (NewTelegraph)
		{
			NewTelegraph->InitializeBeam(LockedStartLocation, LockedDirection, LockedLength, BeamWidth,
				TrackingDuration + LockDuration, TrackingDuration);
			NewTelegraph->FinishSpawning(SpawnTransform);
			Telegraph = NewTelegraph;
		}
	}

	World->GetTimerManager().SetTimer(TrackingTimerHandle, this,
		&UASMonsterSniperBeamAbility::UpdateTrackingAim, TrackingUpdateInterval, true);
	World->GetTimerManager().SetTimer(LockTimerHandle, this,
		&UASMonsterSniperBeamAbility::LockAim, TrackingDuration, false);
	World->GetTimerManager().SetTimer(FireTimerHandle, this,
		&UASMonsterSniperBeamAbility::FireBeam, TrackingDuration + LockDuration, false);
}

void UASMonsterSniperBeamAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicatedEndAbility, const bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(TrackingTimerHandle);
		Character->GetWorldTimerManager().ClearTimer(LockTimerHandle);
		Character->GetWorldTimerManager().ClearTimer(FireTimerHandle);
		if (AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(Character))
		{
			Monster->SetFaceControllerFocus(false);
		}
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Monster_AimLocked);
	}

	DestroyTelegraph();
	LockedTarget.Reset();
	bAimLocked = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

bool UASMonsterSniperBeamAbility::LockTarget()
{
	const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetAvatarActorFromActorInfo());
	const UASMonsterTargetingComponent* Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	AActor* Target = Targeting ? Targeting->GetTargetActor() : nullptr;
	if (!IsValid(Target))
	{
		return false;
	}

	LockedTarget = Target;
	bAimLocked = false;
	return true;
}

void UASMonsterSniperBeamAbility::UpdateTrackingAim()
{
	if (!IsActive() || bAimLocked)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	AActor* Target = LockedTarget.Get();
	if (!IsValid(Avatar) || !IsValid(Target))
	{
		FinishBeam(true);
		return;
	}

	LockedStartLocation = GetMuzzleLocation();
	LockedDirection = (Target->GetActorLocation() - LockedStartLocation).GetSafeNormal2D();
	if (LockedDirection.IsNearlyZero())
	{
		LockedDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	LockedLength = GetBlockedBeamLength(LockedStartLocation, LockedDirection);
	Avatar->SetActorRotation(LockedDirection.Rotation());

	if (AASMonsterBeamTelegraphActor* ActiveTelegraph = Telegraph.Get())
	{
		ActiveTelegraph->UpdateTrackingTransform(LockedStartLocation, LockedDirection, LockedLength);
	}
}

void UASMonsterSniperBeamAbility::LockAim()
{
	if (!IsActive())
	{
		return;
	}

	UpdateTrackingAim();
	bAimLocked = true;
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(TrackingTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(ASGameplayTags::State_Monster_AimLocked);
	}

	if (AASMonsterBeamTelegraphActor* ActiveTelegraph = Telegraph.Get())
	{
		ActiveTelegraph->SetPhase(EASMonsterBeamTelegraphPhase::Locked);
	}
}

void UASMonsterSniperBeamAbility::FireBeam()
{
	if (!IsActive() || !HasSkillAuthority())
	{
		return;
	}

	if (!bAimLocked)
	{
		LockAim();
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (IsValid(Avatar) && World && !LockedDirection.IsNearlyZero())
	{
		if (UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>())
		{
			FASCombatTargetQuery Query;
			Query.Shape = EASCombatTargetShape::Box;
			Query.SearchOrigin = LockedStartLocation + LockedDirection * (LockedLength * 0.5f);
			Query.Direction = LockedDirection;
			Query.BoxHalfExtent = FVector(LockedLength * 0.5f, BeamWidth * 0.5f, 100.0f);
			Query.Instigator = Avatar;
			Query.Domain = EASCombatTargetDomain::ActorOnly;
			Query.bCanHitPlayers = true;

			FASCombatImpactContext ImpactContext;
			ImpactContext.Preset = ImpactPreset;
			ImpactContext.HitLocation = LockedStartLocation;
			ImpactContext.HitDirection = LockedDirection;
			FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
			// 이미 생성된 Blueprint가 과거의 null 기본값을 직렬화한 경우에도 피해가 빠지지 않게 한다.
			if (!DamageSpec.IsValid())
			{
				DamageSpec = MakeOutgoingGameplayEffectSpec(UASMonsterDamageGameplayEffect::StaticClass(), GetAbilityLevel());
				if (DamageSpec.IsValid())
				{
					DamageSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -CalculateDamage());
				}
			}

			if (DamageSpec.IsValid())
			{
				CombatTargets->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1), ImpactContext);
			}
		}
	}

	if (AASMonsterBeamTelegraphActor* ActiveTelegraph = Telegraph.Get())
	{
		ActiveTelegraph->NotifyBeamFired();
	}

	FinishBeam(false);
}

void UASMonsterSniperBeamAbility::FinishBeam(const bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UASMonsterSniperBeamAbility::DestroyTelegraph()
{
	if (AASMonsterBeamTelegraphActor* ActiveTelegraph = Telegraph.Get())
	{
		ActiveTelegraph->Destroy();
	}

	Telegraph.Reset();
}

FVector UASMonsterSniperBeamAbility::GetMuzzleLocation() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return FVector::ZeroVector;
	}

	return Avatar->GetActorLocation() + Avatar->GetActorForwardVector().GetSafeNormal2D() * MuzzleForwardOffset
		+ FVector(0.0f, 0.0f, MuzzleHeightOffset);
}

float UASMonsterSniperBeamAbility::GetBlockedBeamLength(const FVector& Start, const FVector& Direction) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EliteBSniperBeam), false, Avatar);
	// Trace channel(ECC_WorldStatic)은 Pawn이 해당 채널을 Block으로 응답하면 플레이어 캡슐에도
	// 멈출 수 있다. 이 Trace는 빔 길이를 자를 "실제 벽"만 찾아야 하므로 Object Type을 한정한다.
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	const FVector End = Start + Direction.GetSafeNormal2D() * BeamLength;
	if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjectQuery, QueryParams))
	{
		return FMath::Max(1.0f, FVector::Dist2D(Start, Hit.ImpactPoint));
	}

	return BeamLength;
}
