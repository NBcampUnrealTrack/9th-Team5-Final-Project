#include "Items/Data/ASItemAssetLoading.h"

#include "Engine/AssetManager.h"
#include "Items/Data/ASItemDefinition.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/GAS/ASItemUseAbility.h"
#include "Items/Use/ASGrenadeActor.h"
#include "Items/Use/ASBarricadeActor.h"
#include "UObject/UnrealType.h"

namespace
{
	bool IsItemData(const UObject* Object)
	{
		return Object && (Object->IsA<UASItemDefinition>() || Object->IsA<UASWeaponAttackProfile>()
			|| Object->IsA<UASItemUseAbility>() || Object->IsA<AASGrenadeActor>() || Object->IsA<AASBarricadeActor>());
	}

	void GatherDependencies(const UObject& Object, TArray<FSoftObjectPath>& Paths, TSet<const UObject*>& Visited, bool bVisuals)
	{
		if (Visited.Contains(&Object)) return;
		Visited.Add(&Object);
		auto Add = [&](const FSoftObjectPtr& Reference)
		{
			const FSoftObjectPath Path = Reference.GetUniqueID();
			if (Path.IsNull()) return;
			Paths.AddUnique(Path);
			UObject* Loaded = Reference.Get();
			if (UClass* Class = Cast<UClass>(Loaded)) Loaded = Class->GetDefaultObject();
			if (IsItemData(Loaded)) GatherDependencies(*Loaded, Paths, Visited, false);
		};
		for (TFieldIterator<FProperty> It(Object.GetClass()); It; ++It)
		{
			if (!bVisuals && Object.IsA<UASItemDefinition>() && (It->GetFName() == TEXT("Icon") || It->GetFName() == TEXT("WorldMesh"))) continue;
			if (const auto* Soft = CastField<FSoftObjectProperty>(*It)) Add(Soft->GetPropertyValue_InContainer(&Object));
			else if (const auto* Array = CastField<FArrayProperty>(*It))
			{
				if (const auto* Inner = CastField<FSoftObjectProperty>(Array->Inner))
				{
					FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(&Object));
					for (int32 Index = 0; Index < Helper.Num(); ++Index) Add(Inner->GetPropertyValue(Helper.GetRawPtr(Index)));
				}
			}
		}
	}

	TArray<FSoftObjectPath> Expand(const TArray<FSoftObjectPath>& Input)
	{
		TArray<FSoftObjectPath> Paths;
		TSet<const UObject*> Visited;
		for (const FSoftObjectPath& Path : Input)
		{
			if (Path.IsNull()) continue;
			Paths.AddUnique(Path);
			UObject* Loaded = Path.ResolveObject();
			if (UClass* Class = Cast<UClass>(Loaded)) Loaded = Class->GetDefaultObject();
			if (IsItemData(Loaded)) GatherDependencies(*Loaded, Paths, Visited, false);
		}
		Paths.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
		return Paths;
	}
	bool AllResolved(const TArray<FSoftObjectPath>& Paths)
	{
		return !Paths.ContainsByPredicate([](const FSoftObjectPath& Path) { return !IsValid(Path.ResolveObject()); });
	}
}

TArray<FSoftObjectPath> ASItemAssetLoading::Gather(const UASItemDefinition& Definition, const bool bIncludeVisuals)
{
	TArray<FSoftObjectPath> Paths;
	auto Add = [&Paths](const FSoftObjectPath& Path) { if (!Path.IsNull()) Paths.AddUnique(Path); };
	if (bIncludeVisuals)
	{
		Add(Definition.Icon.ToSoftObjectPath());
		Add(Definition.WorldMesh.ToSoftObjectPath());
	}
	if (Definition.ItemType == EASItemType::Weapon)
	{
		Add(Definition.AttackProfile.ToSoftObjectPath());
		Add(Definition.EquippedActorClass.ToSoftObjectPath());
		Add(Definition.DamageEffectClass.ToSoftObjectPath());
	}
	else if (Definition.ItemType == EASItemType::Consumable)
	{
		Add(Definition.UseAbility.ToSoftObjectPath());
		for (const auto& Effect : Definition.UseEffects) Add(Effect.ToSoftObjectPath());
	}
	return Expand(Paths);
}

TArray<FSoftObjectPath> ASItemAssetLoading::GatherObject(const UObject& Object)
{
	TArray<FSoftObjectPath> Paths;
	TSet<const UObject*> Visited;
	GatherDependencies(Object, Paths, Visited, true);
	return Expand(Paths);
}

