#include "Skills/Jaei/ASZeroGravityAreaActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Debug/ASSkillDamageDebug.h"

void AASZeroGravityAreaActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AASZeroGravityAreaActor, ReplicatedBurstRadius);
}

void AASZeroGravityAreaActor::InitZeroGravity(
	const FGameplayEffectSpecHandle& InStunSpec,
	float InStunChance,
	int32 InStunRollCount,
	float InStunRollInterval,
	const FGameplayEffectSpecHandle& InBurstSpec,
	float InBurstBonusPerTarget,
	float InBurstRadiusPerTarget,
	int32 InMaxBurstTargets,
	EASImpactPreset InBurstImpactPreset)
{
	StunSpec = InStunSpec;
	StunChance = FMath::Clamp(InStunChance, 0.0f, 1.0f);
	StunRollCount = FMath::Max(0, InStunRollCount);
	StunRollInterval = FMath::Max(0.0f, InStunRollInterval);
	BurstSpec = InBurstSpec;
	BurstBonusPerTarget = FMath::Max(0.0f, InBurstBonusPerTarget);
	BurstRadiusPerTarget = FMath::Max(0.0f, InBurstRadiusPerTarget);
	MaxBurstTargets = FMath::Max(0, InMaxBurstTargets);
	BurstImpactPreset = InBurstImpactPreset;
}

void AASZeroGravityAreaActor::OnAreaTick(const FASCombatApplyResult& Result, const FVector& Center)
{
	LastHitCount = Result.GetTotalHitCount();

	//클라이언트도 폭발 크기를 알 수 있게 (폭발은 마지막 틱 기준이라 값이 같음)
	ReplicatedBurstRadius = CalcBurstRadius(LastHitCount);

	for (AActor* Target : Result.ActorTargets)
	{
		TryStun(Target);
	}
}

void AASZeroGravityAreaActor::TryStun(AActor* Target)
{
	if (!Target || !StunSpec.IsValid() || !GetWorld())
	{
		return;
	}

	//이미 스턴했거나, 횟수를 다 썼거나, 다음 판정 시간이 아직이면 넘어감
	FStunRollState& RollState = StunRolls.FindOrAdd(Target);
	const double Now = GetWorld()->GetTimeSeconds();
	if (RollState.bStunned || RollState.RollCount >= StunRollCount || Now < RollState.NextRollTime)
	{
		return;
	}

	++RollState.RollCount;
	RollState.NextRollTime = Now + StunRollInterval;

	const bool bSuccess = FMath::FRand() < StunChance;

	if (ShouldDrawDebug())
	{
		DrawDebugString(GetWorld(), Target->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f),
			FString::Printf(TEXT("Roll %d/%d %s"), RollState.RollCount, StunRollCount, bSuccess ? TEXT("O") : TEXT("X")),
			nullptr, bSuccess ? FColor::Yellow : FColor::White, 1.5f, true, 1.2f);
	}

	if (!bSuccess)
	{
		return;
	}

	//스턴 GE (State.Monster.HitReact 태그 → 몬스터 AI가 이동·공격을 멈춤)
	if (UASCombatTargetSubsystem::ApplyEffectSpecToActor(StunSpec, Target) <= 0)
	{
		return;
	}
	RollState.bStunned = true;

	//이미 시작한 공격도 끊음 (태그는 새 공격만 막음)
	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		FGameplayTagContainer AttackTags(ASGameplayTags::Ability_Monster_Attack);
		TargetASC->CancelAbilities(&AttackTags);
	}

	ExecuteCueAt(StunCueTag, Target->GetActorLocation(), 0.0f);
	BP_OnTargetStunned(Target);

	if (ShouldDrawDebug())
	{
		DrawDebugString(GetWorld(), Target->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), TEXT("STUN"),
			nullptr, FColor::Yellow, 1.5f, true, 1.4f);
	}
}

void AASZeroGravityAreaActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//시간이 다 돼서 사라질 때만 폭발 (레벨 종료 등은 제외)
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		if (HasAuthority())
		{
			DoBurst();
		}

		//폭발 이펙트는 각 화면에서 직접 (클라이언트는 서버가 장판을 지울 때 같이 사라짐)
		SpawnBurstVFX();
	}

	Super::EndPlay(EndPlayReason);
}

void AASZeroGravityAreaActor::DoBurst()
{
	if (bBurstDone || !BurstSpec.IsValid())
	{
		return;
	}
	bBurstDone = true;

	UWorld* World = GetWorld();
	UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets)
	{
		return;
	}

	//모인 수만큼 강화 (최대 MaxBurstTargets명까지)
	const int32 Count = FMath::Min(LastHitCount, MaxBurstTargets);
	const float DamageMultiplier = 1.0f + BurstBonusPerTarget * Count;
	const float BurstRadius = CalcBurstRadius(LastHitCount);
	ReplicatedBurstRadius = BurstRadius;

	//원본 Spec은 그대로 두고 복사해서 피해만 바꿈
	FGameplayEffectSpecHandle ScaledSpec(new FGameplayEffectSpec(*BurstSpec.Data.Get()));
	const float BaseDamage = BurstSpec.Data->GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f);
	ScaledSpec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, BaseDamage * DamageMultiplier);

	const FVector Center = GetActorLocation();
	const TArray<FGameplayEffectSpecHandle> Specs = { ScaledSpec };

	FASCombatTargetQuery Query;
	Query.SearchOrigin = Center;
	Query.SearchRadius = BurstRadius;
	Query.Instigator = GetInstigatorAvatar();
	Query.bCanHitPlayers = CanHitPlayers();
	//폭발 경직 (Mass 포함, 등급별로 약해짐)
	FASCombatImpactContext BurstContext;
	BurstContext.Preset = BurstImpactPreset;
	BurstContext.HitLocation = Center;
	const FASCombatApplyResult Result = CombatTargets->ResolveAndApply(Query, Specs, BurstContext);

	ASSkillDamageDebug::Report(this, Specs, Result, Center);

	if (ShouldDrawDebug())
	{
		DrawDebugSphere(World, Center, BurstRadius, 24, FColor::Purple, false, 1.5f);
		DrawDebugString(World, Center + FVector(0.0f, 0.0f, 150.0f),
			FString::Printf(TEXT("BURST x%.1f (%d)"), DamageMultiplier, Count), nullptr, FColor::Purple, 1.5f, true, 1.4f);
	}
}

float AASZeroGravityAreaActor::CalcBurstRadius(int32 HitCount) const
{
	const int32 Count = FMath::Min(HitCount, MaxBurstTargets);
	return GetRadius() * (1.0f + BurstRadiusPerTarget * Count);
}

void AASZeroGravityAreaActor::SpawnBurstVFX() const
{
	//데디서버는 화면이 없음
	if (!BurstNiagara || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float BurstRadius = ReplicatedBurstRadius > 0.0f ? ReplicatedBurstRadius : GetRadius();
	const float Scale = BurstRadius / FMath::Max(1.0f, BurstNiagaraBaseRadius);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BurstNiagara, GetFXLocation(), FRotator::ZeroRotator, FVector(Scale));
}
