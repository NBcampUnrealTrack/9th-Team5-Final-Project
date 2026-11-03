#include "Items/GAS/ASTargetedItemUseAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

UASTargetedItemUseAbility::UASTargetedItemUseAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void UASTargetedItemUseAbility::StartTargeting(float Timeout, float UpdateInterval, bool bRecheckCost)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();
	if (!CurrentActorInfo || !World || !ASC || !Avatar || !CurrentActorInfo->PlayerController.IsValid()
		|| !FMath::IsFinite(Timeout) || Timeout <= 0.0f || !FMath::IsFinite(UpdateInterval) || UpdateInterval <= 0.0f)
	{
		OnLocalCancel();
		return;
	}
	bTargeting = true;
	bSubmittedTarget = false;
	TargetingAvatar = Avatar;
	ActiveTargetingTimeout = Timeout;
	bRecheckPendingCost = bRecheckCost;
	TargetingStartedAt = World->GetTimeSeconds();
	OtherAbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::OnOtherAbilityActivated);
	World->GetTimerManager().SetTimer(PendingUseTimer, this, &ThisClass::CheckPendingUse, UpdateInterval, true);
	if (CurrentActorInfo->IsLocallyControlled())
	{
		BeginTargeting();
	}
	else if (CurrentActorInfo->IsNetAuthority())
	{
		const FPredictionKey Key = CurrentActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, Key).AddUObject(this, &ThisClass::OnTargetDataReceived);
		ASC->AbilityTargetDataCancelledDelegate(CurrentSpecHandle, Key).AddUObject(this, &ThisClass::OnTargetDataCancelled);
		ASC->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, Key);
	}
	else
	{
		OnLocalCancel();
	}
}

void UASTargetedItemUseAbility::BindTargetingInput()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Targeting);
	bAddedTargetingTag = true;
	ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &ThisClass::OnLocalConfirm);
	ASC->GenericLocalCancelCallbacks.AddDynamic(this, &ThisClass::OnLocalCancel);
}

void UASTargetedItemUseAbility::OnLocalConfirm()
{
	if (!bTargeting || bSubmittedTarget) return;
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	FVector CursorLocation;
	const AASPlayerController* PC = Cast<AASPlayerController>(CurrentActorInfo->PlayerController.Get());
	if ((PC && PC->GetItemInteractionComponent()->IsCursorOverInventory())
		|| !AASSkillRangeIndicatorActor::GetCursorWorldLocation(Avatar, CursorLocation)
		|| !IsTargetLocationValid(CursorLocation))
	{
		// LocalInputConfirm clears callbacks before broadcasting, so invalid clicks must rebind.
		GetAbilitySystemComponentFromActorInfo()->GenericLocalConfirmCallbacks.AddUniqueDynamic(this, &ThisClass::OnLocalConfirm);
		return;
	}
	SubmitTargetLocation(AASSkillRangeIndicatorActor::ResolveTargetLocation(Avatar, CursorLocation, GetTargetRange()));
}

void UASTargetedItemUseAbility::SubmitTargetLocation(const FVector& Location)
{
	bSubmittedTarget = true;
	StopTargeting();
	if (CurrentActorInfo->IsNetAuthority())
	{
		ExecuteAtLocation(Location);
		return;
	}
	FGameplayAbilityTargetDataHandle Data;
	FGameplayAbilityTargetData_LocationInfo* Target = new FGameplayAbilityTargetData_LocationInfo();
	Target->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Target->TargetLocation.LiteralTransform = FTransform(Location);
	Data.Add(Target);
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
		Data, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UASTargetedItemUseAbility::OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	// Copy before consuming: Data may be a reference into the ASC's target-data cache.
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
	const FVector Location = Target->GetEndPointTransform().GetLocation();
	bSubmittedTarget = true;
	StopTargeting();
	// Concrete execution still validates world placement/range and commits inventory cost on authority.
	ExecuteAtLocation(Location);
}

void UASTargetedItemUseAbility::OnLocalCancel()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UASTargetedItemUseAbility::OnTargetDataCancelled()
{
	OnLocalCancel();
}

void UASTargetedItemUseAbility::OnOtherAbilityActivated(UGameplayAbility* Ability)
{
	if (bTargeting && Ability != this) OnLocalCancel();
}

void UASTargetedItemUseAbility::CheckPendingUse()
{
	const UASItemInstance* Item = GetSourceItem();
	const UASInventoryComponent* Inventory = Item ? Item->GetTypedOuter<UASInventoryComponent>() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();
	if (!Item || !Inventory || Inventory->FindItemById(Item->GetInstanceId()) != Item
		|| !TargetingAvatar.IsValid() || GetAvatarActorFromActorInfo() != TargetingAvatar.Get()
		|| !ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead) || !World
		|| (bRecheckPendingCost && CurrentActorInfo->IsNetAuthority() && !CheckCost(CurrentSpecHandle, CurrentActorInfo))
		|| World->GetTimeSeconds() - TargetingStartedAt >= ActiveTargetingTimeout)
	{
		OnLocalCancel();
		return;
	}
	UpdateTargetingPreview();
}

void UASTargetedItemUseAbility::StopTargeting()
{
	bTargeting = false;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &ThisClass::OnLocalConfirm);
		ASC->GenericLocalCancelCallbacks.RemoveDynamic(this, &ThisClass::OnLocalCancel);
		if (bAddedTargetingTag) ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Targeting);
	}
	bAddedTargetingTag = false;
	CleanupTargetingVisuals();
}

void UASTargetedItemUseAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (!IsActive()) return;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC)
	{
		if (bWasCancelled && !bSubmittedTarget && !ActorInfo->IsNetAuthority())
			ASC->ServerSetReplicatedTargetDataCancelled(Handle, ActivationInfo.GetActivationPredictionKey(), ASC->ScopedPredictionKey);
		const FPredictionKey Key = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, Key).RemoveAll(this);
		ASC->AbilityTargetDataCancelledDelegate(Handle, Key).RemoveAll(this);
		ASC->ConsumeClientReplicatedTargetData(Handle, Key);
		ASC->AbilityActivatedCallbacks.Remove(OtherAbilityActivatedHandle);
	}
	OtherAbilityActivatedHandle.Reset();
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PendingUseTimer);
	StopTargeting();
	TargetingAvatar.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
