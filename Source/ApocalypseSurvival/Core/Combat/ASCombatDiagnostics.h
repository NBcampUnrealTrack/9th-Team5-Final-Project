#pragma once

#include "CoreMinimal.h"

namespace ASCombatDiagnostics
{
	bool IsEnabled();
	void Record(const UObject* Context, const FString& Message);
}
