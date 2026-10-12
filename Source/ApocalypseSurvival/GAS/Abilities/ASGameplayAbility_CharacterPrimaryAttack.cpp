#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Characters/Player/Components/ASEquipmentComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"

UASGameplayAbility_CharacterPrimaryAttack::UASGameplayAbility_CharacterPrimaryAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Dodging);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Casting);
}

void UASGameplayAbility_CharacterPrimaryAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bAttackStarted = false;
	bHitPerformed = false;

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority() && !IsLocallyControlled())
	{
		WaitForClientAim();
		return;
	}

	FVector AimDirection;
	FVector TargetLocation;
	ComputeLocalAim(AimDirection, TargetLocation);
	if (!ActorInfo->IsNetAuthority())
	{
		SendAimToServer(AimDirection, TargetLocation);
	}
	StartAttack(AimDirection);
}

void UASGameplayAbility_CharacterPrimaryAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicatedEndAbility, const bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, PredictionKey).RemoveAll(this);
		ASC->AbilityTargetDataCancelledDelegate(Handle, PredictionKey).RemoveAll(this);
	}

	bAttackStarted = false;
	bHitPerformed = false;
	CachedExecutionContext = FASWeaponAttackExecutionContext();
	CachedExecution = nullptr;
	ActiveMontage = nullptr;
	ActiveHitEventTag = FGameplayTag();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

bool UASGameplayAbility_CharacterPrimaryAttack::BuildExecutionContext(
	FASWeaponAttackExecutionContext& OutContext, const UASWeaponAttackExecution*& OutExecution) const
{
	OutContext = FASWeaponAttackExecutionContext();
	OutExecution = nullptr;
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UASEquipmentComponent* Equipment = Avatar ? Avatar->FindComponentByClass<UASEquipmentComponent>() : nullptr;
	UASItemInstance* Weapon = Equipment ? Equipment->GetEquippedItem() : nullptr;
	const UASItemDefinition* Definition = Weapon ? Weapon->GetItemDefinition() : nullptr;
	UASInventoryComponent* Inventory = Weapon ? Weapon->GetTypedOuter<UASInventoryComponent>() : nullptr;
	if (!Avatar || !ASC || !Weapon || !Definition || !Inventory || Definition->ItemType != EASItemType::Weapon)
	{
		return false;
	}

	const UASWeaponAttackProfile* Profile = Definition->AttackProfile;
	UClass* ExecutionClass = Profile ? Profile->ExecutionClass.Get() : nullptr;
	OutExecution = ExecutionClass ? ExecutionClass->GetDefaultObject<UASWeaponAttackExecution>() : nullptr;
	if (!OutExecution) return false;

	OutContext.Avatar = Avatar;
	OutContext.AbilitySystem = ASC;
	OutContext.Inventory = Inventory;
	OutContext.Weapon = Weapon;
	OutContext.Definition = Definition;
	OutContext.Profile = Profile;
	return true;
}

void UASGameplayAbility_CharacterPrimaryAttack::ResolveAnimation(
	const FASWeaponAttackExecutionContext& Context, UAnimMontage*& OutMontage,
	FGameplayTag& OutHitEventTag, bool& bOutScaleToInterval) const
{
	OutMontage = nullptr;
	OutHitEventTag = ASGameplayTags::Event_Skill_Hit;
	bOutScaleToInterval = true;
	if (!Context.Profile) return;
	OutMontage = Context.Profile->AttackMontage;
	OutHitEventTag = Context.Profile->HitEventTag;
	bOutScaleToInterval = Context.Profile->bScaleMontageToAttackInterval;
}

void UASGameplayAbility_CharacterPrimaryAttack::ComputeLocalAim(FVector& OutAimDirection, FVector& OutTargetLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const FVector Origin = Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;
	OutAimDirection = Avatar ? Avatar->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	OutTargetLocation = Origin;
	FVector CursorLocation;
	if (Avatar && AASSkillRangeIndicatorActor::GetCursorWorldLocation(Cast<APawn>(Avatar), CursorLocation))
	{
		const FVector ToCursor = (CursorLocation - Origin).GetSafeNormal2D();
		if (!ToCursor.IsNearlyZero()) OutAimDirection = ToCursor;
		OutTargetLocation = CursorLocation;
	}
}

