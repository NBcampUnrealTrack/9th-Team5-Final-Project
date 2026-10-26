#include "Skills/Area/ASSkillAreaActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AASSkillAreaActor::AASSkillAreaActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

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

void AASSkillAreaActor::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
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

	const FVector Center = GetActorLocation();
	TArray<FGameplayEffectSpecHandle> EffectSpecs;
	if (DamageSpec.IsValid())
	{
		EffectSpecs.Add(DamageSpec);
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
