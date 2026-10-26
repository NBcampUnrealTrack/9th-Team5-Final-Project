#include "Skills/Melee/ASSkillMelee.h"

#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Skills/Data/ASSkillData.h"
#include "Engine/World.h"

UASSkillMelee::UASSkillMelee()
{
	ImpactPreset = EASImpactPreset::Standard;
}

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
	TArray<FGameplayEffectSpecHandle> EffectSpecs;
	const FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
	if (DamageSpec.IsValid())
	{
		EffectSpecs.Add(DamageSpec);
	}

	FASCombatApplyResult ApplyResult;
	if (UASCombatTargetSubsystem* CombatTargets = World->GetSubsystem<UASCombatTargetSubsystem>())
	{
		FASCombatTargetQuery Query;
		Query.Shape = EASCombatTargetShape::Cone;
		Query.SearchOrigin = Center;
		Query.Direction = Aim;
		Query.SearchRadius = Radius;
		Query.AngleDegrees = Angle;
		Query.Instigator = Avatar;
		Query.bCanHitPlayers = bCanHitPlayers;

		FASCombatImpactContext ImpactContext;
		ImpactContext.Preset = ImpactPreset;
		ImpactContext.HitLocation = Center;
		ImpactContext.HitDirection = Aim;
		ApplyResult = CombatTargets->ResolveAndApply(Query, EffectSpecs, ImpactContext);
	}

	if (bDrawDebug)
	{
		const FColor Color = ApplyResult.HasAnyHit() ? FColor::Red : FColor::Green;

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

}

void UASSkillMelee::OnTargetsHit_Implementation(const TArray<AActor*>& Targets)
{
	for (AActor* Target : Targets)
	{
		UASCombatTargetSubsystem::ApplyEffectSpecToActor(MakeDamageSpec(), Target);
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
