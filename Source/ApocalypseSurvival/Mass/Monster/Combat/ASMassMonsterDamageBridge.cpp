#include "Mass/Monster/Combat/ASMassMonsterDamageBridge.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/ASAbilityTags.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Mass/Monster/Combat/ASMassMonsterDamageEffect.h"
#include "Mass/Monster/Combat/ASMonsterDamageable.h"
#include "Skills/ASSkillTags.h"

ASMassMonsterDamage::EApplyResult ASMassMonsterDamage::ApplyToActor(
	AActor* TargetActor,
	const float Damage,
	const FVector& SourceLocation,
	const FVector& HitLocation,
	AActor* DamageCauser)
{
	if (!IsValid(TargetActor) || !TargetActor->HasAuthority() || Damage <= 0.0f)
	{
		return EApplyResult::InvalidTarget;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (IsValid(TargetASC))
	{
		if (TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
		{
			return EApplyResult::InvalidTarget;
		}

		if (TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_Invulnerable))
		{
			return EApplyResult::Blocked;
		}

		const UGameplayEffect* DamageEffect = GetDefault<UASMassMonsterDamageEffect>();
		FGameplayEffectContextHandle EffectContext(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
		EffectContext.AddOrigin(SourceLocation);

		FGameplayEffectSpec DamageSpec(DamageEffect, EffectContext, 1.0f);
		DamageSpec.SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -FMath::Max(0.0f, Damage));
		DamageSpec.AddDynamicAssetTag(ASGameplayTags::Effect_Damage);
		TargetASC->ApplyGameplayEffectSpecToSelf(DamageSpec);
		return EApplyResult::Applied;
	}

	if (!TargetActor->GetClass()->ImplementsInterface(UASMonsterDamageable::StaticClass())
		|| !IASMonsterDamageable::Execute_CanReceiveMonsterDamage(TargetActor)
		|| !IASMonsterDamageable::Execute_IsAliveForMonsterTargeting(TargetActor))
	{
		return EApplyResult::InvalidTarget;
	}

	FASMonsterDamageContext DamageContext;
	DamageContext.Damage = Damage;
	DamageContext.DamageOrigin = SourceLocation;
	DamageContext.HitLocation = HitLocation.IsNearlyZero() ? TargetActor->GetActorLocation() : HitLocation;
	DamageContext.DamageCauser = DamageCauser;
	IASMonsterDamageable::Execute_ApplyMonsterDamage(TargetActor, DamageContext);
	return EApplyResult::Applied;
}
