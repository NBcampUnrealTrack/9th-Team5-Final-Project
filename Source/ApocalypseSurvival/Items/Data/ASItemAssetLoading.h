#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"

class UASItemDefinition;
class UASLoadingFlowSubsystem;

namespace ASItemAssetLoading
{
	/** Gameplay excludes optional icon/world mesh references. Empty soft references are not requested. */
	APOCALYPSESURVIVAL_API TArray<FSoftObjectPath> Gather(const UASItemDefinition& Definition, bool bIncludeVisuals);
	APOCALYPSESURVIVAL_API bool AreLoaded(const TArray<FSoftObjectPath>& Paths);
	/** Includes soft dependencies reachable through already-loaded item data and class defaults. */
	APOCALYPSESURVIVAL_API TArray<FSoftObjectPath> GatherObject(const UObject& Object);
}

/** Owner-held request: deduplicates refreshes, pins loaded assets, and cancels obsolete callbacks. */
class APOCALYPSESURVIVAL_API FASItemAssetRequest
{
public:
	FASItemAssetRequest() = default;
	FASItemAssetRequest(const FASItemAssetRequest&) = delete;
	FASItemAssetRequest& operator=(const FASItemAssetRequest&) = delete;
	~FASItemAssetRequest() { Reset(); }
	bool Request(const TArray<FSoftObjectPath>& Paths, FStreamableDelegate OnComplete);
	bool Request(const FSoftObjectPath& Path, FStreamableDelegate OnComplete);
	/** Optional screen barrier for this item-owned request. Never opens a loading window. */
	void TrackLoadingScreen(UObject* Owner, const FText& Label);
	void Reset();
private:
	bool RequestStage(const TArray<FSoftObjectPath>& Paths, FStreamableDelegate OnComplete);
	void FinishLoadingScreenTask(bool bSuccess);
	TArray<FSoftObjectPath> RequestedPaths;
	TSharedPtr<FStreamableHandle> Handle;
	TWeakObjectPtr<UASLoadingFlowSubsystem> ScreenFlow;
	FGuid ScreenTask;
};
