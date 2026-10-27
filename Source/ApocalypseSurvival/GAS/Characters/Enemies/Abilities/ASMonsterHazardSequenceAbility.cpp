#include "GAS/Characters/Enemies/Abilities/ASMonsterHazardSequenceAbility.h"

#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

UASMonsterHazardSequenceAbility::UASMonsterHazardSequenceAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = false;
	ImpactPreset = EASImpactPreset::Standard;
	BaseDamage = 0.0f;
	AttackPowerScale = 1.0f;
	AttackPowerAttribute = UASCombatAttributeSet::GetAttackPowerAttribute();
	IndicatorClass = AASMonsterAttackIndicatorActor::StaticClass();
	CompletionEventTag = ASGameplayTags::Event_Monster_ComboStep_HazardSequenceFinished;

	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack_HazardSequence);
	SetAssetTags(AssetTagsToSet);

	FASMonsterHazardWave InitialCircleWave;
	InitialCircleWave.Shape = EASMonsterHazardShape::Circle;
	InitialCircleWave.Count = 6;
	InitialCircleWave.TelegraphDuration = 1.0f;
	InitialCircleWave.NextWaveDelay = 0.2f;
	InitialCircleWave.CircleRadius = 180.0f;
	InitialCircleWave.MinPlacementRadius = 250.0f;
	InitialCircleWave.MaxPlacementRadius = 850.0f;
	Waves.Add(InitialCircleWave);
}

void UASMonsterHazardSequenceAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!HasSkillAuthority() || Waves.IsEmpty() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishComboStep(true);
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		FinishComboStep(true);
		return;
	}

	Character->GetCharacterMovement()->StopMovementImmediately();
	SequenceOrigin = Character->GetActorLocation();
	RandomStream.Initialize(FMath::Rand());
	CurrentWaveIndex = INDEX_NONE;
	CurrentWaveInstances.Reset();
	DestroyCurrentWaveIndicators();
	BeginNextWave();
}

void UASMonsterHazardSequenceAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(WaveTelegraphTimerHandle);
		Character->GetWorldTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	DestroyCurrentWaveIndicators();
	CurrentWaveInstances.Reset();
	CurrentWaveIndex = INDEX_NONE;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterHazardSequenceAbility::BeginNextWave()
{
	if (!IsActive())
	{
		return;
	}

	++CurrentWaveIndex;
	if (!Waves.IsValidIndex(CurrentWaveIndex))
	{
		FinishComboStep(false);
		return;
	}

	const FASMonsterHazardWave& Wave = Waves[CurrentWaveIndex];
	BuildCurrentWaveInstances(Wave);
	SpawnCurrentWaveIndicators(Wave);

	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().SetTimer(WaveTelegraphTimerHandle, this,
			&ThisClass::ExplodeCurrentWave, FMath::Max(0.05f, Wave.TelegraphDuration), false);
	}
	else
	{
		FinishComboStep(true);
	}
}

void UASMonsterHazardSequenceAbility::ExplodeCurrentWave()
{
	if (!IsActive() || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	const FASMonsterHazardWave& Wave = Waves[CurrentWaveIndex];
	DestroyCurrentWaveIndicators();
	ApplyCurrentWaveDamage(Wave);
	CurrentWaveInstances.Reset();

	if (!Waves.IsValidIndex(CurrentWaveIndex + 1))
	{
		FinishComboStep(false);
		return;
	}

	if (Wave.NextWaveDelay <= 0.0f)
	{
		BeginNextWave();
		return;
	}

	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().SetTimer(NextWaveTimerHandle, this,
			&ThisClass::BeginNextWave, Wave.NextWaveDelay, false);
	}
	else
	{
		FinishComboStep(true);
	}
}

