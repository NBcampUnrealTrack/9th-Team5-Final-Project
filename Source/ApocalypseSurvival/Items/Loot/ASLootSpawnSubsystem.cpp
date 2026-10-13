#include "Items/Loot/ASLootSpawnSubsystem.h"

#include "Engine/World.h"
#include "Items/Loot/ASLootChest.h"
#include "Items/Loot/ASLootDefinition.h"
#include "Templates/UnrealTemplate.h"

namespace
{
	bool ValidatePreset(const FDataTableRowHandle& Preset, FString& Error)
	{
		if (!IsValid(Preset.DataTable) || Preset.RowName.IsNone()
			|| Preset.DataTable->GetRowStruct() != FASLootChestPresetRow::StaticStruct())
		{
			Error = TEXT("A loot preset table and valid row name are required.");
			return false;
		}
		const FASLootChestPresetRow* Row = Preset.DataTable->FindRow<FASLootChestPresetRow>(
			Preset.RowName, TEXT("LootSpawnSubsystem"), false);
		if (!Row)
		{
			Error = FString::Printf(TEXT("Loot preset '%s' does not exist."), *Preset.RowName.ToString());
			return false;
		}
		return Row->Validate(Error);
	}

	void CompleteBatch(FASLootSpawnBatchResult& Result)
	{
		if (Result.Chests.Num() == Result.RequestedCount)
		{
			Result.Status = EASLootSpawnBatchStatus::Success;
			Result.ErrorMessage.Reset();
		}
		else
		{
			Result.Status = Result.Chests.IsEmpty() ? EASLootSpawnBatchStatus::SpawnFailed : EASLootSpawnBatchStatus::PartialSuccess;
			Result.ErrorMessage = FString::Printf(TEXT("Spawned %d/%d chests after %d candidates. %s"),
				Result.Chests.Num(), Result.RequestedCount, Result.AttemptCount, *Result.ErrorMessage);
		}
	}

	bool IsInsideBox(const FVector& Location, const FTransform& Transform, const FVector& Extent)
	{
		const FVector Local = Transform.InverseTransformPosition(Location);
		return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
	}
}

bool UASLootSpawnSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UASLootSpawnSubsystem::Deinitialize()
{
	ManagedChests.Reset();
	Super::Deinitialize();
}

