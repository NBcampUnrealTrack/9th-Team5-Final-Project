#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMassPlayerTargetSubsystem.generated.h"

class APawn;
class APlayerController;

/** 한 번 수집한 플레이어 타깃 정보. 다음 Snapshot 갱신 전까지 읽기 전용으로 사용한다. */
struct APOCALYPSESURVIVAL_API FASMassPlayerTargetSnapshot
{
	int32 TargetId = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<APawn> Pawn;
	bool bAlive = false;
};

/** 서버의 플레이어 타깃을 낮은 빈도로 수집해 Mass Processor에 제공한다. */
UCLASS(Config = Game, DefaultConfig)
class APOCALYPSESURVIVAL_API UASMassPlayerTargetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	TConstArrayView<FASMassPlayerTargetSnapshot> GetTargetSnapshots() const { return TargetSnapshots; }
	const FASMassPlayerTargetSnapshot* FindTarget(int32 TargetId) const;
	const FASMassPlayerTargetSnapshot* FindClosestAliveTarget(const FVector& Origin, float MaxDistance) const;
	APawn* ResolveTargetPawn(int32 TargetId) const;

	UFUNCTION(BlueprintPure, Category = "Mass|Monster|Targeting")
	int32 GetTargetSnapshotCount() const { return TargetSnapshots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Mass|Monster|Targeting")
	int32 GetAliveTargetSnapshotCount() const;

private:
	void RefreshTargetSnapshots();
	int32 FindOrAssignTargetId(APlayerController& PlayerController);

	/** Actor 탐색 비용과 위치 최신성 사이의 기본 절충값. */
	UPROPERTY(Config, EditAnywhere, Category = "Mass|Monster|Targeting", meta = (ClampMin = "0.01", Units = "s"))
	float SnapshotRefreshInterval = 0.1f;

	TArray<FASMassPlayerTargetSnapshot> TargetSnapshots;
	TMap<TWeakObjectPtr<APlayerController>, int32> TargetIdsByController;
	int32 NextTargetId = 1;
	float TimeUntilRefresh = 0.0f;
};
