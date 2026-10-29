#include "Skills/Area/ASSkillAreaActor.h"

#include "Skills/Debug/ASSkillDamageDebug.h"
#include "Skills/ASSkillFX.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AASSkillAreaActor::AASSkillAreaActor()
{
	//따라가기용 Tick (따라갈 타겟이 있을 때 서버에서만 켬)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AASSkillAreaActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AASSkillAreaActor, Radius);
	DOREPLIFETIME_CONDITION(AASSkillAreaActor, Duration, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AASSkillAreaActor, TickInterval, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AASSkillAreaActor, bDamageOnSpawn, COND_InitialOnly);
}

void AASSkillAreaActor::InitArea(
	const FGameplayEffectSpecHandle& InDamageSpec,
	AActor* InInstigatorAvatar,
	float InRadius,
	float InDuration,
	float InTickInterval,
	bool bInDamageOnSpawn,
	bool bInCanHitPlayers,
	bool bInDrawDebug,
	const FASCombatImpactContext& InImpactContext)
{
	DamageSpec = InDamageSpec;
	ImpactContext = InImpactContext;
	InstigatorAvatar = InInstigatorAvatar;
	Radius = InRadius;
	Duration = FMath::Max(0.05f, InDuration);
	TickInterval = FMath::Max(0.05f, InTickInterval);
	bDamageOnSpawn = bInDamageOnSpawn;
	bCanHitPlayers = bInCanHitPlayers;
	bDrawDebug = bInDrawDebug;
}

void AASSkillAreaActor::SetExtraEffectSpecs(const TArray<FGameplayEffectSpecHandle>& InEffectSpecs)
{
	ExtraEffectSpecs = InEffectSpecs;
}

void AASSkillAreaActor::SetFollowTarget(const FASCombatTargetRef& InTarget)
{
	FollowTarget = InTarget;
}

void AASSkillAreaActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateFollow();
}

void AASSkillAreaActor::UpdateFollow()
{
	if (!HasAuthority() || !FollowTarget.IsValid())
	{
		return;
	}

	UASCombatTargetSubsystem* CombatTargets = GetWorld() ? GetWorld()->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets)
	{
		return;
	}

	FASCombatTargetQuery Query;
	Query.Instigator = InstigatorAvatar.Get();
	Query.bCanHitPlayers = bCanHitPlayers;

	//타겟이 죽었거나 사라짐 -> 그 자리에 남음
	if (!CombatTargets->RefreshTarget(Query, FollowTarget))
	{
		FollowTarget.Reset();
		SetActorTickEnabled(false);
		return;
	}

	FVector NewLocation = FollowTarget.GetLocation();

	//Actor는 위치가 몸 중앙이라 발밑으로 내림 (Mass는 이미 발밑 기준)
	if (const AActor* TargetActor = FollowTarget.GetActor())
	{
		NewLocation.Z -= TargetActor->GetSimpleCollisionHalfHeight();
	}

	SetActorLocation(NewLocation);
}

void AASSkillAreaActor::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		//클라이언트는 이펙트만
		StartLocalPulseVFX();
		StartLoopFX();
		return;
	}

	//따라갈 타겟이 있으면 첫 데미지 전에 위치를 맞추고 Tick 켜기
	if (FollowTarget.IsValid())
	{
		UpdateFollow();
		SetActorTickEnabled(FollowTarget.IsValid());
	}

	//예: 3초 / 0.5초 = 6번
	TicksRemaining = FMath::Max(1, FMath::FloorToInt(Duration / TickInterval + KINDA_SMALL_NUMBER));

	if (bDamageOnSpawn)
	{
		DoDamageTick();
	}

	if (TicksRemaining > 0)
	{
		GetWorldTimerManager().SetTimer(TickTimerHandle, this, &AASSkillAreaActor::DoDamageTick, TickInterval, true);
	}

	//마지막 틱이 끝난 뒤 사라지도록 약간 여유
	SetLifeSpan(Duration + 0.05f);

	//리슨서버 호스트 화면 이펙트 (따라가기 위치를 맞춘 뒤)
	StartLocalPulseVFX();
	StartLoopFX();
}

void AASSkillAreaActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLoopFX();

	Super::EndPlay(EndPlayReason);
}

