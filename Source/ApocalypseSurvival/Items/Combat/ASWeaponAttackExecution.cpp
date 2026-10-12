#include "Items/Combat/ASWeaponAttackExecution.h"

#include "AbilitySystemComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Skills/ASSkillAbility.h"

namespace ASWeaponAttackExecutionPrivate
{
	float GetStaminaCost(const FASWeaponAttackExecutionContext& Context)
	{
		const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
		return Profile ? FMath::Max(0.0f, Profile->StaminaCost) : 0.0f;
	}

	UASItemDefinition* GetAmmoDefinition(const FASWeaponAttackExecutionContext& Context)
	{
		const UASRangedAttackProfile* Profile = Cast<UASRangedAttackProfile>(Context.Profile);
		return Profile ? Profile->AmmoDefinition.Get() : nullptr;
	}

	int32 GetAmmoPerShot(const FASWeaponAttackExecutionContext& Context)
	{
		const UASRangedAttackProfile* Profile = Cast<UASRangedAttackProfile>(Context.Profile);
		return Profile ? Profile->AmmoPerShot : 0;
	}

	void ApplyDamage(const FASWeaponAttackExecutionContext& Context, const TArray<AActor*>& Targets)
	{
		const TArray<FGameplayEffectSpecHandle> DamageSpecs = UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(
			Context.Weapon, Context.AbilitySystem);
		for (AActor* Target : Targets)
		{
			for (const FGameplayEffectSpecHandle& DamageSpec : DamageSpecs)
			{
				UASSkillAbility::ApplyDamageSpecToTarget(DamageSpec, Target);
			}
		}
	}
}

bool UASMeleeAttackExecution::CanPayCost(const FASWeaponAttackExecutionContext& Context) const
{
	const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
	return Profile
		&& Context.Definition && Context.Definition->WeaponType == EASWeaponType::Melee
		&& IsValid(Context.AbilitySystem)
		&& Profile->StaminaCostEffect
		&& Context.AbilitySystem->GetNumericAttribute(UASPlayerAttributeSet::GetStaminaAttribute())
			>= ASWeaponAttackExecutionPrivate::GetStaminaCost(Context);
}

bool UASMeleeAttackExecution::CommitCost(const FASWeaponAttackExecutionContext& Context) const
{
	if (!CanPayCost(Context) || !IsValid(Context.AbilitySystem) || !IsValid(Context.Weapon)) return false;
	const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
	if (!Profile) return false;
	FGameplayEffectContextHandle EffectContext = Context.AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(Context.Weapon);
	const FGameplayEffectSpecHandle CostSpec = Context.AbilitySystem->MakeOutgoingSpec(
		Profile->StaminaCostEffect, 1.0f, EffectContext);
	if (!CostSpec.IsValid()) return false;
	const FGameplayTag StaminaCostTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.StaminaCost")), false);
	if (!StaminaCostTag.IsValid()) return false;
	CostSpec.Data->SetSetByCallerMagnitude(StaminaCostTag, -ASWeaponAttackExecutionPrivate::GetStaminaCost(Context));
	Context.AbilitySystem->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());

	if (ASWeaponAttackExecutionPrivate::GetStaminaCost(Context) > 0.0f
		&& Profile->StaminaRegenDelayEffect)
	{
		const FGameplayEffectSpecHandle DelaySpec = Context.AbilitySystem->MakeOutgoingSpec(
			Profile->StaminaRegenDelayEffect, 1.0f, EffectContext);
		if (DelaySpec.IsValid())
		{
			Context.AbilitySystem->ApplyGameplayEffectSpecToSelf(*DelaySpec.Data.Get());
		}
	}
	return true;
}

