#include "Skills/Jaei/ASSkillForceRepulse.h"

#include "Skills/Debug/ASSkillDamageDebug.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

UASSkillForceRepulse::UASSkillForceRepulse()
{
	//기본값: 전방 부채꼴
	Angle = 90.0f;

	//밀쳐내는 스킬이라 강한 경직 (일반 몹 강한 경직, 엘리트 약한 경직, 보스 이펙트만)
	ImpactPreset = EASImpactPreset::Heavy;
	Radius = 500.0f;
	ForwardOffset = 0.0f;

	//파동이 끝날 때 직접 종료
	bEndAbilityManually = true;
}

void UASSkillForceRepulse::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility,
	bool bWasCancelled)
{
	StopWave();
	bMontageDone = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASSkillForceRepulse::OnSkillEffect_Implementation()
{
	//부모(Melee)의 한 번에 판정은 쓰지 않음 - 파동으로 대체
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	//피해·넉백은 서버에서만 (클라이언트는 몽타주만)
	if (!World || !HasSkillAuthority())
	{
		TryFinish();
		return;
	}

	//조준 방향, 시작 위치, 피해 스펙은 시전 순간 고정
	WaveDirection = GetAimDirection().GetSafeNormal2D();
	if (WaveDirection.IsNearlyZero())
	{
		WaveDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	WaveOrigin = Avatar->GetActorLocation() + WaveDirection * ForwardOffset;
	WaveDamageSpec = MakeDamageSpec();
	WaveHitTargets.Reset();
	WaveElapsed = 0.0f;
	bWaveActive = true;

	//Mass는 한 번만 (파동마다 판정하면 중복 피해)
	ApplyMassDamageOnce();

	//Actor는 파동으로: 첫 판정은 바로, 이후 간격마다
	TickWave();
	if (bWaveActive)
	{
		World->GetTimerManager().SetTimer(WaveTimerHandle, this, &UASSkillForceRepulse::TickWave, WaveTickInterval, true);
	}
}

void UASSkillForceRepulse::TickWave()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets || !bWaveActive)
	{
		StopWave();
		TryFinish();
		return;
	}

	//0 -> 1 진행도, 반경을 점점 키움
	const float Alpha = WaveDuration > 0.0f ? FMath::Clamp(WaveElapsed / WaveDuration, 0.0f, 1.0f) : 1.0f;
	const float CurrentRadius = FMath::Max(1.0f, Radius * Alpha);

	TArray<FGameplayEffectSpecHandle> EffectSpecs;
	if (WaveDamageSpec.IsValid())
	{
		EffectSpecs.Add(WaveDamageSpec);
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Cone;
	Query.SearchOrigin = WaveOrigin;
	Query.Direction = WaveDirection;
	Query.SearchRadius = CurrentRadius;
	Query.AngleDegrees = Angle;
	Query.Instigator = Avatar;
	Query.bCanHitPlayers = bCanHitPlayers;
	//Actor만 (HitSet으로 대상당 1회) - Mass는 ApplyMassDamageOnce에서
	Query.Domain = EASCombatTargetDomain::ActorOnly;

	//이번 틱에 새로 닿은 대상만 피해 (이미 맞은 대상은 HitSet이 걸러냄)
	FASCombatImpactContext ImpactContext;
	ImpactContext.Preset = ImpactPreset;
	ImpactContext.HitLocation = WaveOrigin;
	ImpactContext.HitDirection = WaveDirection;
	const FASCombatApplyResult Result = CombatTargets->ResolveAndApply(Query, EffectSpecs, ImpactContext, &WaveHitTargets);
	ASSkillDamageDebug::Report(this, EffectSpecs, Result, WaveOrigin);
	for (AActor* Target : Result.ActorTargets)
	{
		ApplyKnockback(Target);
	}

	if (ShouldDrawDebug())
	{
		DrawDebugCone(World, WaveOrigin, WaveDirection, CurrentRadius, FMath::DegreesToRadians(Angle * 0.5f), 0.05f, 16,
			Result.HasAnyHit() ? FColor::Red : FColor::Green, false, WaveTickInterval * 2.0f);
	}

	//끝까지 퍼졌으면 종료
	if (Alpha >= 1.0f)
	{
		StopWave();
		TryFinish();
		return;
	}

	WaveElapsed += WaveTickInterval;
}

void UASSkillForceRepulse::ApplyMassDamageOnce() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets || !WaveDamageSpec.IsValid())
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.Shape = EASCombatTargetShape::Cone;
	Query.SearchOrigin = WaveOrigin;
	Query.Direction = WaveDirection;
	Query.SearchRadius = Radius;
	Query.AngleDegrees = Angle;
	Query.Instigator = Avatar;
	Query.Domain = EASCombatTargetDomain::MassOnly;

	const TArray<FGameplayEffectSpecHandle> EffectSpecs = { WaveDamageSpec };
	FASCombatImpactContext ImpactContext;
	ImpactContext.Preset = ImpactPreset;
	ImpactContext.HitLocation = WaveOrigin;
	ImpactContext.HitDirection = WaveDirection;
	const FASCombatApplyResult Result = CombatTargets->ResolveAndApply(Query, EffectSpecs, ImpactContext);
	ASSkillDamageDebug::Report(this, EffectSpecs, Result, WaveOrigin);
}

