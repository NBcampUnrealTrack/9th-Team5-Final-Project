#include "Skills/Jaei/ASSkillCatchSling.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Skills/Area/ASSkillAreaActor.h"

UASSkillCatchSling::UASSkillCatchSling()
{
	//커서 위치 지정, 시전자를 따라다니지 않음, Actor 적에게 달라붙음
	TargetingType = EASSkillTargetingType::Location;
	bAttachToOwner = false;
	bSnapToActorTarget = true;

	Radius = 250.0f;
	Duration = 4.0f;
	TickInterval = 0.5f;
}

bool UASSkillCatchSling::FindFollowTarget(const FVector& Location, FASCombatTargetRef& OutTarget) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets)
	{
		return false;
	}

	FASCombatTargetQuery Query;
	Query.SearchOrigin = Location;
	Query.SearchRadius = ServerSearchRadius;
	Query.PriorityLocation = Location;
	Query.SortPolicy = EASCombatTargetSortPolicy::NearestToPriorityLocation;
	Query.Domain = bFollowMassTargets ? EASCombatTargetDomain::All : EASCombatTargetDomain::ActorOnly;
	Query.Instigator = Avatar;
	Query.bCanHitPlayers = bCanHitPlayers;
	return CombatTargets->FindBestTarget(Query, OutTarget);
}

void UASSkillCatchSling::OnSkillEffect_Implementation()
{
	//부모(UASSkillArea) 생성 로직 대신 따라갈 적을 찾은 뒤 직접 생성
	if (!HasSkillAuthority())
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
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

	//클라이언트가 보낸 목표 지점 (Actor 적에게 달라붙었으면 그 적의 발밑)
	const FVector TargetLocation = GetTargetLocation();

	FASCombatTargetRef Target;
	const bool bFoundTarget = FindFollowTarget(TargetLocation, Target);

	if (bDrawDebug)
	{
		DrawDebugCircle(World, TargetLocation, ServerSearchRadius, 32,
			bFoundTarget ? FColor::Green : FColor::Yellow, false, 1.0f, 0, 3.0f,
			FVector::ForwardVector, FVector::RightVector, false);
	}

	//적이 있으면 따라다니는 장판, 없으면 그 자리 고정 장판
	const FVector SpawnLocation = bFoundTarget ? Target.GetLocation() : TargetLocation;
	SpawnArea(SpawnLocation, Aim, bFoundTarget ? &Target : nullptr);
}
