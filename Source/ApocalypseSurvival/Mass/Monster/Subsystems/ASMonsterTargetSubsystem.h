#pragma once

#include "CoreMinimal.h"
#include "Mass/Monster/Targeting/ASMonsterTargetTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMonsterTargetSubsystem.generated.h"

class AActor;
class UASMonsterTargetableComponent;

/**
 * 서버에서 몬스터가 공격 또는 차단 대상으로 삼을 수 있는 Actor를 등록·조회한다.
 * 현재는 기능 검증을 위한 선형 후보 조회를 제공하며, 호출부는 이후 공간 인덱스로 교체할 수 있다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMonsterTargetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	FASTargetHandle RegisterTarget(UASMonsterTargetableComponent& Component);
	void UnregisterTarget(FASTargetHandle Handle);

	bool ResolveTarget(FASTargetHandle Handle, FASTargetSnapshot& OutSnapshot) const;
	FASTargetHandle FindTargetForActor(const AActor* Actor) const;
	bool FindClosestTargetInRange(const FVector& Origin, float MaxDistance,
		EASMonsterTargetCapability RequiredCapabilities, FASTargetSnapshot& OutTarget) const;

	void FindTargetsInRange(const FVector& Origin, float MaxDistance,
		EASMonsterTargetCapability RequiredCapabilities, TArray<FASTargetSnapshot>& OutTargets) const;
	void GetTargetsWithCapabilities(EASMonsterTargetCapability RequiredCapabilities,
		TArray<FASTargetSnapshot>& OutTargets) const;
	void GetGlobalCandidateTargets(TArray<FASTargetSnapshot>& OutTargets) const;

private:
	struct FTargetRecord
	{
		FASTargetHandle Handle;
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UASMonsterTargetableComponent> Component;
	};

	FASTargetHandle AllocateHandle();
	bool BuildSnapshot(const FTargetRecord& Record, FASTargetSnapshot& OutSnapshot) const;
	void RemoveActorMapping(const FTargetRecord& Record);

	TMap<int32, FTargetRecord> TargetsById;
	TMap<TWeakObjectPtr<AActor>, FASTargetHandle> HandlesByActor;
	TMap<int32, int32> LatestGenerationById;
	TArray<int32> RecycledIds;
	int32 NextTargetId = 1;
};

