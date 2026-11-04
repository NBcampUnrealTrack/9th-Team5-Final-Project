#include "Characters/Enemy/BossSpider/ASBossSpiderWebTrap.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASBossSpiderDirectorComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AASBossSpiderWebTrap::AASBossSpiderWebTrap()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void AASBossSpiderWebTrap::InitializeTrap(AASMonsterCharacter* InBossMonster,
    const FASBossSpiderWebTrapConfig& InConfig)
{
    if (!HasAuthority() || bInitialized || !IsValid(InBossMonster))
    {
        return;
    }

    bInitialized = true;
    BossMonster = InBossMonster;
    ActiveConfig = InConfig;
    TrapRadius = FMath::Max(1.0f, ActiveConfig.TrapRadius);
    RequiredEscapeInputs = FMath::Max(1, ActiveConfig.RequiredEscapeInputs);
    SetTrapState(EASBossSpiderWebTrapState::Arming);
}

void AASBossSpiderWebTrap::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority() || !bInitialized)
    {
        return;
    }

    const double CurrentTime = GetWorldTimeSeconds();
    ArmEndServerTime = static_cast<float>(CurrentTime + FMath::Max(0.0f, ActiveConfig.ArmTime));
    // 서버는 RepNotify를 받지 않으므로, BP Cosmetic 자식도 Arming 상태를 즉시 표현하게 한다.
    OnRep_TrapState();
    OnRep_EscapeProgress();
    GetWorldTimerManager().SetTimer(ExpireTimerHandle, this, &ThisClass::ExpireTrap,
        FMath::Max(0.1f, ActiveConfig.Lifetime), false);

    if (ActiveConfig.ArmTime <= 0.0f)
    {
        BeginArmedState();
        return;
    }

    GetWorldTimerManager().SetTimer(ArmTimerHandle, this, &ThisClass::BeginArmedState,
        ActiveConfig.ArmTime, false);
}

void AASBossSpiderWebTrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        ReleaseCapture(true);
    }
    Super::EndPlay(EndPlayReason);
}

void AASBossSpiderWebTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AASBossSpiderWebTrap, TrapState);
    DOREPLIFETIME(AASBossSpiderWebTrap, TrapRadius);
    DOREPLIFETIME(AASBossSpiderWebTrap, ArmEndServerTime);
    DOREPLIFETIME(AASBossSpiderWebTrap, EscapeInputCount);
    DOREPLIFETIME(AASBossSpiderWebTrap, RequiredEscapeInputs);
    DOREPLIFETIME(AASBossSpiderWebTrap, EscapeEndServerTime);
}

bool AASBossSpiderWebTrap::ServerRegisterBreakFreeInput(AASPlayerCharacter* Player)
{
    if (!HasAuthority() || TrapState != EASBossSpiderWebTrapState::Triggered
        || CapturedPlayer.Get() != Player || !IsValid(Player) || !Player->IsWebBound())
    {
        return false;
    }

    if (GetWorldTimeSeconds() > EscapeEndServerTime + FMath::Max(0.0f, ActiveConfig.EscapeInputGrace))
    {
        return false;
    }

    EscapeInputCount = FMath::Min(RequiredEscapeInputs, EscapeInputCount + 1);
    OnRep_EscapeProgress();
    ForceNetUpdate();
    if (EscapeInputCount < RequiredEscapeInputs)
    {
        return true;
    }

    ReleaseCapture(true);
    SetTrapState(EASBossSpiderWebTrapState::Released);
    SetLifeSpan(0.2f);
    return true;
}

void AASBossSpiderWebTrap::ReleaseCapturedPlayer(AASPlayerCharacter* Player)
{
    if (!HasAuthority() || CapturedPlayer.Get() != Player || !IsValid(Player))
    {
        return;
    }

    ReleaseCapture(true);
    SetTrapState(EASBossSpiderWebTrapState::Released);
    SetLifeSpan(0.2f);
}

void AASBossSpiderWebTrap::OnRep_TrapState()
{
    OnTrapStateChanged(TrapState);
}

void AASBossSpiderWebTrap::OnRep_EscapeProgress()
{
    OnEscapeProgressChanged(EscapeInputCount, RequiredEscapeInputs, EscapeEndServerTime);
}

void AASBossSpiderWebTrap::BeginArmedState()
{
    if (!HasAuthority() || TrapState != EASBossSpiderWebTrapState::Arming)
    {
        return;
    }

    if (!BossMonster.IsValid() || BossMonster->IsDead())
    {
        ExpireTrap();
        return;
    }

    SetTrapState(EASBossSpiderWebTrapState::Armed);
    GetWorldTimerManager().SetTimer(MonitorTimerHandle, this, &ThisClass::MonitorTrap, 0.05f, true);
}

