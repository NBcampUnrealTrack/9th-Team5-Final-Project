#include "Mass/Monster/Subsystems/ASMassPlayerTargetSubsystem.h"

#include "Characters/Player/ASPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassPlayerTargetSubsystem)

bool UASMassPlayerTargetSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UASMassPlayerTargetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TimeUntilRefresh = 0.0f;
}

void UASMassPlayerTargetSubsystem::Deinitialize()
{
	TargetSnapshots.Reset();
	TargetIdsByController.Reset();
	NextTargetId = 1;
	TimeUntilRefresh = 0.0f;
	Super::Deinitialize();
}

void UASMassPlayerTargetSubsystem::Tick(const float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASMassPlayerTargetSubsystem);
	CSV_SCOPED_TIMING_STAT_GLOBAL(ASMassPlayerTargetSubsystem);

	Super::Tick(DeltaTime);

	TimeUntilRefresh -= DeltaTime;
	if (TimeUntilRefresh > 0.0f)
	{
		return;
	}

	RefreshTargetSnapshots();
	TimeUntilRefresh = FMath::Max(0.01f, SnapshotRefreshInterval);
}

bool UASMassPlayerTargetSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return !HasAnyFlags(RF_ClassDefaultObject)
		&& World
		&& World->IsGameWorld()
		&& World->GetNetMode() != NM_Client;
}

TStatId UASMassPlayerTargetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UASMassPlayerTargetSubsystem, STATGROUP_Tickables);
}

const FASMassPlayerTargetSnapshot* UASMassPlayerTargetSubsystem::FindTarget(const int32 TargetId) const
{
	if (TargetId == INDEX_NONE)
	{
		return nullptr;
	}

	return TargetSnapshots.FindByPredicate([TargetId](const FASMassPlayerTargetSnapshot& Snapshot)
	{
		return Snapshot.TargetId == TargetId;
	});
}

const FASMassPlayerTargetSnapshot* UASMassPlayerTargetSubsystem::FindClosestAliveTarget(
	const FVector& Origin,
	const float MaxDistance) const
{
	if (!FMath::IsFinite(MaxDistance) || MaxDistance < 0.0f)
	{
		return nullptr;
	}

	const double MaxDistanceSquared = FMath::Square(static_cast<double>(MaxDistance));
	double ClosestDistanceSquared = MaxDistanceSquared;
	const FASMassPlayerTargetSnapshot* ClosestTarget = nullptr;

	for (const FASMassPlayerTargetSnapshot& Snapshot : TargetSnapshots)
	{
		if (!Snapshot.bAlive || !Snapshot.Pawn.IsValid())
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared2D(Origin, Snapshot.Location);
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestTarget = &Snapshot;
		}
	}

	return ClosestTarget;
}

const FASMassPlayerTargetSnapshot* UASMassPlayerTargetSubsystem::FindTargetForActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	return TargetSnapshots.FindByPredicate([Actor](const FASMassPlayerTargetSnapshot& Snapshot)
	{
		return Snapshot.Pawn.Get() == Actor || Snapshot.Controller.Get() == Actor;
	});
}

APawn* UASMassPlayerTargetSubsystem::ResolveTargetPawn(const int32 TargetId) const
{
	const FASMassPlayerTargetSnapshot* Snapshot = FindTarget(TargetId);
	return Snapshot && Snapshot->bAlive ? Snapshot->Pawn.Get() : nullptr;
}

int32 UASMassPlayerTargetSubsystem::GetAliveTargetSnapshotCount() const
{
	int32 Count = 0;
	for (const FASMassPlayerTargetSnapshot& Snapshot : TargetSnapshots)
	{
		Count += Snapshot.bAlive ? 1 : 0;
	}
	return Count;
}

void UASMassPlayerTargetSubsystem::RefreshTargetSnapshots()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		TargetSnapshots.Reset();
		return;
	}

	TArray<FASMassPlayerTargetSnapshot> NewSnapshots;
	TSet<TWeakObjectPtr<APlayerController>> ConnectedControllers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!IsValid(PlayerController))
		{
			continue;
		}

		const TWeakObjectPtr<APlayerController> ControllerKey(PlayerController);
		ConnectedControllers.Add(ControllerKey);
		const int32 TargetId = FindOrAssignTargetId(*PlayerController);

		APawn* Pawn = PlayerController->GetPawn();
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
		{
			continue;
		}

		FASMassPlayerTargetSnapshot& Snapshot = NewSnapshots.AddDefaulted_GetRef();
		Snapshot.TargetId = TargetId;
		Snapshot.Location = Pawn->GetActorLocation();
		Snapshot.Controller = PlayerController;
		Snapshot.Pawn = Pawn;

		const AASPlayerCharacter* PlayerCharacter = Cast<AASPlayerCharacter>(Pawn);
		Snapshot.bAlive = !PlayerCharacter || !PlayerCharacter->IsDead();
	}

	for (auto It = TargetIdsByController.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !ConnectedControllers.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	NewSnapshots.Sort([](const FASMassPlayerTargetSnapshot& Left, const FASMassPlayerTargetSnapshot& Right)
	{
		return Left.TargetId < Right.TargetId;
	});
	TargetSnapshots = MoveTemp(NewSnapshots);
}

int32 UASMassPlayerTargetSubsystem::FindOrAssignTargetId(APlayerController& PlayerController)
{
	const TWeakObjectPtr<APlayerController> ControllerKey(&PlayerController);
	if (const int32* ExistingTargetId = TargetIdsByController.Find(ControllerKey))
	{
		return *ExistingTargetId;
	}

	const int32 NewTargetId = NextTargetId++;
	TargetIdsByController.Add(ControllerKey, NewTargetId);
	return NewTargetId;
}
