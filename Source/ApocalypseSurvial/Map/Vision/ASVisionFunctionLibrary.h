#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ASVisionFunctionLibrary.generated.h"

class AActor;

/** 모든 머신에서 사용할 수 있는 시야 조회. NotifyAttackStarted만 권한이 필요하다. */
UCLASS()
class APOCALYPSESURVIAL_API UASVisionFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 첫 로컬 플레이어 Pawn의 시야 소스 팀 (없으면 0). */
	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static int32 GetLocalTeamId(const UObject* WorldContextObject);

	/**
	 * 서버에서 확인된 공격 시작. 공격자가 수풀에 숨었으면 일시적으로 노출한다.
	 * 액터에 UASVisibilityComponent가 없으면 아무것도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Map|Vision")
	static void NotifyAttackStarted(AActor* Attacker);

	/** 액터에 시야 컴포넌트가 없거나 로컬 팀에 보이면 true. */
	UFUNCTION(BlueprintPure, Category = "AS|Map|Vision")
	static bool IsActorVisibleToLocalTeam(const AActor* Actor);
};
