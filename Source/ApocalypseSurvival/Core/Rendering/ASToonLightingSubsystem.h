#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASToonLightingSubsystem.generated.h"

class UDirectionalLightComponent;
class UMaterialParameterCollection;

/**
 * 툰 셰이딩용 조명 정보를 머티리얼 파라미터 컬렉션(MPC_ToonLighting)에 매 프레임 써 넣는다.
 *
 * 캐릭터 툰 머티리얼은 Unlit 이라 엔진 조명을 받지 않는다. 대신 이 서브시스템이
 * 월드의 디렉셔널 라이트(낮밤 조명 액터의 태양)를 찾아 방향·색을 MPC 에 올리고,
 * 머티리얼이 그 값으로 명암을 직접 계산한다. 낮밤이 바뀌면 캐릭터 명암도 같이 바뀐다.
 *
 * 디렉셔널 라이트가 없는 맵에서는 MPC 기본값이 그대로 쓰인다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASToonLightingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UASToonLightingSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickableInEditor() const override { return true; }

private:
	/** 라이트는 바뀔 수 있으므로(낮밤 액터 스폰 등) 잃어버리면 다시 찾는다. */
	UDirectionalLightComponent* FindSunLight();

	TWeakObjectPtr<UDirectionalLightComponent> CachedSun;
	TWeakObjectPtr<UMaterialParameterCollection> Collection;
	float RetryTimer = 0.0f;
};
