#include "ASProgressionData.h"

int32 UASProgressionData::GetExpToNextLevel(int32 Level) const
{
	if (Level >= MaxLevel || ExpToNextLevel.Num() == 0)
	{
		return 0;
	}
	
	const int32 Index = FMath::Clamp(Level - 1, 0, ExpToNextLevel.Num() - 1);
	return FMath::Max(1, ExpToNextLevel[Index]);
}

int32 UASProgressionData::GetSkillPointAtLevel(int32 Level) const
{
	const int32* Found = SkillPointsAtLevel.Find(Level);
	return Found ? *Found : 0;
}
