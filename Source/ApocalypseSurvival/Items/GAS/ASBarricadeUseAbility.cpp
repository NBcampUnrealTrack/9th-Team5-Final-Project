#include "Items/GAS/ASBarricadeUseAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Data/ASItemTags.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Items/Interaction/ASItemInteractionComponent.h"
#include "Items/Use/ASBarricadeActor.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"
#include "TimerManager.h"

UASBarricadeUseAbility::UASBarricadeUseAbility()
{
	BarricadeClass = AASBarricadeActor::StaticClass();
	// Inventory requests still start on the server; the owning client also runs the targeting UI.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void UASBarricadeUseAbility::ActivateItemUse(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	APawn* Avatar = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	const float Range = Definition ? Definition->GetEffectMagnitude(
		ASGameplayTags::Data_Item_PlacementRange, ASItemMagnitudeDefaults::PlacementRange) : 0.0f;
	const float Health = Definition ? Definition->GetEffectMagnitude(
		ASGameplayTags::Data_Item_BarricadeHealth, ASItemMagnitudeDefaults::BarricadeHealth) : 0.0f;
	const float Lifetime = Definition ? Definition->GetEffectMagnitude(
		ASGameplayTags::Data_Item_BarricadeLifetime, ASItemMagnitudeDefaults::BarricadeLifetime) : 0.0f;
	if (!World || !ASC || !Definition || !ActorInfo->PlayerController.IsValid() || !BarricadeClass
		|| BarricadeClass->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(Range) || Range <= 0.0f
		|| !FMath::IsFinite(Health) || Health <= 0.0f
		|| !FMath::IsFinite(Lifetime) || Lifetime < 0.0f
		|| !FMath::IsFinite(TargetingTimeout) || TargetingTimeout <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bTargeting = true;
	bSubmittedTarget = false;
	TargetingAvatar = Avatar;
	TargetingStartedAt = World->GetTimeSeconds();
	OtherAbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::OnOtherAbilityActivated);
	World->GetTimerManager().SetTimer(PendingUseTimer, this, &ThisClass::CheckPendingUse, 0.03f, true);
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

void UASBarricadeUseAbility::BeginTargeting()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	ASC->LocalInputCancel();
	if (APlayerController* PC = CurrentActorInfo->PlayerController.Get()) PC->StopMovement();
	PreviewActor = GetWorld()->SpawnActorDeferred<AASBarricadeActor>(BarricadeClass, Avatar->GetActorTransform(),
		Avatar, Avatar, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PreviewActor)
	{
		OnLocalCancel();
		return;
	}
	PreviewActor->SetReplicates(false);
	PreviewActor->FinishSpawning(Avatar->GetActorTransform());
	UpdatePlacementPreview();
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Targeting);
	bAddedTargetingTag = true;
	ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &ThisClass::OnLocalConfirm);
	ASC->GenericLocalCancelCallbacks.AddDynamic(this, &ThisClass::OnLocalCancel);
}

void UASBarricadeUseAbility::UpdatePlacementPreview()
{
	if (!IsValid(PreviewActor) || !TargetingAvatar.IsValid()) return;
	APawn* Avatar = TargetingAvatar.Get();
	const float Range = GetSourceItemDefinition()->GetEffectMagnitude(
		ASGameplayTags::Data_Item_PlacementRange, ASItemMagnitudeDefaults::PlacementRange);
	FVector CursorLocation;
	const bool bHasCursor = AASSkillRangeIndicatorActor::GetCursorWorldLocation(Avatar, CursorLocation);
	if (!bHasCursor) CursorLocation = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * Range;
	FTransform Placement;
	const bool bCanPlace = PreviewActor->TryGetPlacementTransform(Avatar, CursorLocation, Range, Placement);
	if (!bCanPlace)
	{
		FVector Location = AASSkillRangeIndicatorActor::ResolveTargetLocation(Avatar, CursorLocation, Range);
		const FVector Direction = (Location - Avatar->GetActorLocation()).GetSafeNormal2D();
		Location.Z += PreviewActor->FindComponentByClass<UBoxComponent>()->GetScaledBoxExtent().Z + 1.0f;
		Placement = FTransform(Direction.IsNearlyZero() ? Avatar->GetActorRotation() : Direction.Rotation(), Location);
	}
	PreviewActor->SetActorTransform(Placement);
	PreviewActor->SetPlacementPreview(bHasCursor && bCanPlace);
}

