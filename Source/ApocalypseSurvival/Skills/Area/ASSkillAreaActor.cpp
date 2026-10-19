#include "Skills/Area/ASSkillAreaActor.h"

#include "DrawDebugHelpers.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Skills/ASSkillAbility.h"
#include "Skills/ASSkillTags.h"
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
	bool bInDrawDebug)
{
	DamageSpec = InDamageSpec;
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
	TArray<AActor*> Targets = UASSkillAbility::FindSkillTargets(GetWorld(), Center, Radius, InstigatorAvatar.Get(), bCanHitPlayers);

	for (AActor* Target : Targets)
	{
		UASSkillAbility::ApplyDamageSpecToTarget(DamageSpec, Target);
	}

	int32 MassHitCount = 0;
	if (!bCanHitPlayers && DamageSpec.IsValid())
	{
		const float MassDamage = FMath::Max(0.0f,
			-DamageSpec.Data->GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f));
		if (UASMassMonsterSubsystem* MassMonsters = GetWorld()->GetSubsystem<UASMassMonsterSubsystem>())
		{
			MassHitCount = MassMonsters->ApplyDamageInRadius(Center, Radius, MassDamage, InstigatorAvatar.Get());
		}
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 16,
			Targets.Num() > 0 || MassHitCount > 0 ? FColor::Red : FColor::Orange, false, TickInterval * 0.8f);
	}

	BP_OnAreaTick(Targets);

	if (TicksRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(TickTimerHandle);
	}
}
