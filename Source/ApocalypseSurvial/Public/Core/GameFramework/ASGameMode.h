#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ASGameMode.generated.h"

UCLASS()
class APOCALYPSESURVIAL_API AASGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AASGameMode();
	
	// 규칙 조회
	virtual float GetDayDuration(int32 Day) const;
	virtual float GetNightDuration(int32 Day) const;
	virtual bool IsBossDay(int32 Day) const;
	virtual bool IsDeathCountEnabled() const;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float DayDuration = 140.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Time")
	float NightDuration = 110.f;
	
	// 보스 등장일
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Boss")
	TArray<int32> BossDays;
	
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	int32 MaxDeathCount = 1;
	
	// 0 = 무제한
	UPROPERTY(EditDefaultsOnly, Category = "Rule|Respawn")
	float RespawnSelectTimeLimit = 0.f;
};
