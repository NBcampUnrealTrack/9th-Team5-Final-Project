#include "Characters/Enemy/BossSpider/ASBossSpiderWebTrap.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/AI/StateTree/ASBossSpiderStateTreeTypes.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "StateTreeEvents.h"
#include "StructUtils/StructView.h"
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
    UAbilitySystemComponent* SourceASC = Boss ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Boss) : nullptr;
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(&Player);
    if (!SourceASC || !TargetASC || !ActiveConfig.WebBoundEffectClass
		|| SourceASC->HasMatchingGameplayTag(ASGameplayTags::State_BossSpider_Capture_Active))
    {
        return false;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(this);
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(ActiveConfig.WebBoundEffectClass, 1.0f, Context);
    if (!Spec.IsValid())
    {
        return false;
    }

    Spec.Data->SetDuration(FMath::Max(0.1f, ActiveConfig.BoundDuration), true);
    BoundEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (!BoundEffectHandle.IsValid() || !TargetASC->HasMatchingGameplayTag(ASGameplayTags::State_WebBound))
    {
        BoundEffectHandle.Invalidate();
        return false;
    }

    CapturedPlayer = &Player;
    Player.SetActiveWebTrap(this);
    EscapeInputCount = 0;
    EscapeEndServerTime = static_cast<float>(GetWorldTimeSeconds() + FMath::Max(0.05f, ActiveConfig.EscapeInputWindow));
	RegisterBossCaptureState();
    SetTrapState(EASBossSpiderWebTrapState::Triggered);
    OnRep_EscapeProgress();
    ForceNetUpdate();
	SendCaptureStateTreeEvent(ASGameplayTags::StateTreeEvent_BossSpider_CaptureReady, &Player);
    return true;
}

void AASBossSpiderWebTrap::ReleaseCapture(const bool bRemoveBoundEffect)
{
    AASPlayerCharacter* Player = CapturedPlayer.Get();
	if (bBossCaptureStateRegistered)
	{
		SendCaptureStateTreeEvent(ASGameplayTags::StateTreeEvent_BossSpider_CaptureCancelled, Player);
	}
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

	ReleaseBossCaptureState();
    CapturedPlayer.Reset();
    BoundEffectHandle.Invalidate();
    EscapeInputCount = 0;
    EscapeEndServerTime = 0.0f;
    OnRep_EscapeProgress();
}

bool AASBossSpiderWebTrap::IsCaptureActiveFor(const AActor* Target) const
{
	return HasAuthority()
		&& TrapState == EASBossSpiderWebTrapState::Triggered
		&& IsValid(Target)
		&& CapturedPlayer.Get() == Target
		&& bBossCaptureStateRegistered;
}

void AASBossSpiderWebTrap::SendCaptureStateTreeEvent(const FGameplayTag& EventTag, AActor* Target) const
{
	const AASMonsterCharacter* Boss = BossMonster.Get();
	const AASMonsterAIController* Controller = Boss ? Cast<AASMonsterAIController>(Boss->GetController()) : nullptr;
	UStateTreeAIComponent* StateTree = Controller ? Controller->GetStateTreeAIComponent() : nullptr;
	if (!HasAuthority() || !EventTag.IsValid() || !IsValid(StateTree) || !StateTree->IsRunning())
	{
		return;
	}

	FASBossSpiderCaptureEventPayload Payload;
	Payload.Target = Target;
	Payload.SourceTrap = const_cast<AASBossSpiderWebTrap*>(this);
	StateTree->SendStateTreeEvent(EventTag, FConstStructView::Make(Payload), GetFName());
}

void AASBossSpiderWebTrap::RegisterBossCaptureState()
{
	if (bBossCaptureStateRegistered)
	{
		return;
	}

	AASMonsterCharacter* Boss = BossMonster.Get();
	UAbilitySystemComponent* BossASC = Boss ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Boss) : nullptr;
	if (IsValid(BossASC))
	{
		BossASC->AddLooseGameplayTag(ASGameplayTags::State_BossSpider_Capture_Active, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		bBossCaptureStateRegistered = true;
	}
}

void AASBossSpiderWebTrap::ReleaseBossCaptureState()
{
	if (!bBossCaptureStateRegistered)
	{
		return;
	}
	bBossCaptureStateRegistered = false;

	AASMonsterCharacter* Boss = BossMonster.Get();
	UAbilitySystemComponent* BossASC = Boss ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Boss) : nullptr;
	if (IsValid(BossASC))
	{
		BossASC->RemoveLooseGameplayTag(ASGameplayTags::State_BossSpider_Capture_Active, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
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
