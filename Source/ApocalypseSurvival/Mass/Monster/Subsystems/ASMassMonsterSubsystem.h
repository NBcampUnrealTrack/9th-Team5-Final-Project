#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "Mass/Monster/Targeting/ASMonsterTargetTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMassMonsterSubsystem.generated.h"

class UMassEntityConfigAsset;
class UMassSpawnerSubsystem;
class AActor;

/** 낮 필드 스폰포인트가 Entity별 Home Fragment에 덮어쓸 설정. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASMassMonsterHomeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Monster|Home", meta = (ClampMin = "0.0", Units = "cm"))
	float LeashRadius = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Monster|Home", meta = (ClampMin = "1.0", Units = "cm"))
	float ReturnAcceptanceRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Monster|Home", meta = (ClampMin = "0.0", Units = "cm"))
	float DetectionRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Monster|Home", meta = (ClampMin = "0.0", Units = "cm"))
	float LoseTargetRadius = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Monster|Home", meta = (ClampMin = "0.0"))
	float ReturnHealthPercentPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass|Monster|Home", meta = (ClampMin = "1.0"))
	float ReturnMoveSpeedMultiplier = 1.0f;
};

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

USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASMassMonsterStateCounts
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 Total = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 Idle = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 Chase = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 Attack = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 ReturnHome = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster")
	int32 Dead = 0;
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

	/**
	 * Mass Processor가 실행 중일 때 사용할 지연 생성 요청.
	 * Entity Manager 처리 구간이 끝난 뒤 생성하며, 실제 생성 수만 밤 웨이브 집계에 반영한다.
	 */
	bool QueueMonsterSpawns(UMassEntityConfigAsset* EntityConfig,
		TConstArrayView<FTransform> SpawnTransforms,
		EASMassMonsterWorldBehavior WorldBehavior,
		FASTargetHandle InitialPrimaryTarget,
		const FVector& InitialTargetLocation,
		bool bCountTowardWave);

	/** 방금 생성한 낮 몬스터에 스폰포인트별 감지·귀환 설정을 적용한다. */
	int32 ConfigureMonsterHomes(TConstArrayView<FMassEntityHandle> Entities,
		const FASMassMonsterHomeSettings& HomeSettings);

	/** 살아 있는 관리 Entity의 낮/밤 행동 모드를 일괄 전환한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster")
	int32 SetManagedMonsterWorldBehavior(EASMassMonsterWorldBehavior WorldBehavior);

	/** 이 Subsystem이 생성하고 아직 살아 있는 몬스터만 제거한다. */
	bool DestroyMonster(FMassEntityHandle Entity);
	int32 DestroyMonsters(TConstArrayView<FMassEntityHandle> Entities);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster")
	void DestroyAllManagedMonsters();

	UFUNCTION(BlueprintPure, Category = "Mass|Monster")
	int32 GetManagedMonsterCount() const;

	UFUNCTION(BlueprintPure, Category = "Mass|Monster")
	int32 GetAliveManagedMonsterCount() const;

	/** 공격 타겟 획득용으로 범위 안의 살아 있는 몬스터 중 선호 위치에 가장 가까운 Entity를 찾는다. */
	bool FindBestAliveMonsterTarget(const FVector& SearchOrigin, float SearchRadius,
		const FVector& PreferredLocation, uint64& OutTargetId, FVector& OutLocation,
		float& OutHitRadius) const;

	/** 익명 Target Id가 아직 살아 있는 관리 몬스터인지 확인하고 최신 위치를 반환한다. */
	bool GetAliveMonsterTarget(uint64 TargetId, FVector& OutLocation, float& OutHitRadius) const;

	/** Target Id로 지정한 살아 있는 Mass 몬스터 하나에 서버 피해를 적용한다. */
	int32 ApplyDamageToTarget(uint64 TargetId, float Damage, AActor* DamageInstigator);

	/** Actor 충돌체가 없는 Mass 몬스터에 원형 서버 피해를 적용한다. 반환값은 실제 피격 수다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster|Combat")
	int32 ApplyDamageInRadius(const FVector& Center, float Radius, float Damage,
		AActor* DamageInstigator, int32 MaxTargets = 0);

	/** AimDirection을 중심으로 한 부채꼴 서버 피해. AngleDegrees가 360이면 원형 판정과 같다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster|Combat")
	int32 ApplyDamageInCone(const FVector& Center, const FVector& AimDirection, float Radius,
		float AngleDegrees, float Damage, AActor* DamageInstigator, int32 MaxTargets = 0);

	/** Start-End 선분 주위의 Mass 몬스터에 서버 피해를 적용한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster|Combat")
	int32 ApplyDamageAlongSegment(const FVector& Start, const FVector& End, float SweepRadius,
		float Damage, AActor* DamageInstigator, int32 MaxTargets = 1);

	/** 투사체가 이미 맞힌 Entity Index를 건너뛰며 이동 선분 순서대로 피해를 적용하는 C++ 경로. */
	int32 ApplyDamageAlongSegmentUnique(const FVector& Start, const FVector& End, float SweepRadius,
		float Damage, AActor* DamageInstigator, TSet<int32>& InOutHitEntityIndices, int32 MaxTargets = 1);

	/** 기능 검증과 벤치마크 로그에서 사용하는 현재 행동 상태 집계. */
	UFUNCTION(BlueprintPure, Category = "Mass|Monster|Diagnostics")
	FASMassMonsterStateCounts GetManagedMonsterStateCounts() const;

private:
	struct FQueuedMonsterSpawnRequest
	{
		TWeakObjectPtr<UMassEntityConfigAsset> EntityConfig;
		TArray<FTransform> SpawnTransforms;
		EASMassMonsterWorldBehavior WorldBehavior = EASMassMonsterWorldBehavior::DayTerritory;
		FASTargetHandle InitialPrimaryTarget;
		FVector InitialTargetLocation = FVector::ZeroVector;
		bool bCountTowardWave = false;
	};

	bool CanMutateEntities(FString& OutError) const;
	int32 ApplyDamageToMatchingMonsters(float Damage, AActor* DamageInstigator, int32 MaxTargets,
		TFunctionRef<bool(FMassEntityHandle, const FVector&,
			const struct FASMassMonsterConfigFragment&)> Predicate);
	void FlushQueuedMonsterSpawns();
	void ScheduleQueuedMonsterSpawnFlush();
	void ConfigureInitialTargets(TConstArrayView<FMassEntityHandle> Entities, FASTargetHandle PrimaryTarget,
		const FVector& TargetLocation);
	void CompactManagedEntities();

	UPROPERTY(Transient)
	TObjectPtr<UMassSpawnerSubsystem> SpawnerSubsystem;

	TArray<FMassEntityHandle> ManagedEntities;
	TArray<FQueuedMonsterSpawnRequest> QueuedMonsterSpawnRequests;
	FTimerHandle QueuedMonsterSpawnFlushTimerHandle;
	bool bQueuedMonsterSpawnFlushScheduled = false;
	bool bMutationInProgress = false;
};
