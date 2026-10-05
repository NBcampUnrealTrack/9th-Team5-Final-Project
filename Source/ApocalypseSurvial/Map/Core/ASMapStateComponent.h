#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Map/Core/ASMapTypes.h"
#include "ASMapStateComponent.generated.h"

class UASMapDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASOnZoneStateChangedSignature, FName, ZoneId, const FASZoneRuntimeState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASOnMapStateEventSignature);

/**
 * 공유 맵 상태를 보관하는 유일한 곳. GameState의 기본 서브오브젝트.
 *
 * 서버는 전체 스냅샷을 기록한다. 클라이언트는 이를 단일 복제
 * 프로퍼티로 받고, 컴포넌트가 차이를 이벤트로 변환한다. 서버는
 * 같은 이벤트를 명시적으로 발생시켜 리슨 서버 호스트의 UI도 갱신한다.
 *
 * UI는 먼저 GetSnapshot()을 읽고 (늦은 참가, 위젯 재개방) 이후
 * 구독해야 한다. 이벤트만으로는 상태의 기준이 될 수 없다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIAL_API UASMapStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASMapStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FASMapStateSnapshot& GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "AS|Map", meta = (DisplayName = "Get Map Snapshot"))
	FASMapStateSnapshot GetSnapshotCopy() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	bool GetZoneState(FName ZoneId, FASZoneRuntimeState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	bool IsMapReady() const { return Snapshot.bMapReady; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	UASMapDefinition* GetMapDefinition() const { return MapDefinition; }

	/** 동기화된 서버 시간 (GameState::GetServerWorldTimeSeconds). */
	UFUNCTION(BlueprintPure, Category = "AS|Map")
	double GetServerTimeSeconds() const;

	// 서버 측 기록. 맵 매니저만 호출하는 것을 전제로 한다.

	void ServerSetMapDefinition(UASMapDefinition* InMapDefinition);

	/** 스냅샷을 교체하고 차이를 로컬에 브로드캐스트한다. 서버 전용. */
	void ServerWriteSnapshot(const FASMapStateSnapshot& NewSnapshot);

	UPROPERTY(BlueprintAssignable, Category = "AS|Map")
	FASOnZoneStateChangedSignature OnZoneStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "AS|Map")
	FASOnMapStateEventSignature OnMapReady;

	/** 스냅샷이 변경되면 개별 이벤트 이후에 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "AS|Map")
	FASOnMapStateEventSignature OnMapStateUpdated;

	/** MatchId 변경: 매치별 클라이언트 데이터 (탐색 안개)를 지워야 한다. */
	UPROPERTY(BlueprintAssignable, Category = "AS|Map")
	FASOnMapStateEventSignature OnMatchReset;

private:
	UFUNCTION()
	void OnRep_Snapshot(const FASMapStateSnapshot& OldSnapshot);

	void BroadcastDifference(const FASMapStateSnapshot& OldSnapshot, const FASMapStateSnapshot& NewSnapshot);

	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FASMapStateSnapshot Snapshot;

	UPROPERTY(Replicated)
	TObjectPtr<UASMapDefinition> MapDefinition;
};