void UASBarricadeUseAbility::OnLocalConfirm()
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
	FTransform Placement;
	const float Range = GetSourceItemDefinition()->GetEffectMagnitude(
		ASGameplayTags::Data_Item_PlacementRange, ASItemMagnitudeDefaults::PlacementRange);
	if (!IsValid(PreviewActor) || !PreviewActor->TryGetPlacementTransform(Avatar, CursorLocation, Range, Placement))
	{
		GetAbilitySystemComponentFromActorInfo()->GenericLocalConfirmCallbacks.AddUniqueDynamic(this, &ThisClass::OnLocalConfirm);
		return;
	}
	const FVector TargetLocation = AASSkillRangeIndicatorActor::ResolveTargetLocation(Avatar, CursorLocation, Range);
	bSubmittedTarget = true;
	StopTargeting();
	if (CurrentActorInfo->IsNetAuthority())
	{
		PlaceAtLocation(TargetLocation);
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

void UASBarricadeUseAbility::OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
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
	PlaceAtLocation(Target->GetEndPointTransform().GetLocation());
}

void UASBarricadeUseAbility::PlaceAtLocation(const FVector& DesiredLocation)
{
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const UASItemDefinition* Definition = GetSourceItemDefinition();
	if (!World || !Definition || !CurrentActorInfo->IsNetAuthority() || DesiredLocation.ContainsNaN()
		|| !CheckCost(CurrentSpecHandle, CurrentActorInfo))
	{
		OnTargetDataCancelled();
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = Avatar;
	Params.Instigator = Avatar;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AASBarricadeActor* Barricade = World->SpawnActor<AASBarricadeActor>(BarricadeClass, Avatar->GetActorTransform(), Params);
	const float Range = Definition->GetEffectMagnitude(ASGameplayTags::Data_Item_PlacementRange, ASItemMagnitudeDefaults::PlacementRange);
	const float Health = Definition->GetEffectMagnitude(ASGameplayTags::Data_Item_BarricadeHealth, ASItemMagnitudeDefaults::BarricadeHealth);
	const float Lifetime = Definition->GetEffectMagnitude(ASGameplayTags::Data_Item_BarricadeLifetime, ASItemMagnitudeDefaults::BarricadeLifetime);
	FTransform Placement;
	if (!IsValid(Barricade) || !Barricade->TryGetPlacementTransform(Avatar, DesiredLocation, Range, Placement)
		|| !Barricade->PreparePlacement(Health, Lifetime))
	{
		if (IsValid(Barricade)) Barricade->Destroy();
		OnTargetDataCancelled();
		return;
	}
	Barricade->SetActorTransform(Placement);
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		Barricade->Destroy();
		OnTargetDataCancelled();
		return;
	}
	Barricade->FinishPlacement();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UASBarricadeUseAbility::OnLocalCancel()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UASBarricadeUseAbility::OnTargetDataCancelled()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UASBarricadeUseAbility::OnOtherAbilityActivated(UGameplayAbility* Ability)
{
	if (bTargeting && Ability != this) OnLocalCancel();
}

void UASBarricadeUseAbility::CheckPendingUse()
{
	const UASItemInstance* Item = GetSourceItem();
	const UASInventoryComponent* Inventory = Item ? Item->GetTypedOuter<UASInventoryComponent>() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Item || !Inventory || Inventory->FindItemById(Item->GetInstanceId()) != Item
		|| !TargetingAvatar.IsValid() || GetAvatarActorFromActorInfo() != TargetingAvatar.Get()
		|| !ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead)
		|| (CurrentActorInfo->IsNetAuthority() && !CheckCost(CurrentSpecHandle, CurrentActorInfo))
		|| GetWorld()->GetTimeSeconds() - TargetingStartedAt >= TargetingTimeout)
	{
		OnLocalCancel();
		return;
	}
	UpdatePlacementPreview();
}

void UASBarricadeUseAbility::StopTargeting()
{
	bTargeting = false;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &ThisClass::OnLocalConfirm);
		ASC->GenericLocalCancelCallbacks.RemoveDynamic(this, &ThisClass::OnLocalCancel);
		if (bAddedTargetingTag) ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Targeting);
	}
	bAddedTargetingTag = false;
	if (IsValid(PreviewActor)) PreviewActor->Destroy();
	PreviewActor = nullptr;
}

void UASBarricadeUseAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
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
