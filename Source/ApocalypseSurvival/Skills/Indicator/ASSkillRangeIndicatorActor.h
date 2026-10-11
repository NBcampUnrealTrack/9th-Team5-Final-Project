#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Skills/Indicator/ASSkillIndicatorTypes.h"
#include "ASSkillRangeIndicatorActor.generated.h"

class APawn;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * 스킬 조준 중 바닥에 범위를 표시하는 로컬 전용 액터 (복제 안 함)
 * RangeDecal = 사거리 원 (시전자 중심), AreaDecal = 실제 범위 (원·부채꼴·직선)
 * 본인 화면에서만 생성하고 매 프레임 시전자와 커서를 따라감
 * 머티리얼과 색은 BP 자식에서 지정
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASSkillRangeIndicatorActor : public AActor
{
	GENERATED_BODY()

public:
	AASSkillRangeIndicatorActor();

	virtual void Tick(float DeltaSeconds) override;

	//표시 시작. 스폰 직후 한 번 호출
	void InitIndicator(APawn* InCaster, const FASSkillIndicatorSettings& InSettings);

	//현재 조준 방향 (2D)
	UFUNCTION(BlueprintPure, Category = "AS|Skill|Indicator")
	FVector GetAimDirection() const { return AimDirection; }

	//현재 목표 지점 (위치형: 사거리 안으로 제한된 커서 위치)
	UFUNCTION(BlueprintPure, Category = "AS|Skill|Indicator")
	FVector GetAimTargetLocation() const { return TargetLocation; }

	//커서 아래 바닥 위치 (로컬 플레이어만 성공)
	static bool GetCursorWorldLocation(const APawn* Caster, FVector& OutLocation);

	//목표 지점을 사거리 안으로 제한하고 바닥 높이에 맞춤 (스킬·인디케이터 공용)
	static FVector ResolveTargetLocation(const AActor* Caster, const FVector& DesiredLocation, float MaxCastRange);

protected:
	//초기화 직후 호출. BP에서 색 등 머티리얼 파라미터 추가 설정
	UFUNCTION(BlueprintImplementableEvent, Category = "AS|Skill|Indicator")
	void BP_OnIndicatorInitialized();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	//사거리 원
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDecalComponent> RangeDecal;

	//실제 범위 (원·부채꼴·직선)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDecalComponent> AreaDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator")
	TObjectPtr<UMaterialInterface> RangeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator")
	TObjectPtr<UMaterialInterface> CircleMaterial;

	//비우면 CircleMaterial 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator")
	TObjectPtr<UMaterialInterface> ConeMaterial;

	//비우면 CircleMaterial 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator")
	TObjectPtr<UMaterialInterface> LineMaterial;

	//부채꼴 각도(도)를 넘길 머티리얼 스칼라 파라미터 이름 (부채꼴은 데칼 앞쪽을 향하게 그림)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator")
	FName ConeAngleParameterName = TEXT("Angle");

	//데칼 투영 깊이 (경사·계단에서 끊기지 않게)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator", meta = (ClampMin = "1.0", Units = "cm"))
	float DecalDepth = 200.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AS|Skill|Indicator")
	FASSkillIndicatorSettings Settings;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AS|Skill|Indicator")
	TObjectPtr<UMaterialInstanceDynamic> RangeMID;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AS|Skill|Indicator")
	TObjectPtr<UMaterialInstanceDynamic> AreaMID;

private:
	//시전자·커서 위치로 데칼 위치와 방향 갱신
	void UpdateIndicator();

	UMaterialInterface* GetAreaMaterial() const;

	TWeakObjectPtr<APawn> Caster;

	FVector AimDirection = FVector::ForwardVector;
	FVector TargetLocation = FVector::ZeroVector;

	//커서가 UI 위 등으로 안 잡힐 때 쓰는 마지막 위치
	FVector LastCursorLocation = FVector::ZeroVector;
	bool bHasCursorLocation = false;
};
