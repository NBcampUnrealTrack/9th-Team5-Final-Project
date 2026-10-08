#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ASVisionEvaluatorComponent.generated.h"

class UASVisionDefinition;
class UASVisibilityComponent;
class UASVisionSourceComponent;

/** 서버 팀 시야 평가와 로컬 안개용 공유 벽 트레이스. */
UCLASS(ClassGroup = (AS))
class APOCALYPSESURVIVAL_API UASVisionEvaluatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASVisionEvaluatorComponent();

	void Configure(UASVisionDefinition* Definition);
	void ServerEvaluateVisibility();
	bool CanSourceSeeTarget(
		const UASVisionSourceComponent* Source,
		const UASVisibilityComponent* Target,
		double ServerTime,
		bool bWallOcclusion,
		float NightAlpha) const;

	bool IsBlockedByWall(const FVector& From, const FVector& To, const AActor* IgnoreA, const AActor* IgnoreB) const;
	FVector GetTraceLocation(const AActor* Actor) const;
	bool TraceVisionWall(FHitResult& Hit, const FVector& From, const FVector& To, const AActor* IgnoreActor) const;

	/** 매니저의 낮/밤 전환과 서버 가시성 판정이 함께 사용하는 서버 시간. */
	double GetServerTimeSeconds() const;

private:
	/** 시야 벽이 차단하는 트레이스 채널 (Project Settings > Collision > ASVisionWall). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> VisionWallChannel = ECC_GameTraceChannel1;

	UPROPERTY(Transient)
	TObjectPtr<UASVisionDefinition> VisionDefinition;
};