void AASBossSpiderWebTrap::MonitorTrap()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!BossMonster.IsValid() || BossMonster->IsDead())
    {
        ExpireTrap();
        return;
    }

    if (TrapState == EASBossSpiderWebTrapState::Triggered)
    {
        AASPlayerCharacter* Player = CapturedPlayer.Get();
        if (!IsValid(Player) || Player->IsDead() || Player->IsDowned() || !Player->IsWebBound())
        {
            ReleaseCapture(true);
            SetTrapState(EASBossSpiderWebTrapState::Released);
            SetLifeSpan(0.2f);
        }
        return;
    }

    if (TrapState != EASBossSpiderWebTrapState::Armed)
    {
        return;
    }

    AASPlayerCharacter* BestCandidate = nullptr;
    float BestDistanceSquared = FMath::Square(TrapRadius);
    for (TActorIterator<AASPlayerCharacter> It(GetWorld()); It; ++It)
    {
        AASPlayerCharacter* Candidate = *It;
        if (!IsPlayerEligible(*Candidate))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestCandidate = Candidate;
        }
    }

    if (BestCandidate)
    {
        TryCapturePlayer(*BestCandidate);
    }
}

void AASBossSpiderWebTrap::ExpireTrap()
{
    if (!HasAuthority() || TrapState == EASBossSpiderWebTrapState::Expired)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(ArmTimerHandle);
    GetWorldTimerManager().ClearTimer(MonitorTimerHandle);
    GetWorldTimerManager().ClearTimer(ExpireTimerHandle);
    ReleaseCapture(true);
    SetTrapState(EASBossSpiderWebTrapState::Expired);
    SetLifeSpan(0.2f);
}

bool AASBossSpiderWebTrap::TryCapturePlayer(AASPlayerCharacter& Player)
{
    AASMonsterCharacter* Boss = BossMonster.Get();
    UASBossSpiderDirectorComponent* Director = Boss ? Boss->FindComponentByClass<UASBossSpiderDirectorComponent>() : nullptr;
    UAbilitySystemComponent* SourceASC = Boss ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Boss) : nullptr;
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(&Player);
    if (!Director || !SourceASC || !TargetASC || !ActiveConfig.WebBoundEffectClass
        || !Director->TrySetCaptureTarget(&Player))
    {
        return false;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(this);
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(ActiveConfig.WebBoundEffectClass, 1.0f, Context);
    if (!Spec.IsValid())
    {
        Director->ClearCaptureTarget(&Player);
        return false;
    }

    Spec.Data->SetDuration(FMath::Max(0.1f, ActiveConfig.BoundDuration), true);
    BoundEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (!BoundEffectHandle.IsValid() || !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_WebBound))
    {
        Director->ClearCaptureTarget(&Player);
        BoundEffectHandle.Invalidate();
        return false;
    }

    CapturedPlayer = &Player;
    Player.SetActiveWebTrap(this);
    EscapeInputCount = 0;
    EscapeEndServerTime = static_cast<float>(GetWorldTimeSeconds() + FMath::Max(0.05f, ActiveConfig.EscapeInputWindow));
    SetTrapState(EASBossSpiderWebTrapState::Triggered);
    OnRep_EscapeProgress();
    ForceNetUpdate();
    return true;
}

void AASBossSpiderWebTrap::ReleaseCapture(const bool bRemoveBoundEffect)
{
    AASPlayerCharacter* Player = CapturedPlayer.Get();
    if (IsValid(Player))
    {
        if (bRemoveBoundEffect)
        {
            if (UAbilitySystemComponent* PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player))
            {
                PlayerASC->RemoveActiveGameplayEffect(BoundEffectHandle);
            }
        }
        Player->ClearActiveWebTrap(this);
    }

    if (IsValid(Player))
    {
        if (AASMonsterCharacter* Boss = BossMonster.Get())
        {
            if (UASBossSpiderDirectorComponent* Director = Boss->FindComponentByClass<UASBossSpiderDirectorComponent>())
            {
                Director->ClearCaptureTarget(Player);
            }
        }
    }

    CapturedPlayer.Reset();
    BoundEffectHandle.Invalidate();
    EscapeInputCount = 0;
    EscapeEndServerTime = 0.0f;
    OnRep_EscapeProgress();
}

void AASBossSpiderWebTrap::SetTrapState(const EASBossSpiderWebTrapState NewState)
{
    TrapState = NewState;
    if (HasActorBegunPlay())
    {
        OnRep_TrapState();
    }
    ForceNetUpdate();
}

bool AASBossSpiderWebTrap::IsPlayerEligible(const AASPlayerCharacter& Player) const
{
    return !Player.IsDead() && !Player.IsDowned() && !Player.IsWebBound();
}

double AASBossSpiderWebTrap::GetWorldTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.0;
}
