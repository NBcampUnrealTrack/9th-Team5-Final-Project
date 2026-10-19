#include "Skills/Melee/ASSkillMelee.h"

#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Skills/Data/ASSkillData.h"
#include "Engine/World.h"

void UASSkillMelee::OnSkillEffect_Implementation()
{
	if (!HasSkillAuthority())
	{
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FVector Aim = GetAimDirection().GetSafeNormal2D();
	if (Aim.IsNearlyZero())
	{
		Aim = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector Center = Avatar->GetActorLocation() + Aim * ForwardOffset;
	const bool bIsCone = Angle < 360.0f;

	TArray<AActor*> Targets = FindSkillTargets(World, Center, Radius, Avatar, bCanHitPlayers);

	if (bIsCone)
	{
		const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Angle * 0.5f));

		Targets.RemoveAll([&](const AActor* Target)
		{
			const FVector ToTarget = (Target->GetActorLocation() - Center).GetSafeNormal2D();
			return !ToTarget.IsNearlyZero() && FVector::DotProduct(Aim, ToTarget) < CosHalfAngle;
		});
	}

	int32 NonActorHitCount = 0;
	if (!bCanHitPlayers)
	{
		if (UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>())
		{
			TArray<FGameplayEffectSpecHandle> EffectSpecs;
			const FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
			if (DamageSpec.IsValid())
			{
				EffectSpecs.Add(DamageSpec);
			}
			NonActorHitCount = CombatTargets->ApplyEffectSpecsToNonActorTargetsInCone(
				Center, Aim, Radius, Angle, EffectSpecs, const_cast<AActor*>(Avatar));
		}
	}

	if (bDrawDebug)
	{
		const FColor Color = Targets.Num() > 0 || NonActorHitCount > 0 ? FColor::Red : FColor::Green;

		if (bIsCone)
		{
			const float HalfAngleRad = FMath::DegreesToRadians(Angle * 0.5f);
			DrawDebugCone(World, Center, Aim, Radius, HalfAngleRad, 0.05f, 16, Color, false, 1.0f);
		}
		else
		{
			DrawDebugSphere(World, Center, Radius, 16, Color, false, 1.0f);
		}
	}

	if (Targets.Num() > 0)
	{
		OnTargetsHit(Targets);
	}
}

void UASSkillMelee::OnTargetsHit_Implementation(const TArray<AActor*>& Targets)
{
	for (AActor* Target : Targets)
	{
		ApplySkillDamage(Target);
	}
}

void UASSkillMelee::ApplySkillStats(const FASSkillStatsRow& Stats)
{
	Super::ApplySkillStats(Stats);

	Radius = Stats.MeleeRadius;
	Angle = Stats.Angle;
	ForwardOffset = Stats.ForwardOffset;
}

FASSkillIndicatorSettings UASSkillMelee::GetIndicatorSettings_Implementation() const
{
	FASSkillIndicatorSettings Settings = Super::GetIndicatorSettings_Implementation();
	Settings.Shape = Angle < 360.0f ? EASSkillIndicatorShape::Cone : EASSkillIndicatorShape::Circle;
	Settings.Radius = Radius;
	Settings.Angle = Angle;
	Settings.ForwardOffset = ForwardOffset;

	//닿는 거리를 사거리 원으로 표시
	if (Settings.TargetingType != EASSkillTargetingType::Location)
	{
		Settings.RangeRingRadius = FMath::Max(0.0f, ForwardOffset) + Radius;
	}

	return Settings;
}
