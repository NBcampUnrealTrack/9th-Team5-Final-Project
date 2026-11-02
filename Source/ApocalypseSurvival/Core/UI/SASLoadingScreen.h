#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Misc/ScopeLock.h"

/** MoviePlayer's Slate thread only reads this snapshot, never live UObjects. */
struct FASLoadingScreenModel
{
	void Update(const FASLoadingStatus& Value) { FScopeLock Lock(&Mutex); Status = Value; }
	FASLoadingStatus Read() const { FScopeLock Lock(&Mutex); return Status; }
private:
	mutable FCriticalSection Mutex;
	FASLoadingStatus Status;
};

using FASLoadingScreenModelPtr = TSharedPtr<FASLoadingScreenModel, ESPMode::ThreadSafe>;

class SASLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SASLoadingScreen) {}
		SLATE_EVENT(FSimpleDelegate, OnCancel)
		SLATE_EVENT(FSimpleDelegate, OnReturnToMenu)
		SLATE_ARGUMENT(FASLoadingScreenModelPtr, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	static int32 GetVisibleDotCount(double ElapsedSeconds);

private:
	FText GetDotsText() const;
	double StartTime = 0.0;
	TSharedPtr<FASLoadingScreenModel, ESPMode::ThreadSafe> Model;
};
