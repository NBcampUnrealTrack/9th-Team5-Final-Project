#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ASMassMonsterClientInterpolationFragment.generated.h"

/** 클라이언트에서 서버 스냅샷만으로 표시되는 Mass 몬스터를 구분한다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterClientReplicaTag : public FMassTag
{
	GENERATED_BODY()
};

/** 서버 스냅샷 사이의 위치와 Yaw를 부드럽게 표시하기 위한 클라이언트 전용 상태다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterClientInterpolationFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector StartLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	float StartYawRadians = 0.0f;
	float TargetYawRadians = 0.0f;
	float ElapsedSeconds = 0.0f;
	float DurationSeconds = 0.1f;
	float LastServerTimeSeconds = -1.0f;
	bool bInitialized = false;
};
