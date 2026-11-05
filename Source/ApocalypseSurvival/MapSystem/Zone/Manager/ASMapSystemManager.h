#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapSystem/Zone/Core/ASMapRuleEngine.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "ASMapSystemManager.generated.h"

class AASMapSystemManager;
class AASMapZone;
class UASMapDefinition;
class UASMapStateComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASManagerZoneStateChangedSignature, FName, ZoneId, const FASZoneRuntimeState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASManagerMapReadySignature, AASMapSystemManager*, Manager);

/**
 * 서버 전용 맵 규칙 실행기.
 * InitializeMap -> (OnMapReady) -> StartMap -> HandlePhaseChanged ... -> StopMap.
 * 등록된 구역 액터를 검증하고 낮/밤 시작에 위험도 규칙을 적용한다.
 * 현재 상태는 GameState의 UASMapStateComponent에만 보관한다.
 */
UCLASS(Blueprintable, NotPlaceable)
class APOCALYPSESURVIVAL_API AASMapSystemManager : public AActor
{
	GENERATED_BODY()

public:
	AASMapSystemManager();

	/**
	 * 설정과 공유 상태를 연결하고 구역을 등록·검증한다.
	 * 성공하면 OnMapReady를 브로드캐스트한다. 실패하면 오류를 로그와 GetValidationErrors에 남긴다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Map")
	bool InitializeMap(UASMapDefinition* InMapDefinition, UASMapStateComponent* InMapState);

	/** 새 매치의 런타임 상태를 초기화하고 처리를 시작한다. 맵이 준비되어 있어야 한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Map")
	bool StartMap(int32 InMatchSeed);

	/** GameState가 결정한 스텝 (날짜, 낮/밤). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Map")
	void HandlePhaseChanged(const FASMapPhaseContext& PhaseContext);

	/** 처리를 중지한다. 최종 구역 상태는 결과 UI용으로 유지한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Map")
	void StopMap();

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	bool GetZoneState(FName ZoneId, FASZoneRuntimeState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FName FindZoneAtLocation(const FVector& Location) const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	bool IsMapReady() const { return bMapReady; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	bool IsMapRunning() const { return bMapRunning; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	int32 GetMatchSeed() const { return MatchSeed; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	TArray<FString> GetValidationErrors() const { return ValidationErrors; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	UASMapDefinition* GetMapDefinition() const { return MapDefinition; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	UASMapStateComponent* GetMapState() const { return MapState.Get(); }

	/** 현재 상태를 로그에 기록한다. */
	UFUNCTION(BlueprintCallable, Category = "AS|Map|Debug")
	void DumpState() const;

	UPROPERTY(BlueprintAssignable, Category = "AS|Map")
	FASManagerZoneStateChangedSignature OnZoneStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "AS|Map")
	FASManagerMapReadySignature OnMapReady;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 수정한 스냅샷을 MapState에 다시 기록한 뒤 매니저 이벤트를 발생시킨다. */
	void CommitEvents(const FASMapStateSnapshot& WorkingState, const FASMapEngineEvents& Events);
	void WriteSnapshot(const FASMapStateSnapshot& WorkingState);
	bool ValidateLevel(const TArray<FASZoneConfig>& ZoneConfigs, TArray<FString>& OutErrors) const;
	/** 양의 면적으로 겹치는 구역 쌍을 경고한다. 초기화 검증 오류에는 포함하지 않는다. */
	void LogZoneOverlaps() const;
	void CollectLevelZones() const;
	/** 등록된 액터의 구역 설정을 만든다. 연결은 양방향 합집합으로 병합한다. */
	void BuildZoneConfigs(TArray<FASZoneConfig>& OutConfigs) const;
	/** 호출 시점의 구역별 생존 몬스터 수와 플레이어 존재를 서버에서 센다. */
	TMap<FName, FASZonePhaseCounts> BuildPhaseCounts() const;

	void HandleZoneRegistered(AASMapZone* Zone);
	void HandleZoneUnregistered(AASMapZone* Zone);

	UPROPERTY(Transient)
	TObjectPtr<UASMapDefinition> MapDefinition;

	UPROPERTY(Transient)
	TWeakObjectPtr<UASMapStateComponent> MapState;

	FASMapRuleEngine RuleEngine;
	TArray<FString> ValidationErrors;
	FDelegateHandle ZoneRegisteredHandle;
	FDelegateHandle ZoneUnregisteredHandle;
	int32 MatchSeed = 0;
	bool bMapReady = false;
	bool bMapRunning = false;
};
