#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "ASSkillAreaActor.generated.h"

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

	//데미지를 줄 때마다 호출 (서버)
	UFUNCTION(BlueprintImplementableEvent, Category = "AS|Skill|Area")
	void BP_OnAreaTick(const TArray<AActor*>& HitTargets);
	
	//데미지 줄 때마다 실행할 GameplayCue (시전자 ASC로 모든 화면에 복제, 비우면 안 함)
	//큐 에셋(GameplayCueNotify)에서 이펙트·사운드 재생. Location = 장판 위치, RawMagnitude = 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Area|Cue", meta = (Categories = "GameplayCue"))
	FGameplayTag PulseCueTag;

	//데미지 줄 때마다 호출 (서버, C++ 자식 클래스 확장용)
	virtual void OnAreaTick(const FASCombatApplyResult& Result, const FVector& Center) {}

	//시전자 ASC로 GameplayCue 실행 (서버에서 부르면 모든 화면에 복제). RawMagnitude = Magnitude
	void ExecuteCueAt(const FGameplayTag& CueTag, const FVector& Location, float Magnitude) const;

	AActor* GetInstigatorAvatar() const { return InstigatorAvatar.Get(); }
	const FGameplayEffectSpecHandle& GetDamageSpec() const { return DamageSpec; }
	bool CanHitPlayers() const { return bCanHitPlayers; }
	bool ShouldDrawDebug() const;

private:
	void DoDamageTick();

	//따라가는 타겟 위치로 이동 (서버). 타겟을 잃으면 따라가기 종료
	void UpdateFollow();

	//PulseCueTag 실행 (서버)
	void ExecutePulseCue(const FVector& Center) const;

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

	float Duration = 3.0f;
	float TickInterval = 0.5f;
	bool bDamageOnSpawn = true;
	bool bCanHitPlayers = false;
	bool bDrawDebug = false;

	int32 TicksRemaining = 0;
	FTimerHandle TickTimerHandle;
};