void UASMonsterHazardSequenceAbility::BuildCurrentWaveInstances(const FASMonsterHazardWave& Wave)
{
	CurrentWaveInstances.Reset();
	const int32 SafeCount = FMath::Max(1, Wave.Count);
	const int32 MaxTotalHazards = FMath::Max(0, Wave.MaxTotalHazards);
	const float MinRadius = FMath::Max(0.0f, FMath::Min(Wave.MinPlacementRadius, Wave.MaxPlacementRadius));
	const float MaxRadius = FMath::Max(MinRadius, Wave.MaxPlacementRadius);
	const float RingStartAngle = RandomStream.FRandRange(0.0f, 360.0f);
	const bool bPerPlayerPlacement = Wave.Placement == EASMonsterHazardPlacement::PerPlayerRandomAnnulus
		|| Wave.Placement == EASMonsterHazardPlacement::PerPlayerPredictedPath
		|| Wave.Placement == EASMonsterHazardPlacement::PerPlayerSurround;

	auto HasReachedHazardLimit = [this, MaxTotalHazards]()
	{
		return MaxTotalHazards > 0 && CurrentWaveInstances.Num() >= MaxTotalHazards;
	};

	if (bPerPlayerPlacement)
	{
		const TArray<AActor*> Players = FindRelevantPlayers();
		for (AActor* Player : Players)
		{
			if (!IsValid(Player) || HasReachedHazardLimit())
			{
				break;
			}

			const FVector PlayerLocation = Player->GetActorLocation();
			FVector TravelDirection = Player->GetVelocity().GetSafeNormal2D();
			if (TravelDirection.IsNearlyZero())
			{
				TravelDirection = (PlayerLocation - SequenceOrigin).GetSafeNormal2D();
			}
			if (TravelDirection.IsNearlyZero())
			{
				TravelDirection = FVector::ForwardVector;
			}

			const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, TravelDirection);
			const FVector PredictedLocation = PlayerLocation + TravelDirection * Player->GetVelocity().Size2D()
				* FMath::Max(0.0f, Wave.PredictionLeadTime);
			const float SurroundStartAngle = RandomStream.FRandRange(0.0f, 360.0f);
			for (int32 Index = 0; Index < SafeCount && !HasReachedHazardLimit(); ++Index)
			{
				FHazardInstance& Instance = CurrentWaveInstances.AddDefaulted_GetRef();
				if (Wave.Placement == EASMonsterHazardPlacement::PerPlayerRandomAnnulus)
				{
					const float AngleDegrees = RandomStream.FRandRange(0.0f, 360.0f);
					const float DistanceSquared = FMath::Lerp(FMath::Square(MinRadius), FMath::Square(MaxRadius), RandomStream.FRand());
					Instance.Location = PlayerLocation + FRotator(0.0f, AngleDegrees, 0.0f).Vector() * FMath::Sqrt(DistanceSquared);
					Instance.Rotation = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
					continue;
				}

				if (Wave.Placement == EASMonsterHazardPlacement::PerPlayerSurround)
				{
					const float AngleStep = 360.0f / SafeCount;
					const float AngleOffset = RandomStream.FRandRange(-FMath::Max(0.0f, Wave.SurroundAngleJitterDegrees),
						FMath::Max(0.0f, Wave.SurroundAngleJitterDegrees));
					const float AngleDegrees = SurroundStartAngle + AngleStep * Index + AngleOffset;
					const float RadiusOffset = RandomStream.FRandRange(-FMath::Max(0.0f, Wave.SurroundRadiusJitter),
						FMath::Max(0.0f, Wave.SurroundRadiusJitter));
					const float PlacementRadius = FMath::Max(0.0f, Wave.SurroundRadius + RadiusOffset);
					const float RotationOffset = RandomStream.FRandRange(-FMath::Max(0.0f, Wave.RotationJitterDegrees),
						FMath::Max(0.0f, Wave.RotationJitterDegrees));
					Instance.Location = PlayerLocation + FRotator(0.0f, AngleDegrees, 0.0f).Vector() * PlacementRadius;
					Instance.Rotation = FRotator(0.0f, AngleDegrees + 45.0f + RotationOffset, 0.0f);
					continue;
				}

				const float SideIndex = Index - (SafeCount - 1) * 0.5f;
				const float JitterRadius = FMath::Sqrt(RandomStream.FRand()) * FMath::Max(0.0f, Wave.RandomJitter);
				const FVector Jitter = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f).Vector() * JitterRadius;
				const float ForwardOffset = RandomStream.FRandRange(-FMath::Max(0.0f, Wave.ForwardJitter),
					FMath::Max(0.0f, Wave.ForwardJitter));
				const float RotationOffset = RandomStream.FRandRange(-FMath::Max(0.0f, Wave.RotationJitterDegrees),
					FMath::Max(0.0f, Wave.RotationJitterDegrees));
				Instance.Location = PredictedLocation + RightDirection * (SideIndex * Wave.PathSpacing) + Jitter
					+ TravelDirection * ForwardOffset;
				Instance.Rotation = TravelDirection.Rotation() + FRotator(0.0f, RotationOffset, 0.0f);
			}
		}
		return;
	}

	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		if (HasReachedHazardLimit())
		{
			break;
		}
		float AngleDegrees = 0.0f;
		float Distance = 0.0f;
		if (Wave.Placement == EASMonsterHazardPlacement::Ring)
		{
			AngleDegrees = RingStartAngle + (360.0f * Index / SafeCount);
			Distance = MaxRadius;
		}
		else
		{
			AngleDegrees = RandomStream.FRandRange(0.0f, 360.0f);
			const float DistanceSquared = FMath::Lerp(FMath::Square(MinRadius), FMath::Square(MaxRadius), RandomStream.FRand());
			Distance = FMath::Sqrt(DistanceSquared);
		}

		FHazardInstance& Instance = CurrentWaveInstances.AddDefaulted_GetRef();
		Instance.Location = SequenceOrigin + FRotator(0.0f, AngleDegrees, 0.0f).Vector() * Distance;
		Instance.Rotation = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
	}
}

