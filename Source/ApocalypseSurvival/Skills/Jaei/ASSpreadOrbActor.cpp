#include "Skills/Jaei/ASSpreadOrbActor.h"

#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Debug/ASSkillDamageDebug.h"

namespace ASSpreadOrbPrivate
{
	bool IsSameTarget(const FASCombatTargetRef& A, const FASCombatTargetRef& B)
	{
		if (A.IsActor() || B.IsActor())
		{
			return A.GetActor() == B.GetActor();
		}
		return A.GetMassTargetId() == B.GetMassTargetId();
	}

	//대상의 몸 반경 (Actor는 캡슐, Mass는 HitRadius)
	float GetTargetBodyRadius(const FASCombatTargetRef& Target)
	{
		if (const AActor* Actor = Target.GetActor())
		{
			return Actor->GetSimpleCollisionRadius();
		}
		return Target.GetHitRadius();
	}
}

void AASSpreadOrbActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateHoming();
}

void AASSpreadOrbActor::OnTargetsHit(const FASCombatApplyResult& Result, const FVector& HitLocation)
{
	//처음 구체만, 한 번만 튕김
	if (!HasAuthority() || bIsSpreadChild || bHasSpread || SpreadCount <= 0)
	{
		return;
	}

	bHasSpread = true;
	Spread(HitLocation);
}

void AASSpreadOrbActor::Spread(const FVector& Origin)
{
	//작은 구체 피해 = 처음 구체 피해 x 배율
	TArray<FGameplayEffectSpecHandle> SpreadSpecs;
	for (const FGameplayEffectSpecHandle& Spec : DamageSpecs)
	{
		const FGameplayEffectSpecHandle ScaledSpec = MakeScaledSpec(Spec, SpreadDamageScale);
		if (ScaledSpec.IsValid())
		{
			SpreadSpecs.Add(ScaledSpec);
		}
	}

	//1. 주변 적에게 유도 구체
	const TArray<FASCombatTargetRef> Targets = FindSpreadTargets(Origin);
	for (const FASCombatTargetRef& Target : Targets)
	{
		FVector Direction = (Target.GetLocation() - Origin).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = GetActorForwardVector().GetSafeNormal2D();
		}
		SpawnChildOrb(Origin, Direction, SpreadSpecs, &Target);

		if (bDrawDebug && ASSkillDamageDebug::IsSkillDrawEnabled())
		{
			DrawDebugLine(GetWorld(), Origin, FVector(Target.GetLocation().X, Target.GetLocation().Y, Origin.Z),
				FColor::Cyan, false, 1.0f, 0, 2.0f);
		}
	}

	//2. 남는 구체 흩뿌리기 (옵션)
	const int32 Remaining = SpreadCount - Targets.Num();
	if (bScatterWhenNoTarget && Remaining > 0)
	{
		FVector Forward = GetVelocity().GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			Forward = GetActorForwardVector().GetSafeNormal2D();
		}

		const float Step = 360.0f / Remaining;
		for (int32 Index = 0; Index < Remaining; ++Index)
		{
			const FVector Direction = Forward.RotateAngleAxis(Step * 0.5f + Step * Index, FVector::UpVector);
			SpawnChildOrb(Origin, Direction, SpreadSpecs, nullptr);

			if (bDrawDebug && ASSkillDamageDebug::IsSkillDrawEnabled())
			{
				DrawDebugLine(GetWorld(), Origin, Origin + Direction * ScatterRange, FColor::Yellow, false, 1.0f, 0, 2.0f);
			}
		}
	}

	if (bDrawDebug && ASSkillDamageDebug::IsSkillDrawEnabled())
	{
		DrawDebugCircle(GetWorld(), Origin, SeekRadius, 32, FColor::Cyan, false, 1.0f, 0, 2.0f,
			FVector::ForwardVector, FVector::RightVector, false);
	}
}

TArray<FASCombatTargetRef> AASSpreadOrbActor::FindSpreadTargets(const FVector& Origin) const
{
	TArray<FASCombatTargetRef> Candidates;

	const UASCombatTargetSubsystem* CombatTargets = GetWorld() ? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets || SeekRadius <= 0.0f)
	{
		return Candidates;
	}

	FASCombatTargetQuery Query;
	Query.SearchOrigin = Origin;
	Query.SearchRadius = SeekRadius;
	Query.SortPolicy = EASCombatTargetSortPolicy::NearestToPriorityLocation;
	Query.Domain = EASCombatTargetDomain::All;
	Query.Instigator = InstigatorAvatar.Get();
	Query.bCanHitPlayers = CanHitPlayers();

	//처음 맞은 적 제외 거리 (Mass는 맞은 기록을 밖에서 볼 수 없어서 맞은 지점과의 거리로 판단)
	const float OrbRadius = CollisionComponent ? CollisionComponent->GetScaledSphereRadius() : 20.0f;

	//범위 안 목록을 주는 함수가 없어서, 주변에 기준점을 돌려 찍으며 "그 점에서 가장 가까운 적"을 모음
	const int32 SampleCount = FMath::Max(1, SpreadSamplePoints);
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float Angle = 360.0f / SampleCount * Index;
		Query.PriorityLocation = Origin + FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * (SeekRadius * 0.6f);

		FASCombatTargetRef Candidate;
		if (!CombatTargets->FindBestTarget(Query, Candidate))
		{
			continue;
		}

		//처음 맞은 적 (Actor는 맞은 기록, Mass는 맞은 지점 바로 옆)
		if (const AActor* CandidateActor = Candidate.GetActor())
		{
			if (HitTargets.ContainsActor(CandidateActor))
			{
				continue;
			}
		}
		else if (FVector::DistSquared2D(Candidate.GetLocation(), Origin) <= FMath::Square(OrbRadius + Candidate.GetHitRadius()))
		{
			continue;
		}

		const bool bAlreadyAdded = Candidates.ContainsByPredicate([&Candidate](const FASCombatTargetRef& Existing)
		{
			return ASSpreadOrbPrivate::IsSameTarget(Existing, Candidate);
		});
		if (!bAlreadyAdded)
		{
			Candidates.Add(Candidate);
		}
	}

	//엘리트·보스 먼저(옵션), 그다음 가까운 순
	const bool bPreferActors = bPreferActorTargets;
	Candidates.Sort([&Origin, bPreferActors](const FASCombatTargetRef& A, const FASCombatTargetRef& B)
	{
		if (bPreferActors && A.IsActor() != B.IsActor())
		{
			return A.IsActor();
		}
		return FVector::DistSquared2D(A.GetLocation(), Origin) < FVector::DistSquared2D(B.GetLocation(), Origin);
	});

	if (Candidates.Num() > SpreadCount)
	{
		Candidates.SetNum(SpreadCount);
	}
	return Candidates;
}

