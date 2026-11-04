#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "ASMonsterAttackIndicatorActor.generated.h"

class USceneComponent;

/** 공격 직전 위험 범위를 표현하는 형태. 실제 피해 판정에는 사용하지 않는다. */
UENUM(BlueprintType)
enum class EASMonsterAttackIndicatorShape : uint8
{
    Circle,
    Rectangle,
    Sector
};

/**
 * 서버 Ability가 생성하는 복제형 선딜 인디케이터.
 * 시작 시각과 크기만 복제하고, 각 클라이언트가 서버 시간을 기준으로 진행률을 계산한다.
 * 시각 표현은 BP_OnIndicatorInitialized/BP_OnIndicatorProgress를 구현한 Blueprint 자식이 맡는다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASMonsterAttackIndicatorActor : public AActor
{
    GENERATED_BODY()

public:
    AASMonsterAttackIndicatorActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** FinishSpawning 전에 서버가 한 번 호출한다. */
    void InitializeIndicator(EASMonsterAttackIndicatorShape InShape, float InRadius, float InLength, float InWidth,
        float InDuration, float InAngleDegrees = 0.0f);

    /** Blueprint 능력이 이미 스폰된 인디케이터를 사각형 레인으로 초기화할 때 쓴다. 서버 전용. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Indicator")
    void InitializeRectangleIndicator(float InLength, float InWidth, float InDuration);

    // --- [추가 완료] 패턴 기반 장판 초기화 및 데미지 실행 함수 ---
    void InitializeTelegraph(const FASMonsterAttackPattern& Pattern, AActor* MonsterOwner);
    void ApplyDamageAndExecute(const FASMonsterAttackPattern& Pattern, AActor* MonsterOwner);

    UFUNCTION(BlueprintPure, Category = "Enemy|Indicator")
    float GetProgress() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Indicator")
    EASMonsterAttackIndicatorShape GetShape() const { return Shape; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Indicator")
    float GetRadius() const { return Radius; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Indicator")
    float GetLength() const { return Length; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Indicator")
    float GetWidth() const { return Width; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Indicator")
    float GetDuration() const { return Duration; }

    /** Sector 인디케이터의 전체 각도. Circle/Rectangle일 때는 0이다. */
    UFUNCTION(BlueprintPure, Category = "Enemy|Indicator")
    float GetAngleDegrees() const { return AngleDegrees; }

protected:
    /** Blueprint에서 Mesh/Decal/Niagara의 모양과 크기를 초기화한다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Indicator")
    void BP_OnIndicatorInitialized();

    /** Blueprint에서 Material Parameter 등의 진행률을 갱신한다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Indicator")
    void BP_OnIndicatorProgress(float Progress);

private:
    UFUNCTION()
    void OnRep_IndicatorConfig();

    void NotifyIndicatorInitialized();
    float GetSynchronizedWorldTime() const;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(ReplicatedUsing = OnRep_IndicatorConfig)
    EASMonsterAttackIndicatorShape Shape = EASMonsterAttackIndicatorShape::Circle;

    UPROPERTY(ReplicatedUsing = OnRep_IndicatorConfig)
    float Radius = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IndicatorConfig)
    float Length = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IndicatorConfig)
    float Width = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IndicatorConfig)
    float Duration = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IndicatorConfig)
    float AngleDegrees = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IndicatorConfig)
    float StartServerTime = 0.0f;

    bool bIndicatorInitialized = false;
};