void UASMeleeAttackExecution::ExecuteHit(const FASWeaponAttackExecutionContext& Context) const
{
	if (!Context.Avatar || !Context.Definition || !Context.Avatar->GetWorld()) return;
	const UASMeleeAttackProfile* Profile = Cast<UASMeleeAttackProfile>(Context.Profile);
	if (!Profile) return;
	const float AttackRange = FMath::Max(1.0f, Context.Definition->AttackRange);
	const float Radius = Profile->HitRadius > 0.0f
		? Profile->HitRadius : FMath::Max(50.0f, AttackRange * 0.5f);
	const float Offset = Profile->HitboxOffset > 0.0f
		? Profile->HitboxOffset : Radius * 2.0f;
	const FVector Start = Context.Avatar->GetActorLocation();
	const FVector Center = Start + Context.AimDirection * Offset;
	const TArray<AActor*> Targets = UASSkillAbility::FindSkillTargets(
		Context.Avatar->GetWorld(), Center, Radius, Context.Avatar, false);
	ASWeaponAttackExecutionPrivate::ApplyDamage(Context, Targets);

	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		Character->MulticastDrawWeaponAttackDebug(Start, Center, Radius, true, !Targets.IsEmpty());
	}
}

bool UASRangedAttackExecution::CanPayCost(const FASWeaponAttackExecutionContext& Context) const
{
	UASItemDefinition* Ammo = ASWeaponAttackExecutionPrivate::GetAmmoDefinition(Context);
	const int32 Quantity = ASWeaponAttackExecutionPrivate::GetAmmoPerShot(Context);
	return Cast<UASRangedAttackProfile>(Context.Profile)
		&& Context.Definition && Context.Definition->WeaponType == EASWeaponType::Ranged
		&& IsValid(Context.Inventory) && IsValid(Ammo) && Ammo->ItemType == EASItemType::Ammo
		&& Quantity > 0 && Context.Inventory->HasItem(Ammo, Quantity);
}

bool UASRangedAttackExecution::CommitCost(const FASWeaponAttackExecutionContext& Context) const
{
	UASItemDefinition* Ammo = ASWeaponAttackExecutionPrivate::GetAmmoDefinition(Context);
	const int32 Quantity = ASWeaponAttackExecutionPrivate::GetAmmoPerShot(Context);
	return CanPayCost(Context) && Context.Inventory->TryConsumeItem(Ammo, Quantity);
}

void UASRangedAttackExecution::ExecuteHit(const FASWeaponAttackExecutionContext& Context) const
{
	UWorld* World = Context.Avatar ? Context.Avatar->GetWorld() : nullptr;
	if (!World || !Context.Definition) return;
	const UASRangedAttackProfile* Profile = Cast<UASRangedAttackProfile>(Context.Profile);
	if (!Profile) return;
	const float TraceRadius = FMath::Max(0.0f, Profile->TraceRadius);
	const float RangeMultiplier = FMath::Max(1.0f, Profile->RangeMultiplier);
	const FVector Start = Context.Avatar->GetActorLocation();
	const FVector End = Start + Context.AimDirection * FMath::Max(1.0f, Context.Definition->AttackRange) * RangeMultiplier;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponAttack), false, Context.Avatar);
	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(TraceRadius), QueryParams);

	AActor* FirstTarget = nullptr;
	float FirstHitTime = 1.0f;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (UASSkillAbility::IsValidSkillTarget(Target, Context.Avatar, false)
			&& (!FirstTarget || Hit.Time < FirstHitTime))
		{
			FirstTarget = Target;
			FirstHitTime = Hit.Time;
		}
	}

	TArray<AActor*> Targets;
	if (FirstTarget) Targets.Add(FirstTarget);
	ASWeaponAttackExecutionPrivate::ApplyDamage(Context, Targets);
	if (AASPlayerCharacter* Character = Cast<AASPlayerCharacter>(Context.Avatar))
	{
		const FVector DebugEnd = FirstTarget ? Start + (End - Start) * FirstHitTime : End;
		Character->MulticastDrawWeaponAttackDebug(Start, DebugEnd, TraceRadius, false, FirstTarget != nullptr);
	}
}
