#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "ASMapFunctionLibrary.generated.h"

class APawn;
class UASMapDefinition;
class UASMapStateComponent;

/** 모든 머신에서 사용할 수 있는 맵 조회. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMapFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 등록 순서와 무관하게 레벨에 구역 액터가 있는지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static bool LevelHasMapZones(const UObject* WorldContextObject);

	/** 현재 GameState의 MapState 컴포넌트 (있는 경우). */
	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static UASMapStateComponent* GetMapStateComponent(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static UASMapDefinition* GetMapDefinition(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static FName FindZoneIdAtLocation(const UObject* WorldContextObject, const FVector& Location);

	/** 월드 공간 스폰 지점과 구역 정보. None이면 모든 등록 구역, 알 수 없는 ID면 빈 목록을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static TArray<FASZoneSpawnPoint> GetZoneSpawnPoints(const UObject* WorldContextObject, FName ZoneId);

	/** 월드 공간 리스폰 지점과 구역 정보. None이면 모든 등록 구역, 알 수 없는 ID면 빈 목록을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static TArray<FASZoneSpawnPoint> GetZoneRespawnPoints(const UObject* WorldContextObject, FName ZoneId);

	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static FText GetZoneDisplayName(const UObject* WorldContextObject, FName ZoneId);

	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static FLinearColor GetDangerColor(const UObject* WorldContextObject, int32 DangerLevel);

	/** 공유 스냅샷에서 가져온 구역 이름과 위험도. */
	static FASZoneHudInfo GetZoneHudInfo(const UObject* WorldContextObject, FName ZoneId);

	/** 공유 스냅샷에서 가져온 특정 위치의 구역 이름과 위험도. */
	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static FASZoneHudInfo GetZoneHudInfoAtLocation(const UObject* WorldContextObject, const FVector& Location);

	/** 첫 번째 로컬 PlayerController의 Pawn. */
	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (WorldContext = "WorldContextObject"))
	static APawn* GetLocalPlayerPawn(const UObject* WorldContextObject);
};
