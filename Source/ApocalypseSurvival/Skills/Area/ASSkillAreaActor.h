#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ASSkillAreaActor.generated.h"

//일정 시간 유지되며 주기적으로 원 범위 데미지를 주는 액터

UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASSkillAreaActor : public AActor
{
	GENERATED_BODY()

public:
	AASSkillAreaActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//생성 직후(FinishSpawning 전)에 호출
	void InitArea(
		const FGameplayEffectSpecHandle& InDamageSpec,
		AActor* InInstigatorAvatar,
		float InRadius,
		float InDuration,
		float InTickInterval,
		bool bInDamageOnSpawn,
		bool bInCanHitPlayers,
		bool bInDrawDebug);

	UFUNCTION(BlueprintPure, Category = "AS|Skill|Area")
	float GetRadius() const { return Radius; }

protected:
	virtual void BeginPlay() override;

	//데미지를 줄 때마다 호출 (서버)
	UFUNCTION(BlueprintImplementableEvent, Category = "AS|Skill|Area")
	void BP_OnAreaTick(const TArray<AActor*>& HitTargets);

private:
	void DoDamageTick();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	//클라이언트 이펙트 크기 조절용
	UPROPERTY(Replicated)
	float Radius = 300.0f;

	FGameplayEffectSpecHandle DamageSpec;
	TWeakObjectPtr<AActor> InstigatorAvatar;

	float Duration = 3.0f;
	float TickInterval = 0.5f;
	bool bDamageOnSpawn = true;
	bool bCanHitPlayers = false;
	bool bDrawDebug = false;

	int32 TicksRemaining = 0;
	FTimerHandle TickTimerHandle;
};
