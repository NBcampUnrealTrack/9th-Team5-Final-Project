#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASVisibilityComponent.generated.h"

class UPrimitiveComponent;

/** 미니맵 / 확장 맵에서 대상을 그리는 방식. */
UENUM(BlueprintType)
enum class EASMapMarkerType : uint8
{
	None,
	Enemy,
	Elite,
	Objective,
	Interactable
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASOnLocalVisibilityChangedSignature, bool, bVisible);

/**
 * 전장의 안개가 숨길 수 있는 적과 상호작용 대상에 추가한다.
 *
 * 서버가 팀별 가시성 (거리, 벽, 수풀, 공격
 * 노출)을 결정하고 팀 비트 마스크를 복제한다. 각 머신은 소유자의
 * 메시와 위젯 (체력 바)을 로컬에서 숨긴다. 숨김은 충돌, AI
 * 감지, 히트 판정을 건드리지 않는다. 타기팅과 맵 코드는
 * IsVisibleToLocalTeam()을 호출해야 한다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASVisibilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASVisibilityComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버: 최신 시야 평가 결과. */
	void ServerSetVisibleTeamMask(uint8 NewMask);

	/** 서버: 수풀 은폐를 일시 해제한다. 거리와 벽은 계속 적용된다. 음수이면 맵 기본값을 사용한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Vision")
	void NotifyAttackStarted(float RevealSeconds = -1.0f);

	/** 수풀 은폐 해제가 유지되는 서버 시간. */
	double GetRevealEndServerTime() const { return RevealEndServerTime; }

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsVisibleToTeam(int32 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsVisibleToLocalTeam() const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool IsAlwaysVisible() const { return bAlwaysVisible; }

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	EASMapMarkerType GetMarkerType() const { return MarkerType; }

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool ShouldShowOnMap() const { return bShowOnMap && MarkerType != EASMapMarkerType::None; }

	/** 로컬 표시를 다시 적용한다 (새 시각 액터 부착 후 호출). */
	UFUNCTION(BlueprintCallable, Category = "AS|Vision")
	void RefreshLocalPresentation();

	/** 로컬 팀에게 이 대상이 나타나거나 사라질 때 각 머신에서 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "AS|Vision")
	FASOnLocalVisibilityChangedSignature OnLocalVisibilityChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_VisibleTeamMask();

	void UpdateLocalPresentation(bool bForce);
	void SetOwnerPresentationVisible(bool bVisible);

	/** 안개가 숨기지 않는 목표물과 유사한 대상. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bAlwaysVisible = false;

	/** 보이지 않는 동안 소유자와 부착 액터의 메시 및 위젯을 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bHideOwnerWhenNotVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	EASMapMarkerType MarkerType = EASMapMarkerType::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bShowOnMap = true;

	/**
	 * 서버: 어느 팀이든 보는 동안 소유자의 네트워크 관련성을 모든 클라이언트에 유지해,
	 * 먼 팀원이 발견한 대상도 이 클라이언트의 맵에 도달하게 한다.
	 * 팀 시야 밖에서는 소유자의 일반적인 (거리 기반) 관련성으로 돌아간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bRelevantWhileTeamVisible = true;

	/** 비트 N 설정 = 팀 N에 보임. */
	UPROPERTY(ReplicatedUsing = OnRep_VisibleTeamMask)
	uint8 VisibleTeamMask = 0;

	double RevealEndServerTime = 0.0;
	bool bOwnerAlwaysRelevantByDefault = false;
	bool bLocallyVisible = true;
	bool bPresentationInitialized = false;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenPrimitives;
};
