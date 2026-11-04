#pragma once

#include "CoreMinimal.h"
#include "ASBossSpiderStateTreeTypes.generated.h"

class AASBossSpiderWebTrap;

/**
 * WebTrap이 Boss Spider StateTree에 전달하는 서버 전용 Capture 연계 데이터다.
 * StateTree Event의 FInstancedStruct가 값을 복사하므로 다음 전이 처리까지 안전하게 유지된다.
 */
USTRUCT(BlueprintType)
struct FASBossSpiderCaptureEventPayload
{
	GENERATED_BODY()

	/** 완전 구속된 플레이어. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Spider|Capture")
	TObjectPtr<AActor> Target;

	/** Target의 구속 상태를 최종 검증하는 WebTrap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Spider|Capture")
	TObjectPtr<AASBossSpiderWebTrap> SourceTrap;
};
