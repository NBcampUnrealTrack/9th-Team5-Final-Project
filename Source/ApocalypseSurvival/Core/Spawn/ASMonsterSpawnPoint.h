#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASMonsterSpawnPoint.generated.h"

class UArrowComponent;

/**
 * 맵에 배치하는 몬스터 스폰 위치. 구역 구분이 필요해질 때 SpawnAreaIndex를 사용한다.
 * 시간대는 이 액터가 아니라 이를 사용하는 스폰 규칙 DataTable이 결정한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API AASMonsterSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AASMonsterSpawnPoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<UArrowComponent> ArrowComponent;

	/** 추후 구역별 스폰 규칙을 위한 위치 구역 인덱스. 몬스터 그룹 인덱스와는 별개다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0"))
	int32 SpawnAreaIndex = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn")
	bool bEnabled = true;

	/** 같은 포인트에서 여러 마리를 생성할 때 NavMesh 위로 흩뿌릴 반경. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float ScatterRadius = 300.0f;

	/** 한 그룹 안에서 몬스터끼리 유지할 최소 수평 간격. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpawnSpacing = 90.0f;

	/** 낮 필드 몬스터가 이 지점에서 벗어나 추격할 수 있는 최대 수평 거리. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Leash", meta = (ClampMin = "0.0", Units = "cm"))
	float LeashRadius = 2000.0f;

	/** 낮에 플레이어를 처음 감지하는 수평 거리. 밤의 전역 추격에는 적용하지 않는다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Day AI", meta = (ClampMin = "0.0", Units = "cm"))
	float DayDetectionRadius = 1000.0f;

	/** 낮에 이미 잡은 타깃을 놓는 수평 거리. DetectionRadius 이상으로 유지된다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Day AI", meta = (ClampMin = "0.0", Units = "cm"))
	float DayLoseTargetRadius = 1400.0f;

	/** 복귀 완료로 판정할 홈 위치와의 수평 거리. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Leash", meta = (ClampMin = "1.0", Units = "cm"))
	float ReturnAcceptanceRadius = 100.0f;

	/** 복귀 중 초당 회복하는 최대 체력 비율. 5.0은 초당 최대 체력의 500%다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Leash", meta = (ClampMin = "0.0"))
	float ReturnHealthPercentPerSecond = 5.0f;

	/** 홈으로 돌아가는 동안에만 적용할 이동속도 배율. 도착·밤 전환 시 원래 속도로 복구된다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Leash", meta = (ClampMin = "1.0"))
	float ReturnMoveSpeedMultiplier = 3.0f;
};