TArray<AActor*> UASMonsterHazardSequenceAbility::FindRelevantPlayers() const
{
	const ACharacter* Character = GetOwnerCharacter();
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	const UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!Character || !CombatTargets)
	{
		return {};
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Radius;
	Query.SearchOrigin = SequenceOrigin;
	Query.SearchRadius = FMath::Max(1.0f, PlayerSearchRadius);
	Query.Instigator = Character;
	Query.Domain = EASCombatTargetDomain::ActorOnly;
	Query.bCanHitPlayers = bCanHitPlayers;
	return CombatTargets->FindActorTargets(Query);
}

void UASMonsterHazardSequenceAbility::SpawnCurrentWaveIndicators(const FASMonsterHazardWave& Wave)
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || !IndicatorClass)
	{
		return;
	}

	auto SpawnIndicator = [this, Character, World, &Wave](const FVector& Location, const FRotator& Rotation,
		const EASMonsterAttackIndicatorShape Shape, const float Radius, const float Length, const float Width)
	{
		const FTransform SpawnTransform(Rotation, Location);
		AASMonsterAttackIndicatorActor* Indicator = World->SpawnActorDeferred<AASMonsterAttackIndicatorActor>(
			IndicatorClass, SpawnTransform, Character, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Indicator)
		{
			return;
		}

		Indicator->InitializeIndicator(Shape, Radius, Length, Width, Wave.TelegraphDuration);
		Indicator->FinishSpawning(SpawnTransform);
		ActiveIndicators.Add(Indicator);
	};

	for (const FHazardInstance& Instance : CurrentWaveInstances)
	{
		if (Wave.Shape == EASMonsterHazardShape::Circle)
		{
			SpawnIndicator(Instance.Location, Instance.Rotation, EASMonsterAttackIndicatorShape::Circle,
				Wave.CircleRadius, 0.0f, 0.0f);
			continue;
		}

		SpawnIndicator(Instance.Location, Instance.Rotation, EASMonsterAttackIndicatorShape::Rectangle,
			0.0f, Wave.CrossLength, Wave.CrossWidth);
		SpawnIndicator(Instance.Location, Instance.Rotation + FRotator(0.0f, 90.0f, 0.0f),
			EASMonsterAttackIndicatorShape::Rectangle, 0.0f, Wave.CrossLength, Wave.CrossWidth);
	}
}

