#include "Items/GAS/ASGrenadeUseAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/Input/ASInputTags.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Data/ASItemTags.h"
#include "Items/Use/ASGrenadeActor.h"
#include "Items/Use/ASGrenadeTrajectoryPreview.h"
#include "Skills/ASSkillAbility.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"

UASGrenadeUseAbility::UASGrenadeUseAbility()
{
	GrenadeClass = AASGrenadeActor::StaticClass();
}

void UASGrenadeUseAbility::ActivateItemUse(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	APawn* Avatar = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	const float ThrowRange = Definition ? Definition->GetEffectMagnitude(
		ASGameplayTags::Data_Item_ThrowRange, ASItemMagnitudeDefaults::ThrowRange) : 0.0f;
	const float ExplosionRadius = Definition ? Definition->GetEffectMagnitude(
		ASGameplayTags::Data_Item_ExplosionRadius, ASItemMagnitudeDefaults::ExplosionRadius) : 0.0f;
	if (!World || !ASC || !Definition || !ActorInfo->PlayerController.IsValid() || !GrenadeClass.Get()
		|| GrenadeClass.Get()->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(ThrowRange) || ThrowRange <= 0.0f
		|| !FMath::IsFinite(ExplosionRadius) || ExplosionRadius <= 0.0f
		|| !FMath::IsFinite(ThrowFlightTime) || ThrowFlightTime < 0.05f
		|| !FMath::IsFinite(TargetingTimeout) || TargetingTimeout <= 0.0f
		|| SpawnOffset.ContainsNaN())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StartTargeting(TargetingTimeout, 0.1f, false);
}

void UASGrenadeUseAbility::BeginTargeting()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	TSubclassOf<AASSkillRangeIndicatorActor> IndicatorClass = RangeIndicatorClass.Get();
	if (!IndicatorClass)
	{
		// Reuse authored visuals through asset references already on the character, without hardcoding content paths.
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UASSkillAbility* Skill = Cast<UASSkillAbility>(Spec.Ability);
			if (!Skill || !Skill->GetRangeIndicatorClass()) continue;
			if (!IndicatorClass) IndicatorClass = Skill->GetRangeIndicatorClass();
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(ASGameplayTags::InputTag_Skill_E)
				|| Skill->GetAssetTags().HasTagExact(ASGameplayTags::InputTag_Skill_E))
			{
				IndicatorClass = Skill->GetRangeIndicatorClass();
				break;
			}
		}
	}
	if (!IndicatorClass || IndicatorClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Grenade] Configure RangeIndicatorClass, or grant a skill with a configured range indicator."));
		OnLocalCancel();
		return;
	}
	// Release another pending aim before binding our own confirmation.
	ASC->LocalInputCancel();
	if (APlayerController* PC = CurrentActorInfo->PlayerController.Get()) PC->StopMovement();
	FActorSpawnParameters Params;
	Params.Owner = Avatar;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	RangeIndicator = GetWorld()->SpawnActor<AASSkillRangeIndicatorActor>(IndicatorClass, Avatar->GetActorTransform(), Params);
	if (!RangeIndicator)
	{
		OnLocalCancel();
		return;
	}
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	FASSkillIndicatorSettings Settings;
	Settings.TargetingType = EASSkillTargetingType::Location;
	Settings.Shape = EASSkillIndicatorShape::Circle;
	Settings.Radius = Definition->GetEffectMagnitude(ASGameplayTags::Data_Item_ExplosionRadius, ASItemMagnitudeDefaults::ExplosionRadius);
	Settings.MaxCastRange = Definition->GetEffectMagnitude(ASGameplayTags::Data_Item_ThrowRange, ASItemMagnitudeDefaults::ThrowRange);
	Settings.RangeRingRadius = 0.0f;
	RangeIndicator->InitIndicator(Avatar, Settings);
	TrajectoryPreview = GetWorld()->SpawnActor<AASGrenadeTrajectoryPreview>(
		AASGrenadeTrajectoryPreview::StaticClass(), Avatar->GetActorTransform(), Params);
	if (!TrajectoryPreview || !TrajectoryPreview->InitializePreview(Avatar, GrenadeClass.Get(), ThrowFlightTime, SpawnOffset, RangeIndicator))
	{
		OnLocalCancel();
		return;
	}
	BindTargetingInput();
}

void UASGrenadeUseAbility::ThrowAtLocation(const FVector& DesiredLocation)
{
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	const float ThrowRange = Definition ? Definition->GetEffectMagnitude(
		ASGameplayTags::Data_Item_ThrowRange, ASItemMagnitudeDefaults::ThrowRange) : 0.0f;
	if (!World || !Definition || !CurrentActorInfo->IsNetAuthority() || DesiredLocation.ContainsNaN()
		|| !FMath::IsFinite(ThrowRange) || ThrowRange <= 0.0f
		|| !CheckCost(CurrentSpecHandle, CurrentActorInfo))
	{
		OnTargetDataCancelled();
		return;
	}
	// Range and ground height come from server item data and world geometry, never from client claims.
	const FVector TargetLocation = AASSkillRangeIndicatorActor::ResolveTargetLocation(Avatar, DesiredLocation, ThrowRange);
	const FTransform SpawnTransform = AASGrenadeActor::MakeThrowTransform(Avatar, TargetLocation, SpawnOffset);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASGrenadeSpawn), false, Avatar);
	FHitResult Obstruction;
	if (World->LineTraceSingleByChannel(Obstruction, Avatar->GetActorLocation(), SpawnTransform.GetLocation(), ECC_Visibility, Params))
	{
		OnTargetDataCancelled();
		return;
	}
	AASGrenadeActor* Grenade = World->SpawnActorDeferred<AASGrenadeActor>(GrenadeClass.Get(), SpawnTransform,
		Avatar, Avatar, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (Grenade)
	{
		Grenade->FinishSpawning(SpawnTransform);
	}
	UProjectileMovementComponent* Movement = IsValid(Grenade) ? Grenade->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
	USphereComponent* Collision = IsValid(Grenade) ? Grenade->FindComponentByClass<USphereComponent>() : nullptr;
	FVector Velocity = FVector::ZeroVector;
	if (Movement && Collision)
	{
		Velocity = AASGrenadeActor::MakeThrowVelocity(Grenade->GetActorLocation(), TargetLocation,
			Collision->GetScaledSphereRadius(), Movement->GetGravityZ(), ThrowFlightTime);
	}
	if (!Movement || !Collision || !Grenade->InitializeGrenade(ASC, GetSourceItem(), Velocity))
	{
		if (IsValid(Grenade)) Grenade->Destroy();
		OnTargetDataCancelled();
		return;
	}
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		Grenade->Destroy();
		OnTargetDataCancelled();
		return;
	}
	Grenade->StartThrow();
	Avatar->SetActorRotation(SpawnTransform.Rotator());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}


float UASGrenadeUseAbility::GetTargetRange() const
{
	return GetSourceItemDefinition()->GetEffectMagnitude(ASGameplayTags::Data_Item_ThrowRange, ASItemMagnitudeDefaults::ThrowRange);
}

void UASGrenadeUseAbility::ExecuteAtLocation(const FVector& Location)
{
	ThrowAtLocation(Location);
}

void UASGrenadeUseAbility::CleanupTargetingVisuals()
{
	if (IsValid(TrajectoryPreview)) TrajectoryPreview->Destroy();
	TrajectoryPreview = nullptr;
	if (IsValid(RangeIndicator)) RangeIndicator->Destroy();
	RangeIndicator = nullptr;
}
