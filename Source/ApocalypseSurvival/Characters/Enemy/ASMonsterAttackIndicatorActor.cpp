#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"

#include "Components/SceneComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"

AASMonsterAttackIndicatorActor::AASMonsterAttackIndicatorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void AASMonsterAttackIndicatorActor::BeginPlay()
{
    Super::BeginPlay();

    // 서버는 FinishSpawning 이후, 클라이언트는 복제값이 이미 도착한 경우에만 초기화한다.
    NotifyIndicatorInitialized();
}

void AASMonsterAttackIndicatorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bIndicatorInitialized)
    {
       BP_OnIndicatorProgress(GetProgress());
    }
}

void AASMonsterAttackIndicatorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AASMonsterAttackIndicatorActor, Shape);
    DOREPLIFETIME(AASMonsterAttackIndicatorActor, Radius);
    DOREPLIFETIME(AASMonsterAttackIndicatorActor, Length);
    DOREPLIFETIME(AASMonsterAttackIndicatorActor, Width);
    DOREPLIFETIME(AASMonsterAttackIndicatorActor, Duration);
    DOREPLIFETIME(AASMonsterAttackIndicatorActor, AngleDegrees);
    DOREPLIFETIME(AASMonsterAttackIndicatorActor, StartServerTime);
}

void AASMonsterAttackIndicatorActor::InitializeIndicator(EASMonsterAttackIndicatorShape InShape, float InRadius,
    float InLength, float InWidth, float InDuration, float InAngleDegrees)
{
    if (!HasAuthority())
    {
       return;
    }

    Shape = InShape;
    Radius = FMath::Max(0.0f, InRadius);
    Length = FMath::Max(0.0f, InLength);
    Width = FMath::Max(0.0f, InWidth);
    Duration = FMath::Max(0.01f, InDuration);
    AngleDegrees = FMath::Clamp(InAngleDegrees, 0.0f, 360.0f);
    StartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    SetLifeSpan(Duration + 0.2f);
    ForceNetUpdate();
}

// --- 패턴 기반 장판 초기화 및 데미지 타격 구현 ---
void AASMonsterAttackIndicatorActor::InitializeRectangleIndicator(float InLength, float InWidth, float InDuration)
{
    if (!HasAuthority())
    {
       return;
    }

    InitializeIndicator(EASMonsterAttackIndicatorShape::Rectangle, 0.0f, InLength, InWidth, InDuration);
    // 서버는 OnRep이 호출되지 않으므로 BeginPlay 이후 초기화한 경우 직접 시각 초기화를 알린다.
    NotifyIndicatorInitialized();
}

void AASMonsterAttackIndicatorActor::InitializeTelegraph(const FASMonsterAttackPattern& Pattern, AActor* MonsterOwner)
{
    if (!HasAuthority())
    {
       return;
    }

    EASMonsterAttackIndicatorShape IndicatorShape = EASMonsterAttackIndicatorShape::Rectangle;
    if (Pattern.ShapeType == ETelegraphShape::Radial)
    {
       IndicatorShape = EASMonsterAttackIndicatorShape::Circle;
    }
    else if (Pattern.ShapeType == ETelegraphShape::Sector)
    {
       IndicatorShape = EASMonsterAttackIndicatorShape::Sector;
    }

    float InLength = Pattern.BoxExtent.X * 2.0f;
    float InWidth = Pattern.BoxExtent.Y * 2.0f;
    const float InRadius = Pattern.ShapeType == ETelegraphShape::Sector ? Pattern.SectorRadius : Pattern.BoxExtent.X;

    InitializeIndicator(IndicatorShape, InRadius, InLength, InWidth, Pattern.TelegraphDuration,
       Pattern.SectorAngleDegrees);
}

void AASMonsterAttackIndicatorActor::ApplyDamageAndExecute(const FASMonsterAttackPattern& Pattern, AActor* MonsterOwner)
{
    if (!HasAuthority() || !MonsterOwner)
    {
       return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
       return;
    }

    FVector CenterLocation = GetActorLocation();
    FQuat Rotation = GetActorQuat();

    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(MonsterOwner);

    bool bHit = false;
    if (Pattern.ShapeType == ETelegraphShape::Rectangle)
    {
       FCollisionShape CollisionBox = FCollisionShape::MakeBox(Pattern.BoxExtent);
       bHit = World->OverlapMultiByChannel(OverlapResults, CenterLocation, Rotation, ECC_Pawn, CollisionBox, QueryParams);
    }
    else if (Pattern.ShapeType == ETelegraphShape::Radial)
    {
       FCollisionShape CollisionSphere = FCollisionShape::MakeSphere(Pattern.BoxExtent.X);
       bHit = World->OverlapMultiByChannel(OverlapResults, CenterLocation, Rotation, ECC_Pawn, CollisionSphere, QueryParams);
    }
    else if (Pattern.ShapeType == ETelegraphShape::Sector)
    {
       FCollisionShape CollisionSphere = FCollisionShape::MakeSphere(FMath::Max(0.0f, Pattern.SectorRadius));
       bHit = World->OverlapMultiByChannel(OverlapResults, CenterLocation, Rotation, ECC_Pawn, CollisionSphere, QueryParams);
    }

    if (bHit)
    {
       for (const FOverlapResult& Result : OverlapResults)
       {
          AActor* HitActor = Result.GetActor();
          if (!IsValid(HitActor) || HitActor == MonsterOwner)
          {
             continue;
          }

          if (Pattern.ShapeType == ETelegraphShape::Sector)
          {
             const FVector DirectionToTarget = (HitActor->GetActorLocation() - CenterLocation).GetSafeNormal2D();
             const FVector ForwardDirection = MonsterOwner->GetActorForwardVector().GetSafeNormal2D();
             const float HalfAngleRadians = FMath::DegreesToRadians(FMath::Clamp(Pattern.SectorAngleDegrees, 1.0f, 360.0f) * 0.5f);
             if (FVector::DotProduct(ForwardDirection, DirectionToTarget) < FMath::Cos(HalfAngleRadians))
             {
                continue;
             }
          }

          if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(HitActor))
          {
             if (ASCInterface->GetAbilitySystemComponent())
             {
                UGameplayStatics::ApplyDamage(HitActor, Pattern.BaseDamage, MonsterOwner->GetInstigatorController(), MonsterOwner, UDamageType::StaticClass());
             }
          }
       }
    }
}

float AASMonsterAttackIndicatorActor::GetProgress() const
{
    if (Duration <= 0.0f)
    {
       return 1.0f;
    }

    return FMath::Clamp((GetSynchronizedWorldTime() - StartServerTime) / Duration, 0.0f, 1.0f);
}

void AASMonsterAttackIndicatorActor::OnRep_IndicatorConfig()
{
    NotifyIndicatorInitialized();
}

void AASMonsterAttackIndicatorActor::NotifyIndicatorInitialized()
{
    if (Duration <= 0.0f)
    {
       return;
    }

    bIndicatorInitialized = true;
    BP_OnIndicatorInitialized();
    BP_OnIndicatorProgress(GetProgress());
}

float AASMonsterAttackIndicatorActor::GetSynchronizedWorldTime() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
       return 0.0f;
    }

    if (const AGameStateBase* GameState = World->GetGameState())
    {
       return GameState->GetServerWorldTimeSeconds();
    }

    return World->GetTimeSeconds();
}