void UASGameplayAbility_CharacterPrimaryAttack::SendAimToServer(const FVector& AimDirection, const FVector& TargetLocation)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;
	FGameplayAbilityTargetData_LocationInfo* AimData = new FGameplayAbilityTargetData_LocationInfo();
	AimData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	AimData->TargetLocation.LiteralTransform = FTransform(AimDirection.Rotation(), TargetLocation);
	FGameplayAbilityTargetDataHandle TargetData;
	TargetData.Add(AimData);
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
		TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UASGameplayAbility_CharacterPrimaryAttack::WaitForClientAim()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	const FPredictionKey PredictionKey = CurrentActivationInfo.GetActivationPredictionKey();
	ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, PredictionKey).AddUObject(
		this, &UASGameplayAbility_CharacterPrimaryAttack::OnClientAimReceived);
	ASC->AbilityTargetDataCancelledDelegate(CurrentSpecHandle, PredictionKey).AddUObject(
		this, &UASGameplayAbility_CharacterPrimaryAttack::OnClientAimCancelled);
	ASC->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, PredictionKey);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnClientAimReceived(
	const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (bAttackStarted) return;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
	FVector AimDirection;
	FVector TargetLocation;
	ComputeLocalAim(AimDirection, TargetLocation);
	if (const FGameplayAbilityTargetData* AimData = Data.Get(0); AimData && AimData->HasEndPoint())
	{
		AimDirection = AimData->GetEndPointTransform().GetRotation().Vector().GetSafeNormal2D();
	}
	StartAttack(AimDirection);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnClientAimCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UASGameplayAbility_CharacterPrimaryAttack::StartAttack(const FVector& AimDirection)
{
	if (bAttackStarted || !CurrentActorInfo) return;
	const UASWeaponAttackExecution* Execution = nullptr;
	FASWeaponAttackExecutionContext Context;
	if (!BuildExecutionContext(Context, Execution) || !Execution->CanPayCost(Context))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UWorld* World = Context.Avatar->GetWorld();
	if (!World || World->GetTimeSeconds() < NextAttackTime)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (CurrentActorInfo->IsNetAuthority() && !Execution->CommitCost(Context))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bAttackStarted = true;
	bHitPerformed = false;
	NextAttackTime = World->GetTimeSeconds() + FMath::Max(0.01f, Context.Definition->AttackInterval);
	CachedAimDirection = AimDirection.GetSafeNormal2D();
	if (CachedAimDirection.IsNearlyZero()) CachedAimDirection = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
	Context.AimDirection = CachedAimDirection;
	CachedExecutionContext = Context;
	CachedExecution = Execution;
	Context.Avatar->SetActorRotation(FRotator(0.0f, CachedAimDirection.Rotation().Yaw, 0.0f));

	bool bScaleToInterval = true;
	UAnimMontage* ResolvedMontage = nullptr;
	ResolveAnimation(Context, ResolvedMontage, ActiveHitEventTag, bScaleToInterval);
	ActiveMontage = ResolvedMontage;
	if (ActiveMontage)
	{
		if (ActiveHitEventTag.IsValid())
		{
			UAbilityTask_WaitGameplayEvent* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this, ActiveHitEventTag, nullptr, true, true);
			WaitTask->EventReceived.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnHitEvent);
			WaitTask->ReadyForActivation();
		}
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ActiveMontage, GetMontagePlayRate(ActiveMontage, Context.Definition, bScaleToInterval),
			NAME_None, true);
		MontageTask->OnCompleted.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageCancelled);
		MontageTask->ReadyForActivation();
		if (!ActiveHitEventTag.IsValid() && CurrentActorInfo->IsNetAuthority()) PerformHit();
		return;
	}

	if (CurrentActorInfo->IsNetAuthority())
	{
		PerformHit();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	// A predicting client with no montage waits for the server's replicated end. Ending here can
	// race the TargetData RPC and cancel the authoritative activation before it receives the aim.
}

void UASGameplayAbility_CharacterPrimaryAttack::PerformHit()
{
	if (bHitPerformed || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	bHitPerformed = true;

	// The equipped weapon is authoritative. Do not finish an old weapon's attack after a swap.
	FASWeaponAttackExecutionContext CurrentContext;
	const UASWeaponAttackExecution* CurrentExecution = nullptr;
	if (!BuildExecutionContext(CurrentContext, CurrentExecution)
		|| CurrentContext.Weapon != CachedExecutionContext.Weapon
		|| CurrentExecution != CachedExecution
		|| CurrentContext.AbilitySystem->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
	{
		return;
	}

	if (const UASWeaponAttackExecution* Execution = CachedExecution)
	{
		Execution->ExecuteHit(CachedExecutionContext);
	}
}

float UASGameplayAbility_CharacterPrimaryAttack::GetMontagePlayRate(
	const UAnimMontage* Montage, const UASItemDefinition* Definition, const bool bScaleToInterval) const
{
	if (!bScaleToInterval || !Montage || !Definition || Definition->AttackInterval <= 0.0f) return 1.0f;
	return FMath::Max(1.0f, Montage->GetPlayLength() / Definition->AttackInterval);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnHitEvent(FGameplayEventData Payload)
{
	PerformHit();
}

void UASGameplayAbility_CharacterPrimaryAttack::OnMontageFinished()
{
	if (!bHitPerformed && CurrentActorInfo && CurrentActorInfo->IsNetAuthority()) PerformHit();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
