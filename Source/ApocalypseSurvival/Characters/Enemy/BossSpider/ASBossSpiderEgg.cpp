#include "Characters/Enemy/BossSpider/ASBossSpiderEgg.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AASBossSpiderEgg::AASBossSpiderEgg()
{
    bReplicates = true;
    SetReplicateMovement(true);
}

void AASBossSpiderEgg::InitializeEgg(AASMonsterCharacter* InBossMonster,
    const FASBossSpiderEggSummonConfig& InConfig)
{
    if (!HasAuthority() || bInitialized || !IsValid(InBossMonster))
    {
        return;
    }

    bInitialized = true;
    BossMonster = InBossMonster;
    ActiveConfig = InConfig;
}

void AASBossSpiderEgg::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority() || !bInitialized)
    {
        return;
    }

    AASMonsterCharacter* Boss = BossMonster.Get();
    if (!IsValid(Boss) || Boss->IsDead())
    {
        ExpireEgg();
        return;
    }

	if (UAbilitySystemComponent* BossASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Boss))
    {
		BossASC->AddLooseGameplayTag(ASGameplayTags::State_BossSpider_Egg_Active, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		bEggActiveTagRegistered = true;
    }

    const float DescentDuration = FMath::Max(0.0f, ActiveConfig.EggDescentDuration);
    SetDescentInvulnerable(true);
    SetCanBeDamaged(false);
    LandingEndServerTime = GetWorld()->GetTimeSeconds() + DescentDuration;
    HatchEndServerTime = 0.0f;
    OnRep_EggState();
    if (DescentDuration <= 0.0f)
    {
        LandEgg();
    }
    else
    {
        GetWorldTimerManager().SetTimer(LandingTimerHandle, this, &ThisClass::LandEgg,
            DescentDuration, false);
    }
    GetWorldTimerManager().SetTimer(BossMonitorTimerHandle, this, &ThisClass::MonitorBossLifetime, 0.1f, true);
    ForceNetUpdate();
}

void AASBossSpiderEgg::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(LandingTimerHandle);
        GetWorldTimerManager().ClearTimer(HatchTimerHandle);
        GetWorldTimerManager().ClearTimer(BossMonitorTimerHandle);
        SetDescentInvulnerable(false);
        ReleaseEggActiveTag();
    }
    Super::EndPlay(EndPlayReason);
}

void AASBossSpiderEgg::HandleDeath()
{
    if (HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(LandingTimerHandle);
        GetWorldTimerManager().ClearTimer(HatchTimerHandle);
        GetWorldTimerManager().ClearTimer(BossMonitorTimerHandle);
        SetDescentInvulnerable(false);
        ReleaseEggActiveTag();
        SetEggState(EASBossSpiderEggState::Expired);
    }
    Super::HandleDeath();
}

void AASBossSpiderEgg::OnRep_EggState()
{
    OnEggStateChanged(EggState);
}

void AASBossSpiderEgg::OnRep_LandingEndServerTime()
{
    if (LandingEndServerTime > 0.0f && EggState == EASBossSpiderEggState::Descending)
    {
        OnEggStateChanged(EggState);
    }
}

void AASBossSpiderEgg::LandEgg()
{
    if (!HasAuthority() || EggState != EASBossSpiderEggState::Descending)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(LandingTimerHandle);
    SetDescentInvulnerable(false);
    SetCanBeDamaged(true);
    LandingEndServerTime = 0.0f;
    HatchEndServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.05f, ActiveConfig.HatchDelay);
    SetEggState(EASBossSpiderEggState::Incubating);
    GetWorldTimerManager().SetTimer(HatchTimerHandle, this, &ThisClass::HatchEgg,
        FMath::Max(0.05f, ActiveConfig.HatchDelay), false);
}

void AASBossSpiderEgg::HatchEgg()
{
    if (!HasAuthority() || EggState != EASBossSpiderEggState::Incubating)
    {
        return;
    }

    if (!BossMonster.IsValid() || BossMonster->IsDead() || !SpawnEliteMinion())
    {
        ExpireEgg();
        return;
    }

    GetWorldTimerManager().ClearTimer(BossMonitorTimerHandle);
    ReleaseEggActiveTag();
    SetEggState(EASBossSpiderEggState::Hatched);
    if (ActiveConfig.HatchDespawnDelay <= 0.0f)
    {
        Destroy();
    }
    else
    {
        SetLifeSpan(ActiveConfig.HatchDespawnDelay);
    }
}

void AASBossSpiderEgg::MonitorBossLifetime()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!BossMonster.IsValid() || BossMonster->IsDead())
    {
        ExpireEgg();
    }
}

void AASBossSpiderEgg::ExpireEgg()
{
    if (!HasAuthority() || EggState == EASBossSpiderEggState::Expired)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(LandingTimerHandle);
    GetWorldTimerManager().ClearTimer(HatchTimerHandle);
    GetWorldTimerManager().ClearTimer(BossMonitorTimerHandle);
    SetDescentInvulnerable(false);
    ReleaseEggActiveTag();
    SetEggState(EASBossSpiderEggState::Expired);
    SetLifeSpan(0.2f);
}

bool AASBossSpiderEgg::SpawnEliteMinion()
{
    UWorld* World = GetWorld();
    AASMonsterCharacter* Boss = BossMonster.Get();
    if (!World || !IsValid(Boss) || !ActiveConfig.EliteMinionClass)
    {
        return false;
    }

    const FTransform SpawnTransform(GetActorRotation(), GetActorLocation());
    AASMonsterCharacter* EliteMinion = World->SpawnActorDeferred<AASMonsterCharacter>(ActiveConfig.EliteMinionClass,
        SpawnTransform, Boss, Boss, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!EliteMinion)
    {
        return false;
    }

    EliteMinion->FinishSpawning(SpawnTransform);
    return true;
}

void AASBossSpiderEgg::ReleaseEggActiveTag()
{
	if (!bEggActiveTagRegistered)
    {
        return;
    }
	bEggActiveTagRegistered = false;

    if (AASMonsterCharacter* Boss = BossMonster.Get())
    {
		if (UAbilitySystemComponent* BossASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Boss))
        {
			BossASC->RemoveLooseGameplayTag(ASGameplayTags::State_BossSpider_Egg_Active, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
        }
    }
}

void AASBossSpiderEgg::SetDescentInvulnerable(const bool bInvulnerable)
{
    if (bDescentInvulnerable == bInvulnerable)
    {
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    bDescentInvulnerable = bInvulnerable;
    if (bInvulnerable)
    {
        ASC->AddLooseGameplayTag(ASGameplayTags::State_Invulnerable, 1,
            EGameplayTagReplicationState::TagAndCountToAll);
    }
    else
    {
        ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Invulnerable, 1,
            EGameplayTagReplicationState::TagAndCountToAll);
    }
}

void AASBossSpiderEgg::SetEggState(const EASBossSpiderEggState NewState)
{
    EggState = NewState;
    if (HasActorBegunPlay())
    {
        OnRep_EggState();
    }
    ForceNetUpdate();
}

void AASBossSpiderEgg::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AASBossSpiderEgg, EggState);
    DOREPLIFETIME(AASBossSpiderEgg, LandingEndServerTime);
    DOREPLIFETIME(AASBossSpiderEgg, HatchEndServerTime);
}