bool ASItemAssetLoading::AreLoaded(const TArray<FSoftObjectPath>& Paths)
{
	return AllResolved(Expand(Paths));
}

bool FASItemAssetRequest::Request(const FSoftObjectPath& Path, FStreamableDelegate OnComplete)
{
	return Request(Path.IsNull() ? TArray<FSoftObjectPath>() : TArray<FSoftObjectPath>{Path}, MoveTemp(OnComplete));
}

bool FASItemAssetRequest::Request(const TArray<FSoftObjectPath>& Paths, FStreamableDelegate OnComplete)
{
	const TArray<FSoftObjectPath> UniquePaths = Expand(Paths);
	if (UniquePaths == RequestedPaths && Handle.IsValid()
		&& (!Handle->HasLoadCompleted() || ASItemAssetLoading::AreLoaded(UniquePaths))) return true;
	FinishLoadingScreenTask(true);
	return RequestStage(UniquePaths, MoveTemp(OnComplete));
}

bool FASItemAssetRequest::RequestStage(const TArray<FSoftObjectPath>& UniquePaths, FStreamableDelegate OnComplete)
{
	// Start the replacement before releasing old references shared by both requests.
	TSharedPtr<FStreamableHandle> NextHandle;
	if (!UniquePaths.IsEmpty())
	{
		NextHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(UniquePaths,
			FStreamableDelegate::CreateLambda([this, PathsToCheck = UniquePaths, Completion = MoveTemp(OnComplete)]() mutable
			{
				// A newly loaded profile/ability may introduce another layer of soft references.
				const TArray<FSoftObjectPath> Expanded = Expand(PathsToCheck);
				if (AllResolved(PathsToCheck) && Expanded != PathsToCheck)
				{
					FStreamableDelegate FailureCompletion = Completion;
					if (!RequestStage(Expanded, MoveTemp(Completion)))
					{
						FinishLoadingScreenTask(false);
						FailureCompletion.ExecuteIfBound();
					}
					return;
				}
				for (const FSoftObjectPath& Path : PathsToCheck)
					if (!IsValid(Path.ResolveObject())) UE_LOG(LogTemp, Warning, TEXT("[ItemAssets] Could not load %s."), *Path.ToString());
				FinishLoadingScreenTask(ASItemAssetLoading::AreLoaded(PathsToCheck));
				Completion.ExecuteIfBound();
			}), FStreamableManager::DefaultAsyncLoadPriority, false, true);
	}
	if (Handle) Handle->CancelHandle();
	Handle.Reset();
	RequestedPaths = UniquePaths;
	Handle = MoveTemp(NextHandle);
	if (Handle) Handle->StartStalledHandle();
	return Handle.IsValid();
}

void FASItemAssetRequest::TrackLoadingScreen(UObject* Owner, const FText& Label)
{
	// Reattach even for a deduplicated request: the previous window may have been cancelled.
	FinishLoadingScreenTask(true);
	UWorld* World = IsValid(Owner) ? Owner->GetWorld() : nullptr;
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	if (!Instance || World->GetNetMode() == NM_DedicatedServer || RequestedPaths.IsEmpty()
		|| ASItemAssetLoading::AreLoaded(RequestedPaths)) return;
	UASLoadingFlowSubsystem* Flow = Instance->GetSubsystem<UASLoadingFlowSubsystem>();
	if (!Flow || !Flow->IsLoading()) return;
	ScreenFlow = Flow;
	ScreenTask = Flow->BeginTask(Label, Owner);
	// A rejected request or a completed request with unresolved paths is a failure, not readiness.
	if (!Handle) FinishLoadingScreenTask(false);
}

void FASItemAssetRequest::FinishLoadingScreenTask(bool bSuccess)
{
	UASLoadingFlowSubsystem* Flow = ScreenFlow.Get();
	const FGuid Task = ScreenTask;
	ScreenTask.Invalidate();
	ScreenFlow.Reset();
	if (!Flow || !Task.IsValid()) return;
	if (bSuccess) Flow->CompleteTask(Task);
	else Flow->FailTask(Task, NSLOCTEXT("ASItemLoading", "Failed", "Could not prepare item resources."));
}

void FASItemAssetRequest::Reset()
{
	if (Handle) Handle->CancelHandle();
	Handle.Reset();
	RequestedPaths.Reset();
	// The item/old world no longer needs this request. Other callers' tasks remain intact.
	FinishLoadingScreenTask(true);
}
