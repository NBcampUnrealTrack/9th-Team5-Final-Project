#include "Core/Combat/ASCombatImpactSettings.h"

EASResolvedHitReaction UASCombatImpactSettings::ResolveMonsterReaction(
	const EASImpactPreset ImpactPreset,
	const EASStaggerResistance Resistance,
	const bool bHasSuperArmor) const
{
	if (ImpactPreset == EASImpactPreset::None || bHasSuperArmor)
	{
		return EASResolvedHitReaction::VisualOnly;
	}

	switch (ImpactPreset)
	{
	case EASImpactPreset::Standard:
	case EASImpactPreset::Rapid:
		return Resistance == EASStaggerResistance::Normal
			? EASResolvedHitReaction::WeakStagger
			: EASResolvedHitReaction::VisualOnly;
	case EASImpactPreset::Heavy:
		switch (Resistance)
		{
		case EASStaggerResistance::Normal:
			return EASResolvedHitReaction::StrongStagger;
		case EASStaggerResistance::Elite:
			return EASResolvedHitReaction::WeakStagger;
		case EASStaggerResistance::Boss:
		default:
			return EASResolvedHitReaction::VisualOnly;
		}
	case EASImpactPreset::Knockdown:
		switch (Resistance)
		{
		case EASStaggerResistance::Normal:
			return EASResolvedHitReaction::Knockdown;
		case EASStaggerResistance::Elite:
			return EASResolvedHitReaction::StrongStagger;
		case EASStaggerResistance::Boss:
		default:
			return EASResolvedHitReaction::WeakStagger;
		}
	case EASImpactPreset::None:
	default:
		return EASResolvedHitReaction::VisualOnly;
	}
}

EASResolvedHitReaction UASCombatImpactSettings::ResolvePlayerReaction(
	const EASImpactPreset ImpactPreset,
	const bool bHasSuperArmor,
	const bool bHasStaggerProtection) const
{
	if (ImpactPreset != EASImpactPreset::Heavy && ImpactPreset != EASImpactPreset::Knockdown)
	{
		return EASResolvedHitReaction::VisualOnly;
	}

	if (bHasSuperArmor || bHasStaggerProtection)
	{
		return EASResolvedHitReaction::VisualOnly;
	}

	return ImpactPreset == EASImpactPreset::Heavy
		? EASResolvedHitReaction::StrongStagger
		: EASResolvedHitReaction::Knockdown;
}

float UASCombatImpactSettings::GetMonsterReactionDuration(const EASResolvedHitReaction Reaction) const
{
	switch (Reaction)
	{
	case EASResolvedHitReaction::WeakStagger:
		return FMath::Max(0.0f, WeakStaggerDuration);
	case EASResolvedHitReaction::StrongStagger:
		return FMath::Max(0.0f, StrongStaggerDuration);
	case EASResolvedHitReaction::Knockdown:
		return FMath::Max(0.0f, KnockdownDuration);
	case EASResolvedHitReaction::None:
	case EASResolvedHitReaction::VisualOnly:
	default:
		return 0.0f;
	}
}

float UASCombatImpactSettings::GetPlayerReactionDuration(const EASResolvedHitReaction Reaction) const
{
	switch (Reaction)
	{
	case EASResolvedHitReaction::StrongStagger:
		return FMath::Max(0.0f, PlayerStaggerDuration);
	case EASResolvedHitReaction::Knockdown:
		return FMath::Max(0.0f, PlayerKnockdownDuration);
	case EASResolvedHitReaction::None:
	case EASResolvedHitReaction::VisualOnly:
	case EASResolvedHitReaction::WeakStagger:
	default:
		return 0.0f;
	}
}

float UASCombatImpactSettings::GetMeleeHitStopDuration(const EASImpactPreset ImpactPreset) const
{
	switch (ImpactPreset)
	{
	case EASImpactPreset::Standard:
		return FMath::Max(0.0f, MeleeStandardHitStop);
	case EASImpactPreset::Heavy:
		return FMath::Max(0.0f, MeleeHeavyHitStop);
	case EASImpactPreset::Knockdown:
		return FMath::Max(0.0f, MeleeKnockdownHitStop);
	case EASImpactPreset::None:
	case EASImpactPreset::Rapid:
	default:
		return 0.0f;
	}
}