bool UASLootSpawnSubsystem::CanSpawn() const
{
	return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

bool UASLootSpawnSubsystem::HasSpacing(const FVector& Location, const float MinSpacing) const
{
	const double DistanceSquared = FMath::Square(static_cast<double>(MinSpacing));
	for (const TWeakObjectPtr<AASLootChest>& Entry : ManagedChests)
	{
		const AASLootChest* Chest = Entry.Get();
		if (IsValid(Chest) && !Chest->IsActorBeingDestroyed()
			&& FVector::DistSquared2D(Location, Chest->GetActorLocation()) < DistanceSquared) return false;
	}
	return true;
}

FASLootChestSpawnResult UASLootSpawnSubsystem::SpawnManagedChest(const FDataTableRowHandle& Preset,
	const FTransform& Transform, const int32 Seed)
{
	ManagedChests.RemoveAll([](const TWeakObjectPtr<AASLootChest>& Entry)
	{
		return !Entry.IsValid() || Entry->IsActorBeingDestroyed();
	});
	FASLootChestSpawnResult Result = UASLootBlueprintLibrary::SpawnLootChestFromPresetWithResult(this, Preset, Transform, Seed);
	if (Result.IsSuccess()) ManagedChests.Add(Result.Chest.Get());
	return Result;
}

FASLootChestSpawnResult UASLootSpawnSubsystem::SpawnChestAtTransform(const FDataTableRowHandle& Preset,
	const FTransform& Transform, const int32 Seed)
{
	if (!CanSpawn() || bSpawnInProgress)
	{
		FASLootChestSpawnResult Result;
		Result.Status = !GetWorld() ? EASLootChestSpawnStatus::InvalidWorld
			: !CanSpawn() ? EASLootChestSpawnStatus::NotAuthority : EASLootChestSpawnStatus::SpawnFailed;
		Result.ErrorMessage = TEXT("Spawning requires a server world and no active spawn operation.");
		return Result;
	}
	TGuardValue<bool> Guard(bSpawnInProgress, true);
	return SpawnManagedChest(Preset, Transform, Seed);
}

FASLootSpawnBatchResult UASLootSpawnSubsystem::SpawnChestsAtTransforms(const FDataTableRowHandle& Preset,
	const TArray<FTransform>& Transforms, const int32 Seed, const float MinSpacing)
{
	FASLootSpawnBatchResult Result;
	Result.RequestedCount = Transforms.Num();
	if (!CanSpawn())
	{
		Result.Status = EASLootSpawnBatchStatus::NotAuthority;
		Result.ErrorMessage = TEXT("Spawning requires a server world.");
		return Result;
	}
	Result.Status = EASLootSpawnBatchStatus::InvalidRequest;
	if (bSpawnInProgress || Transforms.IsEmpty() || Transforms.Num() > 10000
		|| !FMath::IsFinite(MinSpacing) || MinSpacing < 0.0f)
	{
		Result.ErrorMessage = TEXT("Provide 1..10000 transforms, nonnegative finite spacing, and no active spawn operation.");
		return Result;
	}
	for (const FTransform& Transform : Transforms)
	{
		if (Transform.ContainsNaN())
		{
			Result.ErrorMessage = TEXT("A spawn transform contains non-finite values.");
			return Result;
		}
	}
	if (!ValidatePreset(Preset, Result.ErrorMessage)) return Result;
	TGuardValue<bool> Guard(bSpawnInProgress, true);
	FRandomStream Random(Seed);
	for (const FTransform& Transform : Transforms)
	{
		++Result.AttemptCount;
		if (!HasSpacing(Transform.GetLocation(), MinSpacing))
		{
			Result.ErrorMessage = TEXT("A location is too close to an existing chest.");
			continue;
		}
		FASLootChestSpawnResult Spawn = SpawnManagedChest(Preset, Transform, static_cast<int32>(Random.GetUnsignedInt()));
		if (!Spawn.IsSuccess())
		{
			Result.ErrorMessage = Spawn.ErrorMessage;
			if (Spawn.Status != EASLootChestSpawnStatus::SpawnFailed) break;
			continue;
		}
		// Collision adjustment may have moved the new chest; exclude itself from the spacing check.
		ManagedChests.Pop();
		if (!HasSpacing(Spawn.Chest->GetActorLocation(), MinSpacing))
		{
			Spawn.Chest->Destroy();
			Result.ErrorMessage = TEXT("Collision adjustment violated the minimum chest spacing.");
			continue;
		}
		ManagedChests.Add(Spawn.Chest.Get());
		Result.Chests.Add(Spawn.Chest);
	}
	CompleteBatch(Result);
	return Result;
}

FASLootSpawnBatchResult UASLootSpawnSubsystem::SpawnChestsInBox(const FDataTableRowHandle& Preset,
	const FTransform& BoxTransform, const FVector HalfExtent, const FASLootRandomSpawnSettings& Settings, const int32 Seed)
{
	FASLootSpawnBatchResult Result;
	Result.RequestedCount = Settings.Count;
	if (!CanSpawn())
	{
		Result.Status = EASLootSpawnBatchStatus::NotAuthority;
		Result.ErrorMessage = TEXT("Spawning requires a server world.");
		return Result;
	}
	Result.Status = EASLootSpawnBatchStatus::InvalidRequest;
	if (bSpawnInProgress || BoxTransform.ContainsNaN() || HalfExtent.ContainsNaN()
		|| HalfExtent.GetMin() <= 0.0 || BoxTransform.GetScale3D().GetMin() <= 0.0
		|| Settings.Count < 1 || Settings.Count > 10000 || Settings.MaxAttempts < Settings.Count || Settings.MaxAttempts > 10000
		|| !FMath::IsFinite(Settings.MinSpacing) || Settings.MinSpacing < 0.0f
		|| !FMath::IsFinite(Settings.GroundOffset) || Settings.GroundOffset < 0.0f
		|| !FMath::IsFinite(Settings.MinGroundNormalZ) || Settings.MinGroundNormalZ < 0.0f || Settings.MinGroundNormalZ > 1.0f
		|| Settings.GroundTraceChannel >= ECC_MAX)
	{
		Result.ErrorMessage = TEXT("Invalid box/settings. Use positive extents/scale, 1 <= Count <= MaxAttempts <= 10000, and finite ground/spacing values.");
		return Result;
	}
	if (!ValidatePreset(Preset, Result.ErrorMessage)) return Result;
	TGuardValue<bool> Guard(bSpawnInProgress, true);
	FRandomStream Random(Seed);
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LootSpawnGround));
	for (const TWeakObjectPtr<AASLootChest>& Entry : ManagedChests)
		if (Entry.IsValid()) TraceParams.AddIgnoredActor(Entry.Get());
	while (Result.Chests.Num() < Settings.Count && Result.AttemptCount < Settings.MaxAttempts)
	{
		++Result.AttemptCount;
		const double X = Random.FRandRange(-HalfExtent.X, HalfExtent.X);
		const double Y = Random.FRandRange(-HalfExtent.Y, HalfExtent.Y);
		FVector Location;
		if (Settings.bProjectToGround)
		{
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit,
				BoxTransform.TransformPosition(FVector(X, Y, HalfExtent.Z)),
				BoxTransform.TransformPosition(FVector(X, Y, -HalfExtent.Z)), Settings.GroundTraceChannel, TraceParams)
				|| Hit.bStartPenetrating || Hit.ImpactNormal.Z < Settings.MinGroundNormalZ)
			{
				Result.ErrorMessage = TEXT("No suitable ground inside the box.");
				continue;
			}
			Location = Hit.ImpactPoint + FVector::UpVector * Settings.GroundOffset;
		}
		else Location = BoxTransform.TransformPosition(FVector(X, Y, Random.FRandRange(-HalfExtent.Z, HalfExtent.Z)));
		if (!IsInsideBox(Location, BoxTransform, HalfExtent) || !HasSpacing(Location, Settings.MinSpacing))
		{
			Result.ErrorMessage = TEXT("Candidate is outside the box or too close to an existing chest.");
			continue;
		}
		const FQuat Rotation = Settings.bRandomYaw ? FRotator(0.0, Random.FRandRange(0.0f, 360.0f), 0.0).Quaternion()
			: FRotator(0.0, BoxTransform.Rotator().Yaw, 0.0).Quaternion();
		// The box scale controls placement bounds, not the size of the spawned chest.
		FASLootChestSpawnResult Spawn = SpawnManagedChest(Preset, FTransform(Rotation, Location), static_cast<int32>(Random.GetUnsignedInt()));
		if (!Spawn.IsSuccess())
		{
			Result.ErrorMessage = Spawn.ErrorMessage;
			if (Spawn.Status != EASLootChestSpawnStatus::SpawnFailed) break;
			continue;
		}
		ManagedChests.Pop();
		if (!IsInsideBox(Spawn.Chest->GetActorLocation(), BoxTransform, HalfExtent)
			|| !HasSpacing(Spawn.Chest->GetActorLocation(), Settings.MinSpacing))
		{
			Spawn.Chest->Destroy();
			Result.ErrorMessage = TEXT("Collision adjustment violated the spawn bounds or minimum spacing.");
			continue;
		}
		ManagedChests.Add(Spawn.Chest.Get());
		TraceParams.AddIgnoredActor(Spawn.Chest.Get());
		Result.Chests.Add(Spawn.Chest);
	}
	CompleteBatch(Result);
	return Result;
}

int32 UASLootSpawnSubsystem::GetManagedChestCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AASLootChest>& Entry : ManagedChests)
		if (Entry.IsValid() && !Entry->IsActorBeingDestroyed()) ++Count;
	return Count;
}

void UASLootSpawnSubsystem::DestroyManagedChests()
{
	if (!CanSpawn() || bSpawnInProgress) return;
	TGuardValue<bool> Guard(bSpawnInProgress, true);
	const TArray<TWeakObjectPtr<AASLootChest>> Chests = MoveTemp(ManagedChests);
	ManagedChests.Reset();
	for (const TWeakObjectPtr<AASLootChest>& Entry : Chests)
		if (Entry.IsValid()) Entry->Destroy();
}
