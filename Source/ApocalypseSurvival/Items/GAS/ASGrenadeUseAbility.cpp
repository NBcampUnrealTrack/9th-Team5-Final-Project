#include "Items/GAS/ASGrenadeUseAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Core/Framework/ASPlayerController.h"
#include "Core/Input/ASInputTags.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Data/ASItemTags.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Use/ASGrenadeActor.h"
#include "Items/Use/ASGrenadeTrajectoryPreview.h"
#include "Skills/ASSkillAbility.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"
#include "TimerManager.h"

UASGrenadeUseAbility::UASGrenadeUseAbility()
{
	GrenadeClass = AASGrenadeActor::StaticClass();
	// Inventory requests still start on the server; the owning client also runs the targeting UI.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
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
	if (!World || !ASC || !Definition || !ActorInfo->PlayerController.IsValid() || !GrenadeClass
		|| GrenadeClass->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(ThrowRange) || ThrowRange <= 0.0f
		|| !FMath::IsFinite(ExplosionRadius) || ExplosionRadius <= 0.0f
		|| !FMath::IsFinite(ThrowFlightTime) || ThrowFlightTime < 0.05f
		|| !FMath::IsFinite(TargetingTimeout) || TargetingTimeout <= 0.0f
		|| SpawnOffset.ContainsNaN())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bTargeting = true;
	bSubmittedTarget = false;
	TargetingAvatar = Avatar;
	TargetingStartedAt = World->GetTimeSeconds();
	OtherAbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::OnOtherAbilityActivated);
	World->GetTimerManager().SetTimer(PendingUseTimer, this, &ThisClass::CheckPendingUse, 0.1f, true);
	if (ActorInfo->IsLocallyControlled())
	{
		BeginTargeting();
	}
	else if (ActorInfo->IsNetAuthority())
	{
		const FPredictionKey Key = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, Key).AddUObject(this, &ThisClass::OnTargetDataReceived);
		ASC->AbilityTargetDataCancelledDelegate(Handle, Key).AddUObject(this, &ThisClass::OnTargetDataCancelled);
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, Key);
	}
	else
	{
		OnLocalCancel();
	}
}

void UASGrenadeUseAbility::BeginTargeting()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	TSubclassOf<AASSkillRangeIndicatorActor> IndicatorClass = RangeIndicatorClass;
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
	if (!TrajectoryPreview || !TrajectoryPreview->InitializePreview(Avatar, GrenadeClass, ThrowFlightTime, SpawnOffset, RangeIndicator))
	{
		OnLocalCancel();
		return;
	}
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Targeting);
	bAddedTargetingTag = true;
	ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &ThisClass::OnLocalConfirm);
	ASC->GenericLocalCancelCallbacks.AddDynamic(this, &ThisClass::OnLocalCancel);
}

void UASGrenadeUseAbility::OnLocalConfirm()
{
	if (!bTargeting || bSubmittedTarget) return;
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	FVector CursorLocation;
	const AASPlayerController* PC = Cast<AASPlayerController>(CurrentActorInfo->PlayerController.Get());
	if ((PC && PC->GetItemInteractionComponent()->IsCursorOverInventory())
		|| !AASSkillRangeIndicatorActor::GetCursorWorldLocation(Avatar, CursorLocation))
	{
		// LocalInputConfirm clears its callbacks before broadcasting. Keep aiming if the click has no ground target.
		GetAbilitySystemComponentFromActorInfo()->GenericLocalConfirmCallbacks.AddUniqueDynamic(this, &ThisClass::OnLocalConfirm);
		return;
	}
	const FVector TargetLocation = AASSkillRangeIndicatorActor::ResolveTargetLocation(
		Avatar, CursorLocation, GetSourceItemDefinition()->GetEffectMagnitude(
			ASGameplayTags::Data_Item_ThrowRange, ASItemMagnitudeDefaults::ThrowRange));
	bSubmittedTarget = true;
	StopTargeting();
	if (CurrentActorInfo->IsNetAuthority())
	{
		ThrowAtLocation(TargetLocation);
		return;
	}
	FGameplayAbilityTargetDataHandle Data;
	FGameplayAbilityTargetData_LocationInfo* Location = new FGameplayAbilityTargetData_LocationInfo();
	Location->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Location->TargetLocation.LiteralTransform = FTransform(TargetLocation);
	Data.Add(Location);
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
		Data, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UASGrenadeUseAbility::OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	// Copy before consuming the cached handle: the incoming reference may refer to that cache itself.
	const FGameplayAbilityTargetDataHandle Received = Data;
	GetAbilitySystemComponentFromActorInfo()->ConsumeClientReplicatedTargetData(
		CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	if (!bTargeting || bSubmittedTarget) return;
	const FGameplayAbilityTargetData* Target = Received.Get(0);
	if (Received.Num() != 1 || !Target || Target->GetScriptStruct() != FGameplayAbilityTargetData_LocationInfo::StaticStruct()
		|| !Target->HasEndPoint() || Target->GetEndPointTransform().ContainsNaN())
	{
		OnTargetDataCancelled();
		return;
	}
	bSubmittedTarget = true;
	StopTargeting();
	ThrowAtLocation(Target->GetEndPointTransform().GetLocation());
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
	AASGrenadeActor* Grenade = World->SpawnActorDeferred<AASGrenadeActor>(GrenadeClass, SpawnTransform,
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

void UASGrenadeUseAbility::OnLocalCancel()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UASGrenadeUseAbility::OnTargetDataCancelled()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UASGrenadeUseAbility::OnOtherAbilityActivated(UGameplayAbility* Ability)
{
	if (bTargeting && Ability != this) OnLocalCancel();
}

void UASGrenadeUseAbility::CheckPendingUse()
{
	const UASItemInstance* Item = GetSourceItem();
	const UASInventoryComponent* Inventory = Item ? Item->GetTypedOuter<UASInventoryComponent>() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Item || !Inventory || Inventory->FindItemById(Item->GetInstanceId()) != Item
		|| !TargetingAvatar.IsValid() || GetAvatarActorFromActorInfo() != TargetingAvatar.Get()
		|| !ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| GetWorld()->GetTimeSeconds() - TargetingStartedAt >= TargetingTimeout)
	{
		OnLocalCancel();
	}
}

void UASGrenadeUseAbility::StopTargeting()
{
	bTargeting = false;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &ThisClass::OnLocalConfirm);
		ASC->GenericLocalCancelCallbacks.RemoveDynamic(this, &ThisClass::OnLocalCancel);
		if (bAddedTargetingTag) ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Targeting);
	}
	bAddedTargetingTag = false;
	if (IsValid(TrajectoryPreview)) TrajectoryPreview->Destroy();
	TrajectoryPreview = nullptr;
	if (IsValid(RangeIndicator)) RangeIndicator->Destroy();
	RangeIndicator = nullptr;
}

void UASGrenadeUseAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (!IsActive()) return;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC)
	{
		if (bWasCancelled && !bSubmittedTarget && !ActorInfo->IsNetAuthority())
		{
			ASC->ServerSetReplicatedTargetDataCancelled(Handle, ActivationInfo.GetActivationPredictionKey(), ASC->ScopedPredictionKey);
		}
		const FPredictionKey Key = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, Key).RemoveAll(this);
		ASC->AbilityTargetDataCancelledDelegate(Handle, Key).RemoveAll(this);
		ASC->ConsumeClientReplicatedTargetData(Handle, Key);
		ASC->AbilityActivatedCallbacks.Remove(OtherAbilityActivatedHandle);
	}
	OtherAbilityActivatedHandle.Reset();
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PendingUseTimer);
	StopTargeting();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
