#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ASVisionSourceComponent.generated.h"

/**
 * 팀에 시야를 제공한다. 플레이어 캐릭터에 추가하고 추후 시야 장치에도 추가한다.
 * 서버가 대상 가시성을 판정하고 모든 머신이 같은 소스로 팀 안개를 그리므로
 * 런타임 활성 상태 (bVisionActive)를 복제한다.
 * 팀과 반경은 에디터 기본값이며 런타임에 변경하지 않는다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIAL_API UASVisionSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASVisionSourceComponent();

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	int32 GetTeamId() const { return TeamId; }

	/** cm 단위 반경. 0 이하이면 맵 정의의 기본값을 사용한다. */
	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	float GetVisionRadius(float DefaultRadius) const { return VisionRadius > 0.0f ? VisionRadius : DefaultRadius; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. 클라이언트가 서버 판정과 같은 시야를 그리도록 복제한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AS|Vision")
	void SetVisionActive(bool bActive);

	/** 활성 상태이고 소유자가 살아 있으며 DeadStateTag가 없음 (죽은 아군은 시야를 제공하지 않음). */
	bool IsProvidingVision(const FGameplayTag& DeadStateTag) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 협동 모드에서 모든 플레이어는 팀 0을 공유한다. UASVisibilityComponent 팀 마스크의 비트 인덱스 (0..7). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "7"))
	int32 TeamId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float VisionRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bVisionActive = true;

	/**
	 * 서버: 거리에 관계없이 소유자를 모든 클라이언트에 복제해, 각
	 * 팀원이 이 소스의 공유 시야와 맵 위치를 그릴 수 있게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision", meta = (AllowPrivateAccess = "true"))
	bool bMakeOwnerAlwaysRelevant = true;
};
