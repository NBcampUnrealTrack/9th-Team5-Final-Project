#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SASLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SASLoadingScreen) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	static int32 GetVisibleDotCount(double ElapsedSeconds);

private:
	FText GetDotsText() const;
	double StartTime = 0.0;
};
