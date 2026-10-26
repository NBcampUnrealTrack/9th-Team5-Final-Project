#include "Items/Data/ASItemAssetLoading.h"

#include "Engine/AssetManager.h"
#include "Items/Data/ASItemDefinition.h"

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
		Add(Definition.EquippedActorClass.ToSoftObjectPath());
		Add(Definition.DamageEffectClass.ToSoftObjectPath());
	}
	else if (Definition.ItemType == EASItemType::Consumable)
	{
		Add(Definition.UseAbility.ToSoftObjectPath());
		for (const auto& Effect : Definition.UseEffects) Add(Effect.ToSoftObjectPath());
	}
	return Paths;
}

bool ASItemAssetLoading::AreLoaded(const TArray<FSoftObjectPath>& Paths)
{
	return !Paths.ContainsByPredicate([](const FSoftObjectPath& Path) { return !IsValid(Path.ResolveObject()); });
}

bool FASItemAssetRequest::Request(const FSoftObjectPath& Path, FStreamableDelegate OnComplete)
{
	return Request(Path.IsNull() ? TArray<FSoftObjectPath>() : TArray<FSoftObjectPath>{Path}, MoveTemp(OnComplete));
}

bool FASItemAssetRequest::Request(const TArray<FSoftObjectPath>& Paths, FStreamableDelegate OnComplete)
{
	TArray<FSoftObjectPath> UniquePaths;
	for (const FSoftObjectPath& Path : Paths) if (!Path.IsNull()) UniquePaths.AddUnique(Path);
	UniquePaths.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
	if (UniquePaths == RequestedPaths && Handle.IsValid()) return true;
	// Start the replacement before releasing old references shared by both requests.
	TSharedPtr<FStreamableHandle> NextHandle;
	if (!UniquePaths.IsEmpty())
	{
		NextHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(UniquePaths,
			FStreamableDelegate::CreateLambda([PathsToCheck = UniquePaths, Completion = MoveTemp(OnComplete)]() mutable
			{
				for (const FSoftObjectPath& Path : PathsToCheck)
					if (!IsValid(Path.ResolveObject())) UE_LOG(LogTemp, Warning, TEXT("[ItemAssets] Could not load %s."), *Path.ToString());
				Completion.ExecuteIfBound();
			}));
	}
	Reset();
	RequestedPaths = MoveTemp(UniquePaths);
	Handle = MoveTemp(NextHandle);
	return Handle.IsValid();
}

void FASItemAssetRequest::Reset()
{
	if (Handle) Handle->CancelHandle();
	Handle.Reset();
	RequestedPaths.Reset();
}
