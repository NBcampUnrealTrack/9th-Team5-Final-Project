#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMassMonsterSubsystem.generated.h"

class UMassEntityConfigAsset;
class UMassSpawnerSubsystem;

UENUM(BlueprintType)
enum class EASMassMonsterSpawnStatus : uint8
{
	Success,
	InvalidRequest,
	NotAuthority,
	MassUnavailable,
	InvalidEntityConfig,
	SpawnFailed
};

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASMassMonsterSpawnResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	EASMassMonsterSpawnStatus Status = EASMassMonsterSpawnStatus::SpawnFailed;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 RequestedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 SpawnedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	FString ErrorMessage;

	bool IsSuccess() const { return Status == EASMassMonsterSpawnStatus::Success; }
};

/** 일반 Mass 몬스터의 생성, Handle 추적, 제거를 담당하는 서버 월드 Subsystem. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Blueprint와 테스트 도구에서 사용하는 Batch Spawn 진입점. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster")
	FASMassMonsterSpawnResult SpawnMonstersAtTransforms(UMassEntityConfigAsset* EntityConfig,
		const TArray<FTransform>& SpawnTransforms,
		EASMassMonsterWorldBehavior WorldBehavior = EASMassMonsterWorldBehavior::DayTerritory);

	/** 게임 코드에서 생성된 Entity Handle이 필요할 때 사용하는 Batch Spawn 진입점. */
	FASMassMonsterSpawnResult SpawnMonsters(UMassEntityConfigAsset& EntityConfig,
		TConstArrayView<FTransform> SpawnTransforms,
		EASMassMonsterWorldBehavior WorldBehavior,
		TArray<FMassEntityHandle>& OutEntities);

	/** 이 Subsystem이 생성하고 아직 살아 있는 몬스터만 제거한다. */
	bool DestroyMonster(FMassEntityHandle Entity);
	int32 DestroyMonsters(TConstArrayView<FMassEntityHandle> Entities);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster")
	void DestroyAllManagedMonsters();

	UFUNCTION(BlueprintPure, Category = "Mass|Monster")
	int32 GetManagedMonsterCount() const;

private:
	bool CanMutateEntities(FString& OutError) const;
	void CompactManagedEntities();

	UPROPERTY(Transient)
	TObjectPtr<UMassSpawnerSubsystem> SpawnerSubsystem;

	TArray<FMassEntityHandle> ManagedEntities;
	bool bMutationInProgress = false;
};
