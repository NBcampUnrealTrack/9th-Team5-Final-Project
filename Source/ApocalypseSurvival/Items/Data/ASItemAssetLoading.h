#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"

class UASItemDefinition;

namespace ASItemAssetLoading
{
	/** Gameplay excludes optional icon/world mesh references. Empty soft references are not requested. */
	APOCALYPSESURVIVAL_API TArray<FSoftObjectPath> Gather(const UASItemDefinition& Definition, bool bIncludeVisuals);
	APOCALYPSESURVIVAL_API bool AreLoaded(const TArray<FSoftObjectPath>& Paths);
}

/** Owner-held request: deduplicates refreshes, pins loaded assets, and cancels obsolete callbacks. */
class APOCALYPSESURVIVAL_API FASItemAssetRequest
{
public:
	~FASItemAssetRequest() { Reset(); }
	bool Request(const TArray<FSoftObjectPath>& Paths, FStreamableDelegate OnComplete);
	bool Request(const FSoftObjectPath& Path, FStreamableDelegate OnComplete);
	void Reset();
private:
	TArray<FSoftObjectPath> RequestedPaths;
	TSharedPtr<FStreamableHandle> Handle;
};