void UASMonsterHazardSequenceAbility::DestroyCurrentWaveIndicators()
{
	for (TWeakObjectPtr<AASMonsterAttackIndicatorActor>& Indicator : ActiveIndicators)
	{
		if (AASMonsterAttackIndicatorActor* IndicatorActor = Indicator.Get())
		{
			IndicatorActor->Destroy();
		}
	}

	ActiveIndicators.Reset();
}

void UASMonsterHazardSequenceAbility::ApplyCurrentWaveDamage(const FASMonsterHazardWave& Wave)
{
	FASCombatTargetHitSet HitTargets;
	for (const FHazardInstance& Instance : CurrentWaveInstances)
	{
		if (Wave.Shape == EASMonsterHazardShape::Circle)
		{
			ApplyCircleDamage(Instance, Wave, HitTargets);
		}
		else
		{
			ApplyCrossDamage(Instance, Wave, HitTargets);
		}
	}
}

void UASMonsterHazardSequenceAbility::ApplyCircleDamage(const FHazardInstance& Instance,
	const FASMonsterHazardWave& Wave, FASCombatTargetHitSet& InOutHitTargets)
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!Character || !CombatTargets)
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Radius;
	Query.SearchOrigin = Instance.Location;
	Query.SearchRadius = FMath::Max(1.0f, Wave.CircleRadius);
	Query.Instigator = Character;
	Query.Domain = EASCombatTargetDomain::ActorOnly;
	Query.bCanHitPlayers = bCanHitPlayers;

	FASCombatImpactContext ImpactContext;
	ImpactContext.Preset = ImpactPreset;
	ImpactContext.HitLocation = Instance.Location;
	ImpactContext.HitDirection = (Instance.Location - SequenceOrigin).GetSafeNormal2D();
	const FGameplayEffectSpecHandle DamageSpec = MakeScaledDamageSpec(Wave.DamageScale);
	CombatTargets->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1), ImpactContext, &InOutHitTargets);
}

void UASMonsterHazardSequenceAbility::ApplyCrossDamage(const FHazardInstance& Instance,
	const FASMonsterHazardWave& Wave, FASCombatTargetHitSet& InOutHitTargets)
{
	ACharacter* Character = GetOwnerCharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!Character || !CombatTargets)
	{
		return;
	}

	auto ApplyBox = [this, Character, CombatTargets, &Instance, &Wave, &InOutHitTargets](const FRotator& Rotation)
	{
		const FVector Direction = Rotation.Vector().GetSafeNormal2D();
		FASCombatTargetQuery Query;
		Query.Shape = EASCombatTargetShape::Box;
		Query.SearchOrigin = Instance.Location;
		Query.Direction = Direction;
		Query.BoxHalfExtent = FVector(Wave.CrossLength * 0.5f, Wave.CrossWidth * 0.5f, 100.0f);
		Query.SearchRadius = Query.BoxHalfExtent.Size2D();
		Query.Instigator = Character;
		Query.Domain = EASCombatTargetDomain::ActorOnly;
		Query.bCanHitPlayers = bCanHitPlayers;

		FASCombatImpactContext ImpactContext;
		ImpactContext.Preset = ImpactPreset;
		ImpactContext.HitLocation = Instance.Location;
		ImpactContext.HitDirection = Direction;
		const FGameplayEffectSpecHandle DamageSpec = MakeScaledDamageSpec(Wave.DamageScale);
		CombatTargets->ResolveAndApply(Query, MakeArrayView(&DamageSpec, 1), ImpactContext, &InOutHitTargets);
	};

	ApplyBox(Instance.Rotation);
	ApplyBox(Instance.Rotation + FRotator(0.0f, 90.0f, 0.0f));
}

FGameplayEffectSpecHandle UASMonsterHazardSequenceAbility::MakeScaledDamageSpec(const float DamageScale) const
{
	FGameplayEffectSpecHandle DamageSpec = MakeDamageSpec();
	if (DamageSpec.IsValid())
	{
		DamageSpec.Data->SetSetByCallerMagnitude(
			ASGameplayTags::Data_Damage,
			-CalculateDamage() * FMath::Max(0.0f, DamageScale));
	}

	return DamageSpec;
}
