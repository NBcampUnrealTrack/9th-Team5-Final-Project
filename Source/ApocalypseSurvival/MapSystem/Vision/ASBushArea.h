#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASBushArea.generated.h"

class UBoxComponent;
class USceneComponent;
class UShapeComponent;

/**
 * 은폐 영역. 수풀 안의 대상은 같은 수풀에 시야 소스가 있는
 * 팀에게만 보이거나 (거리/벽 검사도 통과해야 함),
 * 공격으로 노출된 동안 보인다. 수풀 하나에 여러 도형을 사용할 수 있다.
 * 수풀은 시야 벽으로 취급하지 않는다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASBushArea : public AActor
{
	GENERATED_BODY()

public:
	AASBushArea();

	/** 레벨 내 고유 값. None이면 액터 이름을 사용한다. */
	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	FName GetBushId() const;

	UFUNCTION(BlueprintPure, Category = "AS|Vision")
	bool ContainsLocation(const FVector& Location) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bush", meta = (AllowPrivateAccess = "true"))
	FName BushId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bush", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreHeight = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> MainArea;

	TArray<TWeakObjectPtr<UShapeComponent>> AreaShapes;
};
