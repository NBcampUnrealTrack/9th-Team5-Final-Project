#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASMonsterBeamTelegraphActor.generated.h"

class USceneComponent;

/** 저격 빔 텔레그래프의 시각 단계. 피해 판정은 Ability 서버 코드가 담당한다. */
UENUM(BlueprintType)
enum class EASMonsterBeamTelegraphPhase : uint8
{
	Tracking,
	Locked
};

/**
 * 시작점, 길이, 폭, 단계와 서버 시간을 복제하는 저격 빔 표시 Actor.
 * Blueprint 자식은 아래 값과 진행률만 읽어 레이저/데칼/Niagara를 표시한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASMonsterBeamTelegraphActor : public AActor
{
	GENERATED_BODY()

public:
	AASMonsterBeamTelegraphActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeBeam(const FVector& InStartLocation, const FVector& InDirection, float InLength,
		float InWidth, float InTotalDuration, float InTrackingDuration);
	void UpdateTrackingTransform(const FVector& InStartLocation, const FVector& InDirection, float InLength);
	void SetPhase(EASMonsterBeamTelegraphPhase InPhase);
	void NotifyBeamFired();

	UFUNCTION(BlueprintPure, Category = "Enemy|SniperBeam")
	float GetLength() const { return Length; }

	UFUNCTION(BlueprintPure, Category = "Enemy|SniperBeam")
	float GetWidth() const { return Width; }

	UFUNCTION(BlueprintPure, Category = "Enemy|SniperBeam")
	float GetProgress() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|SniperBeam")
	EASMonsterBeamTelegraphPhase GetPhase() const { return Phase; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SniperBeam")
	void BP_OnBeamInitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SniperBeam")
	void BP_OnBeamProgress(float Progress);

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SniperBeam")
	void BP_OnBeamFired();

private:
	UFUNCTION()
	void OnRep_BeamConfig();

	void NotifyBeamInitialized();
	float GetSynchronizedWorldTime() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(ReplicatedUsing = OnRep_BeamConfig)
	float Length = 2200.0f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamConfig)
	float Width = 90.0f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamConfig)
	float TotalDuration = 1.5f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamConfig)
	float TrackingDuration = 1.15f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamConfig)
	float StartServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamConfig)
	EASMonsterBeamTelegraphPhase Phase = EASMonsterBeamTelegraphPhase::Tracking;

	bool bInitialized = false;
};