void AASSpreadOrbActor::SpawnChildOrb(const FVector& Origin, const FVector& Direction,
	const TArray<FGameplayEffectSpecHandle>& Specs, const FASCombatTargetRef* InHomingTarget)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* ClassToSpawn = SpreadOrbClass ? SpreadOrbClass.Get() : GetClass();
	const FTransform SpawnTransform(Direction.Rotation(), Origin);

	AASSpreadOrbActor* Orb = World->SpawnActorDeferred<AASSpreadOrbActor>(
		ClassToSpawn, SpawnTransform, GetOwner(), GetInstigator(), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Orb)
	{
		return;
	}

	Orb->bIsSpreadChild = true;

	if (InHomingTarget)
	{
		//유도: 가는 길 판정 끔, 대상에 닿으면 직접 피해. 사라지는 시간은 넉넉히 (대상을 쫓아가는 거리)
		Orb->HomingTarget = *InHomingTarget;
		Orb->HomingSpeed = SpreadSpeed;
		Orb->bResolveSegmentHits = false;
		Orb->InitProjectile(Specs, InstigatorAvatar.Get(), Direction, SpreadSpeed, SeekRadius * 3.0f, 0, CanHitPlayers(), ImpactContext);
	}
	else
	{
		//직선: 이미 맞은 적은 다시 안 맞음 (맞은 기록 물려받음, 물려받은 수만큼 관통 수에 더함)
		Orb->HitTargets = HitTargets;
		Orb->InitProjectile(Specs, InstigatorAvatar.Get(), Direction, SpreadSpeed, ScatterRange, HitTargets.Num(), CanHitPlayers(), ImpactContext);
	}

	Orb->FinishSpawning(SpawnTransform);
}

void AASSpreadOrbActor::UpdateHoming()
{
	if (!HasAuthority() || !HomingTarget.IsValid())
	{
		return;
	}

	UASCombatTargetSubsystem* CombatTargets = GetWorld() ? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets)
	{
		return;
	}

	//대상 최신 위치 (죽었거나 사라졌으면 구체도 사라짐)
	FASCombatTargetQuery Query;
	Query.Instigator = InstigatorAvatar.Get();
	Query.bCanHitPlayers = CanHitPlayers();
	if (!CombatTargets->RefreshTarget(Query, HomingTarget))
	{
		HomingTarget.Reset();
		EndProjectile();
		return;
	}

	const FVector OrbLocation = GetActorLocation();
	const FVector TargetLocation = HomingTarget.GetLocation();

	//닿음 = 구체 반경 + 대상 몸 반경 (높이는 무시)
	const float OrbRadius = CollisionComponent ? CollisionComponent->GetScaledSphereRadius() : 20.0f;
	const float ReachDistance = OrbRadius + ASSpreadOrbPrivate::GetTargetBodyRadius(HomingTarget);

	if (FVector::DistSquared2D(OrbLocation, TargetLocation) <= FMath::Square(ReachDistance))
	{
		//이 대상에게만 피해
		//경직 정보(처음 구체에서 물려받음)도 같이 전달
		FASCombatImpactContext HitContext = ImpactContext;
		HitContext.HitLocation = TargetLocation;
		HitContext.HitDirection = (TargetLocation - OrbLocation).GetSafeNormal2D();
		const FASCombatApplyResult Result = CombatTargets->ApplyEffectSpecsToTarget(HomingTarget, DamageSpecs, InstigatorAvatar.Get(), HitContext);
		ASSkillDamageDebug::Report(this, DamageSpecs, Result, TargetLocation);

		HomingTarget.Reset();
		EndProjectile();
		return;
	}

	//대상 쪽으로 방향 꺾기 (높이는 그대로 유지)
	const FVector Direction = (TargetLocation - OrbLocation).GetSafeNormal2D();
	if (!Direction.IsNearlyZero() && ProjectileMovement)
	{
		ProjectileMovement->Velocity = Direction * HomingSpeed;
	}
}

FGameplayEffectSpecHandle AASSpreadOrbActor::MakeScaledSpec(const FGameplayEffectSpecHandle& Spec, float Scale)
{
	if (!Spec.IsValid())
	{
		return Spec;
	}

	//원본 Spec은 처음 구체가 쓰고 있으므로 복사해서 수정
	FGameplayEffectSpecHandle ScaledSpec(new FGameplayEffectSpec(*Spec.Data.Get()));
	const float Damage = Spec.Data->GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f);
	ScaledSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, Damage * Scale);
	return ScaledSpec;
}
