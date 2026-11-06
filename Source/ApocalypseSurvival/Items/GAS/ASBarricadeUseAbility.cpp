#include "Items/GAS/ASBarricadeUseAbility.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Data/ASItemTags.h"
#include "Items/Use/ASBarricadeActor.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"

UASBarricadeUseAbility::UASBarricadeUseAbility()
{
	BarricadeClass = AASBarricadeActor::StaticClass();
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
	if (!World || !ASC || !Definition || !ActorInfo->PlayerController.IsValid() || !BarricadeClass.Get()
		|| BarricadeClass.Get()->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsFinite(Range) || Range <= 0.0f
		|| !FMath::IsFinite(Health) || Health <= 0.0f
		|| !FMath::IsFinite(Lifetime) || Lifetime < 0.0f
		|| !FMath::IsFinite(TargetingTimeout) || TargetingTimeout <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StartTargeting(TargetingTimeout, 0.03f, true);
}

void UASBarricadeUseAbility::BeginTargeting()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	ASC->LocalInputCancel();
	if (APlayerController* PC = CurrentActorInfo->PlayerController.Get()) PC->StopMovement();
	PreviewActor = GetWorld()->SpawnActorDeferred<AASBarricadeActor>(BarricadeClass.Get(), Avatar->GetActorTransform(),
		Avatar, Avatar, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PreviewActor)
	{
		OnLocalCancel();
		return;
	}
	PreviewActor->SetReplicates(false);
	PreviewActor->FinishSpawning(Avatar->GetActorTransform());
	UpdatePlacementPreview();
	BindTargetingInput();
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
	AASBarricadeActor* Barricade = World->SpawnActor<AASBarricadeActor>(BarricadeClass.Get(), Avatar->GetActorTransform(), Params);
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


float UASBarricadeUseAbility::GetTargetRange() const
{
	return GetSourceItemDefinition()->GetEffectMagnitude(ASGameplayTags::Data_Item_PlacementRange, ASItemMagnitudeDefaults::PlacementRange);
}

void UASBarricadeUseAbility::ExecuteAtLocation(const FVector& Location)
{
	PlaceAtLocation(Location);
}

bool UASBarricadeUseAbility::IsTargetLocationValid(const FVector& Location) const
{
	FTransform Placement;
	return IsValid(PreviewActor) && PreviewActor->TryGetPlacementTransform(
		Cast<APawn>(GetAvatarActorFromActorInfo()), Location, GetTargetRange(), Placement);
}

void UASBarricadeUseAbility::UpdateTargetingPreview()
{
	UpdatePlacementPreview();
}

void UASBarricadeUseAbility::CleanupTargetingVisuals()
{
	if (IsValid(PreviewActor)) PreviewActor->Destroy();
	PreviewActor = nullptr;
}
