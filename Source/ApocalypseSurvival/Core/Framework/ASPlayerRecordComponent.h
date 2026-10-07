#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASPlayerRecordComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASPlayerRecordComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASPlayerRecordComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용
	void AddDeath();
	void AddKill();
	
	UFUNCTION(BlueprintPure, Category = "Record")
	int32 GetDeathCount() const {return DeathCount;}
	
	UFUNCTION(BlueprintPure, Category = "Record")
	int32 GetKillCount() const {return KillCount;}

protected:
	UPROPERTY(Replicated)
	int32 DeathCount = 0;
	
	UPROPERTY(Replicated)
	int32 KillCount = 0;

public:
};
