#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ASMassMonsterActorSeparationProcessor.generated.h"

/** Actor Capsule과 BlockingTarget 설치물의 충돌 형태에서 Mass 몬스터를 분리한다. */
UCLASS(Config = Game, DefaultConfig)
class APOCALYPSESURVIVAL_API UASMassMonsterActorSeparationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UASMassMonsterActorSeparationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Separation", meta = (ClampMin = "0.0", Units = "cm"))
	float SeparationPadding = 4.0f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Mass|Monster|Separation", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxCorrectionPerFrame = 100.0f;
};
