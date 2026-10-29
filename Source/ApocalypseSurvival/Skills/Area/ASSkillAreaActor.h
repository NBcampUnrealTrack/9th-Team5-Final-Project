#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "ASSkillAreaActor.generated.h"

class UFXSystemAsset;
class UFXSystemComponent;
class UNiagaraSystem;

//일정 시간 유지되며 주기적으로 원 범위 데미지를 주는 액터

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASSkillAreaActor : public AActor
{
	GENERATED_BODY()

public:
	AASSkillAreaActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	//생성 직후(FinishSpawning 전)에 호출
	void InitArea(
		const FGameplayEffectSpecHandle& InDamageSpec,
		AActor* InInstigatorAvatar,
		float InRadius,
		float InDuration,
		float InTickInterval,
		bool bInDamageOnSpawn,
		bool bInCanHitPlayers,
		bool bInDrawDebug,
		const FASCombatImpactContext& InImpactContext = FASCombatImpactContext());

	//데미지와 같이 걸 추가 GE (슬로우 등, Actor만 적용 - Mass는 GE를 못 받음). FinishSpawning 전에 호출
	void SetExtraEffectSpecs(const TArray<FGameplayEffectSpecHandle>& InEffectSpecs);

	//이 타겟(Actor/Mass)을 따라다님. 타겟이 죽거나 사라지면 그 자리에 멈춤. FinishSpawning 전에 호출 (서버)
	void SetFollowTarget(const FASCombatTargetRef& InTarget);

	UFUNCTION(BlueprintPure, Category = "AS|Skill|Area")
	float GetRadius() const { return Radius; }

	UFUNCTION(BlueprintPure, Category = "AS|Skill|Area")
	bool IsFollowingTarget() const { return FollowTarget.IsValid(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//데미지를 줄 때마다 호출 (서버)
	UFUNCTION(BlueprintImplementableEvent, Category = "AS|Skill|Area")
	void BP_OnAreaTick(const TArray<AActor*>& HitTargets);
	
	//데미지 줄 때마다 각 화면에서 직접 재생할 Niagara (GameplayCue 없이, 비우면 안 함)
	//클라이언트는 복제받은 반경·간격으로 서버와 같은 횟수만큼 재생
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area|VFX")
	TObjectPtr<UNiagaraSystem> PulseNiagara;

	//PulseNiagara 크기 기준 반경 (Scale = Radius / 이 값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area|VFX", meta = (ClampMin = "1.0"))
	float PulseNiagaraBaseRadius = 100.0f;

	//장판이 있는 동안 계속 재생할 이펙트 (Niagara·Cascade 루프 에셋, 비우면 안 함)
	//각 화면에서 직접 재생, 장판을 따라다니고 장판이 사라지면 서서히 꺼짐
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area|VFX")
	TObjectPtr<UFXSystemAsset> LoopFX;

	//LoopFX 크기 기준 반경 (Scale = Radius / 이 값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area|VFX", meta = (ClampMin = "1.0"))
	float LoopFXBaseRadius = 100.0f;

	//이펙트(Pulse·Loop·Burst)를 바닥에 붙여서 재생 (장판 아래로 지형을 찾음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area|VFX")
	bool bSnapFXToGround = true;

	//이펙트 높이 보정 (바닥 기준, 음수 = 아래로). 이펙트 자체가 떠 있게 만들어진 경우 조절
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area|VFX", meta = (Units = "cm"))
	float FXHeightOffset = 0.0f;

	//데미지 줄 때마다 호출 (서버, C++ 자식 클래스 확장용)
	virtual void OnAreaTick(const FASCombatApplyResult& Result, const FVector& Center) {}

	//시전자 ASC로 GameplayCue 실행 (서버에서 부르면 모든 화면에 복제). RawMagnitude = Magnitude
	void ExecuteCueAt(const FGameplayTag& CueTag, const FVector& Location, float Magnitude) const;

	AActor* GetInstigatorAvatar() const { return InstigatorAvatar.Get(); }
	const FGameplayEffectSpecHandle& GetDamageSpec() const { return DamageSpec; }
	bool CanHitPlayers() const { return bCanHitPlayers; }
	bool ShouldDrawDebug() const;

	//이펙트 재생 위치 (bSnapFXToGround면 바닥 높이, + FXHeightOffset)
	FVector GetFXLocation() const;

private:
	void DoDamageTick();

	//따라가는 타겟 위치로 이동 (서버). 타겟을 잃으면 따라가기 종료
	void UpdateFollow();

	//PulseNiagara를 이 화면에서 주기적으로 재생 시작 (서버·클라 각각, 데디서버 제외)
	void StartLocalPulseVFX();
	void PlayLocalPulseVFX();

	//LoopFX 시작·종료 (서버·클라 각각, 데디서버 제외)
	void StartLoopFX();
	void StopLoopFX();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	//클라이언트 이펙트 크기 조절용
	UPROPERTY(Replicated)
	float Radius = 300.0f;

	FGameplayEffectSpecHandle DamageSpec;
	TArray<FGameplayEffectSpecHandle> ExtraEffectSpecs;
	FASCombatImpactContext ImpactContext;
	TWeakObjectPtr<AActor> InstigatorAvatar;

	//따라가는 타겟 (비어 있으면 고정 장판)
	FASCombatTargetRef FollowTarget;

	//클라이언트 이펙트 횟수·간격 계산용
	UPROPERTY(Replicated)
	float Duration = 3.0f;

	UPROPERTY(Replicated)
	float TickInterval = 0.5f;

	UPROPERTY(Replicated)
	bool bDamageOnSpawn = true;

	bool bCanHitPlayers = false;
	bool bDrawDebug = false;

	int32 TicksRemaining = 0;
	FTimerHandle TickTimerHandle;

	int32 VFXPulsesRemaining = 0;
	FTimerHandle VFXTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UFXSystemComponent> LoopFXComponent;
};
