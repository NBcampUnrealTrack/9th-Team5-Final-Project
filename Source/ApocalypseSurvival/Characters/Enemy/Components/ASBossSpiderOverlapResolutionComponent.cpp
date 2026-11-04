#include "Characters/Enemy/Components/ASBossSpiderOverlapResolutionComponent.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

UASBossSpiderOverlapResolutionComponent::UASBossSpiderOverlapResolutionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    // 기존 BPC_SpiderOverlapGuard와 같은 시점에 보정해 이동/착지 Ability의 소유권을 보존한다.
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UASBossSpiderOverlapResolutionComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        SetComponentTickEnabled(false);
    }
}

void UASBossSpiderOverlapResolutionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwner());
    if (!Monster || !Monster->HasAuthority())
    {
        SetComponentTickEnabled(false);
        return;
    }

    if (!bHasActiveConfig && !TryInitializeConfig())
    {
        return;
    }

    ResolvePlayerOverlaps(*Monster);
}

bool UASBossSpiderOverlapResolutionComponent::TryInitializeConfig()
{
    const AASMonsterCharacter* Monster = Cast<AASMonsterCharacter>(GetOwner());
    const UASMonsterDefinition* Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
    const UASBossSpiderPatternSet* PatternSet = Definition ? Cast<UASBossSpiderPatternSet>(Definition->PatternSet) : nullptr;
    if (!PatternSet)
    {
        if (!bReportedInvalidOwner)
        {
            UE_LOG(LogTemp, Error, TEXT("[BossSpider] %s requires UASBossSpiderPatternSet for overlap resolution."),
                *GetNameSafe(GetOwner()));
            bReportedInvalidOwner = true;
        }
        SetComponentTickEnabled(false);
        return false;
    }

    ActiveConfig = PatternSet->OverlapResolution;
    bHasActiveConfig = true;
    return true;
}

bool UASBossSpiderOverlapResolutionComponent::ResolvePlayerOverlaps(AASMonsterCharacter& Monster)
{
    UWorld* World = Monster.GetWorld();
    UCapsuleComponent* MonsterCapsule = Monster.GetCapsuleComponent();
    if (!World || !MonsterCapsule)
    {
        return false;
    }

    const float MonsterRadius = MonsterCapsule->GetScaledCapsuleRadius();
    const float SeparationPadding = FMath::Max(0.0f, ActiveConfig.PlayerSeparationPadding);
    const int32 PassCount = FMath::Clamp(ActiveConfig.MaxResolutionPasses, 1, 4);
    bool bMoved = false;

    for (int32 PassIndex = 0; PassIndex < PassCount; ++PassIndex)
    {
        bool bMovedThisPass = false;
        for (TActorIterator<ACharacter> It(World); It; ++It)
        {
            ACharacter* PlayerCharacter = *It;
            if (!IsValid(PlayerCharacter) || PlayerCharacter == &Monster || !PlayerCharacter->IsPlayerControlled())
            {
                continue;
            }

            UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
            if (!PlayerCapsule)
            {
                continue;
            }

            const FVector MonsterLocation = Monster.GetActorLocation();
            const FVector PlayerLocation = PlayerCharacter->GetActorLocation();
            const float RequiredDistance = MonsterRadius + PlayerCapsule->GetScaledCapsuleRadius() + SeparationPadding;
            const float CurrentDistance = FVector::Dist2D(MonsterLocation, PlayerLocation);
            if (CurrentDistance >= RequiredDistance)
            {
                continue;
            }

            FVector SeparationDirection = (MonsterLocation - PlayerLocation).GetSafeNormal2D();
            if (SeparationDirection.IsNearlyZero())
            {
                SeparationDirection = -Monster.GetActorForwardVector().GetSafeNormal2D();
                if (SeparationDirection.IsNearlyZero())
                {
                    SeparationDirection = FVector::ForwardVector;
                }
            }

            const FVector ResolvedLocation = MonsterLocation
                + SeparationDirection * (RequiredDistance - CurrentDistance);
            Monster.SetActorLocation(ResolvedLocation, false, nullptr, ETeleportType::TeleportPhysics);
            bMoved = true;
            bMovedThisPass = true;
        }

        if (!bMovedThisPass)
        {
            break;
        }
    }

    if (bMoved)
    {
        Monster.ForceNetUpdate();
    }
    return bMoved;
}
