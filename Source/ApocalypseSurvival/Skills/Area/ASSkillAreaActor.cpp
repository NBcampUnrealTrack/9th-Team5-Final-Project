#include "Skills/Area/ASSkillAreaActor.h"

#include "Skills/Debug/ASSkillDamageDebug.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "DrawDebugHelpers.h"
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

	//AS.Debug.SkillDamage 1
	ASSkillDamageDebug::Report(this, EffectSpecs, ApplyResult, Center);
	
	//GameplayCue로 충격파 이펙트 (모든 화면)
	ExecutePulseCue(Center);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 16,
			ApplyResult.HasAnyHit() ? FColor::Red : FColor::Orange, false, TickInterval * 0.8f);
	}

	BP_OnAreaTick(ApplyResult.ActorTargets);

	if (TicksRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(TickTimerHandle);
	}
}

void AASSkillAreaActor::ExecutePulseCue(const FVector& Center) const
{
	if (!PulseCueTag.IsValid())
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
	CueParams.Location = Center;
	CueParams.RawMagnitude = Radius;
	CueParams.Instigator = InstigatorAvatar.Get();
	CueParams.EffectCauser = const_cast<AASSkillAreaActor*>(this);
	CueParams.SourceObject = this;
	CueParams.NormalizedMagnitude = TickInterval;
	InstigatorASC->ExecuteGameplayCue(PulseCueTag, CueParams);
}