void AASSkillAreaActor::StartLoopFX()
{
	//데디서버는 화면이 없음
	if (!LoopFX || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	//장판 소유가 아닌 컴포넌트로 만들어서 붙임 (장판이 사라져도 바로 지워지지 않고 서서히 꺼지게)
	const float Scale = Radius / FMath::Max(1.0f, LoopFXBaseRadius);
	LoopFXComponent = ASSkillFX::SpawnAtLocation(this, LoopFX, GetFXLocation(), FRotator::ZeroRotator, FVector(Scale));
	if (LoopFXComponent)
	{
		//SpawnAtLocation은 절대 좌표로 고정돼서 붙여도 안 따라옴 -> 상대 좌표로 풀고 붙임
		LoopFXComponent->SetAbsolute(false, false, false);
		LoopFXComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AASSkillAreaActor::StopLoopFX()
{
	if (!LoopFXComponent)
	{
		return;
	}

	//새 입자만 멈추고 남은 입자는 끝까지 재생 (다 끝나면 자동 삭제)
	LoopFXComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	LoopFXComponent->Deactivate();
	LoopFXComponent = nullptr;
}

void AASSkillAreaActor::StartLocalPulseVFX()
{
	//데디서버는 화면이 없음
	if (!PulseNiagara || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	//서버 판정과 같은 횟수 (예: 3초 / 0.5초 = 6번)
	VFXPulsesRemaining = FMath::Max(1, FMath::FloorToInt(Duration / TickInterval + KINDA_SMALL_NUMBER));

	if (bDamageOnSpawn)
	{
		PlayLocalPulseVFX();
	}

	if (VFXPulsesRemaining > 0)
	{
		GetWorldTimerManager().SetTimer(VFXTimerHandle, this, &AASSkillAreaActor::PlayLocalPulseVFX, TickInterval, true);
	}
}

void AASSkillAreaActor::PlayLocalPulseVFX()
{
	if (VFXPulsesRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(VFXTimerHandle);
		return;
	}

	--VFXPulsesRemaining;

	//클라이언트는 복제된 위치 (따라가는 장판도 같이 움직임)
	const float Scale = Radius / FMath::Max(1.0f, PulseNiagaraBaseRadius);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PulseNiagara, GetFXLocation(), FRotator::ZeroRotator, FVector(Scale));

	if (VFXPulsesRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(VFXTimerHandle);
	}
}

void AASSkillAreaActor::DoDamageTick()
{
	if (TicksRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(TickTimerHandle);
		return;
	}

	--TicksRemaining;

	//타이머가 Tick보다 먼저 올 수 있어서 판정 직전에 위치 갱신
	UpdateFollow();

	const FVector Center = GetActorLocation();
	TArray<FGameplayEffectSpecHandle> EffectSpecs;
	if (DamageSpec.IsValid())
	{
		EffectSpecs.Add(DamageSpec);
	}

	//추가 GE (슬로우 등). 데미지 계산에는 영향 없음 (Data.Damage가 없는 GE)
	for (const FGameplayEffectSpecHandle& ExtraSpec : ExtraEffectSpecs)
	{
		if (ExtraSpec.IsValid())
		{
			EffectSpecs.Add(ExtraSpec);
		}
	}

	FASCombatApplyResult ApplyResult;
	if (UASCombatTargetSubsystem* CombatTargets = GetWorld()->GetSubsystem<UASCombatTargetSubsystem>())
	{
		FASCombatTargetQuery Query;
		Query.SearchOrigin = Center;
		Query.SearchRadius = Radius;
		Query.Instigator = InstigatorAvatar.Get();
		Query.bCanHitPlayers = bCanHitPlayers;
		ImpactContext.HitLocation = Center;
		ApplyResult = CombatTargets->ResolveAndApply(Query, EffectSpecs, ImpactContext);
	}

	//DebugShowSkillDamage 1
	ASSkillDamageDebug::Report(this, EffectSpecs, ApplyResult, Center);
	
	if (ShouldDrawDebug())
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 16,
			ApplyResult.HasAnyHit() ? FColor::Red : FColor::Orange, false, TickInterval * 0.8f);
	}

	BP_OnAreaTick(ApplyResult.ActorTargets);
	OnAreaTick(ApplyResult, Center);

	if (TicksRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(TickTimerHandle);
	}
}

bool AASSkillAreaActor::ShouldDrawDebug() const
{
	return bDrawDebug && ASSkillDamageDebug::IsSkillDrawEnabled();
}

FVector AASSkillAreaActor::GetFXLocation() const
{
	FVector Location = GetActorLocation();

	//지형(WorldStatic)만 검사해서 몬스터·캐릭터 위에 뜨지 않게
	if (bSnapFXToGround && GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ASSkillAreaFXGround), false, this);
		Params.AddIgnoredActor(InstigatorAvatar.Get());
		const FVector Start = Location + FVector(0.0f, 0.0f, 100.0f);
		const FVector End = Location - FVector(0.0f, 0.0f, 500.0f);

		if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Location.Z = Hit.ImpactPoint.Z;
		}
	}

	Location.Z += FXHeightOffset;
	return Location;
}

void AASSkillAreaActor::ExecuteCueAt(const FGameplayTag& CueTag, const FVector& Location, float Magnitude) const
{
	if (!CueTag.IsValid())
	{
		return;
	}

	//장판은 ASC가 없으니 시전자 ASC로 실행 (서버에서 부르면 ASC가 모든 화면에 복제)
	UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorAvatar.Get());
	if (!InstigatorASC)
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = Location;
	CueParams.RawMagnitude = Magnitude;
	CueParams.Instigator = InstigatorAvatar.Get();
	CueParams.EffectCauser = const_cast<AASSkillAreaActor*>(this);
	CueParams.SourceObject = this;
	CueParams.NormalizedMagnitude = TickInterval;
	InstigatorASC->ExecuteGameplayCue(CueTag, CueParams);
}
