#include "Mass/Monster/Subsystems/ASMonsterTargetSubsystem.h"

#include "Mass/Monster/Targeting/ASMonsterTargetableComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMonsterTargetSubsystem)

bool UASMonsterTargetSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UASMonsterTargetSubsystem::Deinitialize()
{
	TargetsById.Reset();
	HandlesByActor.Reset();
	LatestGenerationById.Reset();
	RecycledIds.Reset();
	NextTargetId = 1;
	Super::Deinitialize();
}

FASTargetHandle UASMonsterTargetSubsystem::RegisterTarget(UASMonsterTargetableComponent& Component)
{
	AActor* Actor = Component.GetOwner();
	if (!IsValid(Actor) || !Actor->HasAuthority() || Actor->IsActorBeingDestroyed())
	{
		return FASTargetHandle();
	}

	const TWeakObjectPtr<AActor> ActorKey(Actor);
	if (const FASTargetHandle* ExistingHandle = HandlesByActor.Find(ActorKey))
	{
		if (const FTargetRecord* ExistingRecord = TargetsById.Find(ExistingHandle->Id))
		{
			if (ExistingRecord->Handle == *ExistingHandle && ExistingRecord->Component.Get() == &Component)
			{
				return *ExistingHandle;
			}
		}

		UnregisterTarget(*ExistingHandle);
	}

	const FASTargetHandle Handle = AllocateHandle();
	FTargetRecord& Record = TargetsById.Add(Handle.Id);
	Record.Handle = Handle;
	Record.Actor = Actor;
	Record.Component = &Component;
	HandlesByActor.Add(ActorKey, Handle);
	return Handle;
}

void UASMonsterTargetSubsystem::UnregisterTarget(const FASTargetHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FTargetRecord* Record = TargetsById.Find(Handle.Id);
	if (!Record || Record->Handle != Handle)
	{
		return;
	}

	RemoveActorMapping(*Record);
	TargetsById.Remove(Handle.Id);
	RecycledIds.Add(Handle.Id);
}

bool UASMonsterTargetSubsystem::ResolveTarget(const FASTargetHandle Handle, FASTargetSnapshot& OutSnapshot) const
{
	OutSnapshot = FASTargetSnapshot();
	if (!Handle.IsValid())
	{
		return false;
	}

	const FTargetRecord* Record = TargetsById.Find(Handle.Id);
	return Record && Record->Handle == Handle && BuildSnapshot(*Record, OutSnapshot);
}

FASTargetHandle UASMonsterTargetSubsystem::FindTargetForActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return FASTargetHandle();
	}

	const FASTargetHandle* Handle = HandlesByActor.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Actor)));
	if (!Handle)
	{
		return FASTargetHandle();
	}

	const FTargetRecord* Record = TargetsById.Find(Handle->Id);
	return Record && Record->Handle == *Handle ? *Handle : FASTargetHandle();
}

bool UASMonsterTargetSubsystem::FindClosestTargetInRange(
	const FVector& Origin,
	const float MaxDistance,
	const EASMonsterTargetCapability RequiredCapabilities,
	FASTargetSnapshot& OutTarget) const
{
	OutTarget = FASTargetSnapshot();
	if (!FMath::IsFinite(MaxDistance) || MaxDistance < 0.0f)
	{
		return false;
	}

	const double MaxDistanceSquared = FMath::Square(static_cast<double>(MaxDistance));
	double ClosestDistanceSquared = MaxDistanceSquared;
	bool bFoundTarget = false;
	for (const TPair<int32, FTargetRecord>& Pair : TargetsById)
	{
		FASTargetSnapshot Snapshot;
		if (!BuildSnapshot(Pair.Value, Snapshot) || !Snapshot.bAlive
			|| !ASHasAllTargetCapabilities(Snapshot.Capabilities, RequiredCapabilities))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared2D(Origin, Snapshot.Location);
		const bool bIsCloser = DistanceSquared < ClosestDistanceSquared;
		const bool bBreaksDistanceTie = DistanceSquared == ClosestDistanceSquared
			&& (!bFoundTarget || Snapshot.Handle.Id < OutTarget.Handle.Id);
		if (bIsCloser || bBreaksDistanceTie)
		{
			ClosestDistanceSquared = DistanceSquared;
			OutTarget = MoveTemp(Snapshot);
			bFoundTarget = true;
		}
	}

	return bFoundTarget;
}

