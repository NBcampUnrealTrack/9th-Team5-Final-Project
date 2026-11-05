#pragma once

#include "CoreMinimal.h"

class UASMonsterDefinition;
class AController;
class UWorld;

/** Server gameplay notifications, shared by Actor and Mass monster death paths. */
class APOCALYPSESURVIVAL_API FASQuestEventLibrary
{
public:
	static void ReportMonsterDefeated(UWorld* World, UASMonsterDefinition* Definition, AController* Killer);
};