void UASSkillForceRepulse::ApplyKnockback(AActor* Target) const
{
	ACharacter* TargetCharacter = Cast<ACharacter>(Target);
	UCharacterMovementComponent* Movement = TargetCharacter ? TargetCharacter->GetCharacterMovement() : nullptr;
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Movement || !Avatar || TargetCharacter == Avatar)
	{
		return;
	}

	//재이 기준 바깥쪽 (너무 가까우면 조준 방향)
	FVector Direction = (TargetCharacter->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = WaveDirection;
	}

	//범위 끝 + 여유 거리까지 밀어냄 (가까울수록 멀리)
	const float CurrentDistance = FVector::Dist2D(TargetCharacter->GetActorLocation(), Avatar->GetActorLocation());
	const float PushToDistance = ForwardOffset + Radius + PushExtraDistance;
	const float PushDistance = FMath::Max(MinPushDistance, PushToDistance - CurrentDistance);

	//포물선 이동 (루트 모션 점프 힘) - 서버에서 걸면 다른 화면에도 복제
	TSharedPtr<FRootMotionSource_JumpForce> Knockback = MakeShared<FRootMotionSource_JumpForce>();
	Knockback->InstanceName = TEXT("ForceRepulseKnockback");
	Knockback->AccumulateMode = ERootMotionAccumulateMode::Override;
	Knockback->Priority = 500;
	Knockback->Duration = KnockbackDuration;
	Knockback->Rotation = Direction.Rotation();
	Knockback->Distance = PushDistance;
	Knockback->Height = KnockbackHeight;
	Knockback->bDisableTimeout = false;
	//끝나면 그 자리에 멈춤
	Knockback->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Knockback->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

	//띄우는 경우 땅에 붙어 있지 않게
	if (KnockbackHeight > 0.0f && Movement->IsMovingOnGround())
	{
		Movement->SetMovementMode(MOVE_Falling);
	}
	Movement->ApplyRootMotionSource(Knockback);
}

void UASSkillForceRepulse::OnMontageCompleted()
{
	bMontageDone = true;
	TryFinish();
}

void UASSkillForceRepulse::StopWave()
{
	bWaveActive = false;
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Avatar->GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	}
}

void UASSkillForceRepulse::TryFinish()
{
	//몽타주가 있으면 몽타주까지 끝나야 종료
	const bool bMontageFinished = bMontageDone || !AnimMontage;
	if (IsActive() && !bWaveActive && bMontageFinished)
	{
		FinishSkill(false);
	}
}