void UASMonsterTargetSubsystem::FindTargetsInRange(
	const FVector& Origin,
	const float MaxDistance,
	const EASMonsterTargetCapability RequiredCapabilities,
	TArray<FASTargetSnapshot>& OutTargets) const
{
	OutTargets.Reset();
	if (!FMath::IsFinite(MaxDistance) || MaxDistance < 0.0f)
	{
		return;
	}

	const double MaxDistanceSquared = FMath::Square(static_cast<double>(MaxDistance));
	for (const TPair<int32, FTargetRecord>& Pair : TargetsById)
	{
		FASTargetSnapshot Snapshot;
		if (!BuildSnapshot(Pair.Value, Snapshot) || !Snapshot.bAlive
			|| !ASHasAllTargetCapabilities(Snapshot.Capabilities, RequiredCapabilities)
			|| FVector::DistSquared2D(Origin, Snapshot.Location) > MaxDistanceSquared)
		{
			continue;
		}

		OutTargets.Add(MoveTemp(Snapshot));
	}
}

void UASMonsterTargetSubsystem::GetGlobalCandidateTargets(TArray<FASTargetSnapshot>& OutTargets) const
{
	OutTargets.Reset();
	constexpr EASMonsterTargetCapability GlobalPrimary = static_cast<EASMonsterTargetCapability>(
		static_cast<uint8>(EASMonsterTargetCapability::GlobalCandidate)
		| static_cast<uint8>(EASMonsterTargetCapability::PrimaryTarget));

	for (const TPair<int32, FTargetRecord>& Pair : TargetsById)
	{
		FASTargetSnapshot Snapshot;
		if (BuildSnapshot(Pair.Value, Snapshot) && Snapshot.bAlive
			&& ASHasAllTargetCapabilities(Snapshot.Capabilities, GlobalPrimary))
		{
			OutTargets.Add(MoveTemp(Snapshot));
		}
	}
}

FASTargetHandle UASMonsterTargetSubsystem::AllocateHandle()
{
	int32 Id = 0;
	if (!RecycledIds.IsEmpty())
	{
		Id = RecycledIds.Pop(EAllowShrinking::No);
	}
	else
	{
		Id = NextTargetId++;
		if (Id == 0)
		{
			Id = NextTargetId++;
		}
	}

	int32& Generation = LatestGenerationById.FindOrAdd(Id);
	++Generation;
	if (Generation <= 0)
	{
		Generation = 1;
	}

	FASTargetHandle Handle;
	Handle.Id = Id;
	Handle.Generation = Generation;
	return Handle;
}

bool UASMonsterTargetSubsystem::BuildSnapshot(const FTargetRecord& Record, FASTargetSnapshot& OutSnapshot) const
{
	UASMonsterTargetableComponent* Component = Record.Component.Get();
	AActor* Actor = Record.Actor.Get();
	if (!IsValid(Component) || !IsValid(Actor) || Actor->IsActorBeingDestroyed())
	{
		return false;
	}

	OutSnapshot.Handle = Record.Handle;
	OutSnapshot.Actor = Actor;
	OutSnapshot.Capabilities = Component->GetTargetCapabilities();
	OutSnapshot.Location = Component->GetTargetLocation();
	OutSnapshot.BaseScore = Component->GetTargetBaseScore();
	OutSnapshot.HitRadius = Component->GetTargetRadius();
	OutSnapshot.bAlive = Component->IsTargetable();
	OutSnapshot.bAttackable = OutSnapshot.bAlive
		&& (ASHasAllTargetCapabilities(OutSnapshot.Capabilities, EASMonsterTargetCapability::PrimaryTarget)
			|| ASHasAllTargetCapabilities(OutSnapshot.Capabilities, EASMonsterTargetCapability::BlockingTarget));
	return true;
}

void UASMonsterTargetSubsystem::RemoveActorMapping(const FTargetRecord& Record)
{
	if (AActor* Actor = Record.Actor.Get())
	{
		const TWeakObjectPtr<AActor> ActorKey(Actor);
		if (const FASTargetHandle* ExistingHandle = HandlesByActor.Find(ActorKey))
		{
			if (*ExistingHandle == Record.Handle)
			{
				HandlesByActor.Remove(ActorKey);
			}
		}
	}
}

