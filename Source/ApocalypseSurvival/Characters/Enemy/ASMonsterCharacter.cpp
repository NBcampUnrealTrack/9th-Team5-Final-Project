#include "Characters/Enemy/ASMonsterCharacter.h"

#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterStatDefinition.h"
#include "Core/Combat/ASCombatImpactSettings.h"
#include "Core/Framework/ASGameState.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Characters/Enemy/UI/ASMonsterHealthBarWidget.h"
#include "Core/Framework/ASPlayerCameraManager.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Team/ASTeamTypes.h"
#include "EngineUtils.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Characters/Player/Components/ASProgressionComponent.h"
#include "Items/Loot/ASMonsterDropSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Skills/ASSkillTags.h"
#include "TimerManager.h"

AASMonsterCharacter::AASMonsterCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(30.0f);
    SetMinNetUpdateFrequency(10.0f);
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AASMonsterAIController::StaticClass();
    TeamId = ASTeam::Monster();
    OwnedAbilitySystemComponent = CreateDefaultSubobject<UASAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    OwnedAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    OwnedHealthAttributeSet = CreateDefaultSubobject<UASHealthAttributeSet>(TEXT("HealthAttributeSet"));
    OwnedCombatAttributeSet = CreateDefaultSubobject<UASCombatAttributeSet>(TEXT("CombatAttributeSet"));

    TargetingComponent = CreateDefaultSubobject<UASMonsterTargetingComponent>(TEXT("TargetingComponent"));
    CombatComponent = CreateDefaultSubobject<UASMonsterCombatComponent>(TEXT("CombatComponent"));

    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    MovementComponent->bOrientRotationToMovement = true;
    MovementComponent->bUseRVOAvoidance = true;
    MovementComponent->AvoidanceConsiderationRadius = 200.0f;
}

UAbilitySystemComponent* AASMonsterCharacter::GetAbilitySystemComponent() const
{
    return OwnedAbilitySystemComponent;
}

void AASMonsterCharacter::BeginPlay()
{
    Super::BeginPlay();

    InitAbilitySystem(OwnedAbilitySystemComponent, this);

    if (HasAuthority())
    {
       ApplyMonsterDefinition();
       ForceNetUpdate();

       bMonsterInitialized = true;
       OnMonsterInitialized.Broadcast();
    }

    bHealthBarEnabled = MonsterDefinition && MonsterDefinition->bUseWorldHealthBar;
    bHealthBarVisibleRequested = MonsterDefinition && MonsterDefinition->bHealthBarInitiallyVisible;
    ApplyHealthBarState();
}

void AASMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseHealthBar();
    UnbindTargetedAttackTagDelegate();
    SetTargetedAttackMaterialTint(false);
    UnbindHealthDelegate();
    UnbindDamageEffectDelegate();

    Super::EndPlay(EndPlayReason);
}

void AASMonsterCharacter::SetHealthBarEnabled(bool bEnabled)
{
    if (bHealthBarEnabled == bEnabled)
    {
       return;
    }

    bHealthBarEnabled = bEnabled;
    ApplyHealthBarState();
}

bool AASMonsterCharacter::IsHealthBarVisible() const
{
    return bHealthBarEnabled
       && bHealthBarVisibleRequested
       && IsValid(HealthBarComponent)
       && HealthBarComponent->IsVisible();
}

void AASMonsterCharacter::SetHealthBarVisible(bool bVisible)
{
    if (bHealthBarVisibleRequested == bVisible)
    {
       return;
    }

    bHealthBarVisibleRequested = bVisible;
    ApplyHealthBarState();
}

void AASMonsterCharacter::ApplyHealthBarState()
{
    if (GetNetMode() == NM_DedicatedServer)
    {
       return;
    }

    if (!bHealthBarEnabled)
    {
       ReleaseHealthBar();
       return;
    }

    if (!bHealthBarVisibleRequested)
    {
       UnbindHealthBarFromCameraZoom();
       if (IsValid(HealthBarWidget))
       {
          HealthBarWidget->SetHealthSource(nullptr);
       }

       if (IsValid(HealthBarComponent))
       {
          HealthBarComponent->SetComponentTickEnabled(false);
          HealthBarComponent->SetHiddenInGame(true);
          HealthBarComponent->SetVisibility(false);
       }
       return;
    }

    CreateHealthBarIfNeeded();
    if (!IsValid(HealthBarComponent))
    {
       return;
    }

    HealthBarComponent->SetHiddenInGame(false);
    HealthBarComponent->SetVisibility(true);
    HealthBarComponent->SetComponentTickEnabled(true);
    if (IsValid(HealthBarWidget))
    {
       HealthBarWidget->SetHealthSource(this);
    }
    BindHealthBarToCameraZoom();

    // 안개에 가려진 몬스터의 새 체력바도 숨김 규칙을 따르게 한다.
    if (UASVisibilityComponent* Visibility = FindComponentByClass<UASVisibilityComponent>())
    {
       Visibility->RefreshLocalPresentation();
    }
}

void AASMonsterCharacter::CreateHealthBarIfNeeded()
{
    if (IsValid(HealthBarComponent) || !MonsterDefinition)
    {
       return;
    }

    TSubclassOf<UASMonsterHealthBarWidget> WidgetClass = MonsterDefinition->HealthBarWidgetClass;
    if (!WidgetClass)
    {
       WidgetClass = UASMonsterHealthBarWidget::StaticClass();
    }

    HealthBarComponent = NewObject<UWidgetComponent>(this);
    if (!IsValid(HealthBarComponent))
    {
       return;
    }

    AddInstanceComponent(HealthBarComponent);
    HealthBarComponent->SetupAttachment(GetRootComponent());
    HealthBarComponent->SetRelativeLocation(MonsterDefinition->HealthBarOffset);
    HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
    HealthBarComponent->SetDrawSize(FIntPoint(
       FMath::RoundToInt(MonsterDefinition->HealthBarDrawSize.X),
       FMath::RoundToInt(MonsterDefinition->HealthBarDrawSize.Y)));
    HealthBarComponent->SetDrawAtDesiredSize(true);
    HealthBarComponent->SetPivot(FVector2D(0.5f, 1.0f));
    HealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBarComponent->SetGenerateOverlapEvents(false);
    HealthBarComponent->SetTickWhenOffscreen(false);
    HealthBarComponent->SetWidgetClass(WidgetClass);
    HealthBarComponent->RegisterComponent();
    HealthBarComponent->InitWidget();

    HealthBarWidget = Cast<UASMonsterHealthBarWidget>(HealthBarComponent->GetWidget());
}

void AASMonsterCharacter::ReleaseHealthBar()
{
    UnbindHealthBarFromCameraZoom();

    if (IsValid(HealthBarWidget))
    {
       HealthBarWidget->SetHealthSource(nullptr);
    }
    HealthBarWidget = nullptr;

    if (IsValid(HealthBarComponent))
    {
       HealthBarComponent->SetWidget(nullptr);
       HealthBarComponent->DestroyComponent();
    }
    HealthBarComponent = nullptr;
}

void AASMonsterCharacter::BindHealthBarToCameraZoom()
{
    UnbindHealthBarFromCameraZoom();
    if (!IsValid(HealthBarWidget) || !MonsterDefinition)
    {
       return;
    }

    AASPlayerCameraManager* CameraManager = Cast<AASPlayerCameraManager>(
       UGameplayStatics::GetPlayerCameraManager(this, 0));
    if (!IsValid(CameraManager))
    {
       HandleCameraZoomChanged(0.0f);
       return;
    }

    BoundCameraManager = CameraManager;
    CameraZoomChangedHandle = CameraManager->OnCameraZoomChanged.AddUObject(
       this, &AASMonsterCharacter::HandleCameraZoomChanged);
    HandleCameraZoomChanged(CameraManager->GetZoomAlpha());
}

void AASMonsterCharacter::UnbindHealthBarFromCameraZoom()
{
    if (BoundCameraManager.IsValid() && CameraZoomChangedHandle.IsValid())
    {
       BoundCameraManager->OnCameraZoomChanged.Remove(CameraZoomChangedHandle);
    }

    BoundCameraManager.Reset();
    CameraZoomChangedHandle.Reset();
}

void AASMonsterCharacter::HandleCameraZoomChanged(const float ZoomAlpha)
{
    if (!IsValid(HealthBarWidget) || !MonsterDefinition)
    {
       return;
    }

    const float HealthBarScale = MonsterDefinition->bScaleHealthBarWithCameraZoom
       ? FMath::Lerp(
          MonsterDefinition->HealthBarScaleAtMinZoomDistance,
          MonsterDefinition->HealthBarScaleAtMaxZoomDistance,
          FMath::Clamp(ZoomAlpha, 0.0f, 1.0f))
       : 1.0f;

    HealthBarWidget->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
    HealthBarWidget->SetRenderScale(FVector2D(HealthBarScale));
}

void AASMonsterCharacter::OnAbilitySystemInitialized()
{
    Super::OnAbilitySystemInitialized();

    UnbindHealthDelegate();
    HealthChangedHandle = OwnedAbilitySystemComponent
       ->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
       .AddUObject(this, &AASMonsterCharacter::HandleHealthChanged);

    UnbindDamageEffectDelegate();
    DamageEffectAppliedHandle = OwnedAbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf
       .AddUObject(this, &AASMonsterCharacter::HandleGameplayEffectApplied);

    UnbindTargetedAttackTagDelegate();
    TargetedAttackTagChangedHandle = OwnedAbilitySystemComponent
       ->RegisterGameplayTagEvent(ASGameplayTags::State_Monster_TargetedAttacking, EGameplayTagEventType::NewOrRemoved)
       .AddUObject(this, &AASMonsterCharacter::HandleTargetedAttackTagChanged);
    SetTargetedAttackMaterialTint(OwnedAbilitySystemComponent
       ->HasMatchingGameplayTag(ASGameplayTags::State_Monster_TargetedAttacking));
}

void AASMonsterCharacter::UnbindTargetedAttackTagDelegate()
{
    if (IsValid(OwnedAbilitySystemComponent) && TargetedAttackTagChangedHandle.IsValid())
    {
       OwnedAbilitySystemComponent
          ->RegisterGameplayTagEvent(ASGameplayTags::State_Monster_TargetedAttacking, EGameplayTagEventType::NewOrRemoved)
          .Remove(TargetedAttackTagChangedHandle);
    }

    TargetedAttackTagChangedHandle.Reset();
}

void AASMonsterCharacter::HandleTargetedAttackTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
    SetTargetedAttackMaterialTint(NewCount > 0);
}

void AASMonsterCharacter::SetTargetedAttackMaterialTint(const bool bActive)
{
    if (bTargetedAttackTintActive == bActive)
    {
       return;
    }

    USkeletalMeshComponent* MeshComponent = GetMesh();
    if (!IsValid(MeshComponent))
    {
       return;
    }

    if (!bActive)
    {
       for (int32 MaterialIndex = 0; MaterialIndex < TargetedAttackOriginalMaterials.Num(); ++MaterialIndex)
       {
          if (TargetedAttackMaterialInstances.IsValidIndex(MaterialIndex)
             && MeshComponent->GetMaterial(MaterialIndex) == TargetedAttackMaterialInstances[MaterialIndex])
          {
             MeshComponent->SetMaterial(MaterialIndex, TargetedAttackOriginalMaterials[MaterialIndex]);
          }
       }

       TargetedAttackOriginalMaterials.Reset();
       TargetedAttackMaterialInstances.Reset();
       bTargetedAttackTintActive = false;
       return;
    }

    const int32 MaterialCount = MeshComponent->GetNumMaterials();
    if (MaterialCount <= 0)
    {
       return;
    }

    TargetedAttackOriginalMaterials.SetNum(MaterialCount);
    TargetedAttackMaterialInstances.SetNum(MaterialCount);
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
    {
       UMaterialInterface* OriginalMaterial = MeshComponent->GetMaterial(MaterialIndex);
       TargetedAttackOriginalMaterials[MaterialIndex] = OriginalMaterial;
       if (!IsValid(OriginalMaterial))
       {
          continue;
       }

       UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(OriginalMaterial, this);
       if (!IsValid(DynamicMaterial))
       {
          continue;
       }

       DynamicMaterial->SetVectorParameterValue(TargetedAttackTintParameter, TargetedAttackTintColor);
       MeshComponent->SetMaterial(MaterialIndex, DynamicMaterial);
       TargetedAttackMaterialInstances[MaterialIndex] = DynamicMaterial;
    }

    bTargetedAttackTintActive = true;
}

bool AASMonsterCharacter::IsDead() const
{
    return IsValid(OwnedAbilitySystemComponent) && OwnedAbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead);
}

bool AASMonsterCharacter::IsHitReacting() const
{
    return IsValid(OwnedAbilitySystemComponent) && OwnedAbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_HitReact);
}

void AASMonsterCharacter::ConfigureSpawnHome(const FVector& InLocation, float InLeashRadius,
    float InReturnAcceptanceRadius, float InReturnHealthPercentPerSecond, float InDayDetectionRadius,
    float InDayLoseTargetRadius, float InReturnMoveSpeedMultiplier)
{
    bHasSpawnHome = true;
    SpawnHomeLocation = InLocation;
    SpawnLeashRadius = FMath::Max(0.0f, InLeashRadius);
    ReturnAcceptanceRadius = FMath::Max(1.0f, InReturnAcceptanceRadius);
    ReturnHealthPercentPerSecond = FMath::Max(0.0f, InReturnHealthPercentPerSecond);
    DayDetectionRadius = FMath::Max(0.0f, InDayDetectionRadius);
    DayLoseTargetRadius = FMath::Max(DayDetectionRadius, InDayLoseTargetRadius);
    ReturnMoveSpeedMultiplier = FMath::Max(1.0f, InReturnMoveSpeedMultiplier);
}

bool AASMonsterCharacter::IsOutsideSpawnLeash() const
{
    return bHasSpawnHome && SpawnLeashRadius > 0.0f
       && FVector::Dist2D(GetActorLocation(), SpawnHomeLocation) > SpawnLeashRadius;
}

void AASMonsterCharacter::SetFaceControllerFocus(bool bFaceControllerFocus)
{
    if (!HasAuthority())
    {
       return;
    }

    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    MovementComponent->bOrientRotationToMovement = !bFaceControllerFocus;
    MovementComponent->bUseControllerDesiredRotation = bFaceControllerFocus;
}

void AASMonsterCharacter::NotifyDamagedBy(AActor* InstigatorActor)
{
    if (!HasAuthority() || IsDead() || !IsValid(InstigatorActor))
    {
       return;
    }

    const float DirectHitAggro = MonsterDefinition ? MonsterDefinition->DirectHitAggro : 100.0f;
    NotifyAllyDamagedBy(InstigatorActor, DirectHitAggro);

    const float AssistRadius = MonsterDefinition ? MonsterDefinition->AssistAggroRadius : 0.0f;
    if (AssistRadius <= 0.0f || !GetWorld())
    {
       return;
    }

    const float AssistRadiusSquared = FMath::Square(AssistRadius);
    for (TActorIterator<AASMonsterCharacter> It(GetWorld()); It; ++It)
    {
       AASMonsterCharacter* NearbyMonster = *It;
       if (!IsValid(NearbyMonster) || NearbyMonster == this || NearbyMonster->IsDead()
          || FVector::DistSquared2D(GetActorLocation(), NearbyMonster->GetActorLocation()) > AssistRadiusSquared)
       {
          continue;
       }

       NearbyMonster->NotifyAllyDamagedBy(InstigatorActor, DirectHitAggro);
    }
}

void AASMonsterCharacter::NotifyAllyDamagedBy(AActor* InstigatorActor, float AggroAmount)
{
    if (!HasAuthority() || IsDead() || !IsValid(InstigatorActor))
    {
       return;
    }

    if (AASMonsterAIController* AIController = Cast<AASMonsterAIController>(GetController()))
    {
       AIController->NotifyMonsterDamaged(InstigatorActor, AggroAmount);
    }
}

void AASMonsterCharacter::ApplyMonsterDefinition()
{
    if (!MonsterDefinition)
    {
       return;
    }

    const FASMonsterInitialAttributes& InitialAttributes = MonsterDefinition->InitialAttributes;
    float MaxHealth = InitialAttributes.MaxHealth;
    float MoveSpeed = InitialAttributes.MoveSpeed;
    float AttackPower = InitialAttributes.AttackPower;
    if (const UASMonsterStatDefinition* StatDefinition = MonsterDefinition->StatDefinition)
    {
       FASMonsterStatResolveContext StatContext;
       if (const AASGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AASGameState>() : nullptr)
       {
          StatContext.Round = FMath::Max(1, GameState->GetCurrentDay());
          StatContext.Difficulty = GameState->GetMonsterStatDifficulty();
       }

       const FASResolvedMonsterStats ResolvedStats = FASMonsterStatResolver::Resolve(*StatDefinition, StatContext);
       MaxHealth = ResolvedStats.GetValue(ASMonsterStatTags::MaxHealth, MaxHealth);
       MoveSpeed = ResolvedStats.GetValue(ASMonsterStatTags::MoveSpeed, MoveSpeed);
       AttackPower = ResolvedStats.GetValue(ASMonsterStatTags::AttackPower, AttackPower);
    }

    OwnedHealthAttributeSet->InitMaxHealth(MaxHealth);
    OwnedHealthAttributeSet->InitHealth(MaxHealth);
    OwnedCombatAttributeSet->InitMoveSpeed(MoveSpeed);
    OwnedCombatAttributeSet->InitAttackPower(AttackPower);
    GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

    CombatComponent->Configure(MonsterDefinition->AttackRange, MonsterDefinition->AttackAbilityTag);
    TargetingComponent->Configure(MonsterDefinition->DetectionRadius, MonsterDefinition->LoseTargetRadius,
       MonsterDefinition->AggroDecayPerSecond);
    if (MonsterDefinition->GrantedAbilities.IsEmpty())
    {
       UE_LOG(LogTemp, Warning, TEXT("[Monster] %s has no GrantedAbilities. Configure a monster attack Ability in its Definition."),
          *GetNameSafe(MonsterDefinition));
    }

    for (const TSubclassOf<UGameplayAbility>& AbilityClass : MonsterDefinition->GrantedAbilities)
    {
       if (AbilityClass)
       {
          OwnedAbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
       }
    }

    TArray<FGameplayAbilitySpecHandle> PatternAbilityHandles;
    PatternAbilityHandles.Reserve(MonsterDefinition->AttackPatterns.Num());
    for (const FASMonsterAttackPattern& Pattern : MonsterDefinition->AttackPatterns)
    {
       if (!Pattern.PatternId.IsValid() || !Pattern.AbilityClass)
       {
          UE_LOG(LogTemp, Warning, TEXT("[Monster] %s has an invalid attack pattern. PatternId and AbilityClass are required."),
             *GetNameSafe(MonsterDefinition));
          PatternAbilityHandles.Add(FGameplayAbilitySpecHandle());
          continue;
       }

       PatternAbilityHandles.Add(OwnedAbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Pattern.AbilityClass, 1)));
    }

    CombatComponent->ConfigurePatterns(MonsterDefinition->AttackPatterns, PatternAbilityHandles);
}

void AASMonsterCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    if (HasAuthority() && Data.NewValue <= 0.0f)
    {
       QueueDeathHandling();
    }
}

void AASMonsterCharacter::HandleDeath()
{
    bDeathHandlingQueued = false;
    if (OwnedHealthAttributeSet && OwnedHealthAttributeSet->GetHealth() > 0.0f)
    {
       return;
    }

    if (IsDead())
    {
       return;
    }

    OwnedAbilitySystemComponent->AddMinimalReplicationGameplayTag(ASGameplayTags::State_Monster_Dead);
    SetHealthBarEnabled(false);
    SetTargetedAttackMaterialTint(false);
    AwardExperienceToKiller();
    if (UASMonsterDropSubsystem* DropSubsystem = GetWorld()->GetSubsystem<UASMonsterDropSubsystem>())
    {
       DropSubsystem->SpawnDrops(MonsterDefinition ? MonsterDefinition->DropGroup : FDataTableRowHandle(),
          GetActorLocation(), LastDamageKillerController.Get());
    }
    ReportDeathToGameMode();

    FGameplayTagContainer AttackAbilityTags;
    AttackAbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
    OwnedAbilitySystemComponent->CancelAbilities(&AttackAbilityTags, nullptr, nullptr);

    StopMovementAndAI();
    SetCanBeDamaged(false);
    SetActorEnableCollision(false);
    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
    for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
    {
       if (PrimitiveComponent)
       {
          PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
          PrimitiveComponent->SetGenerateOverlapEvents(false);
          PrimitiveComponent->SetCanEverAffectNavigation(false);
       }
    }

    const float DespawnDelay = MonsterDefinition ? MonsterDefinition->DeathDespawnDelay : 1.5f;
    if (DespawnDelay <= 0.0f)
    {
       Destroy();
    }
    else
    {
       SetLifeSpan(DespawnDelay);
    }
}

void AASMonsterCharacter::StopMovementAndAI()
{
    if (AASMonsterAIController* AIController = Cast<AASMonsterAIController>(GetController()))
    {
       AIController->StopAI(TEXT("Dead"));
    }

    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    MovementComponent->StopMovementImmediately();
    MovementComponent->SetAvoidanceEnabled(false);
    MovementComponent->DisableMovement();
}

void AASMonsterCharacter::ReportDeathToGameMode_Implementation()
{
    if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
    {
       GameMode->OnEnemyKilled(LastDamageKillerController.Get());
    }
}

void AASMonsterCharacter::ApplyImpactReaction(const FASCombatImpactContext& ImpactContext)
{
   if (!HasAuthority() || IsDead() || !MonsterDefinition)
   {
     return;
   }

   const UASCombatImpactSettings* ImpactSettings = GetDefault<UASCombatImpactSettings>();
   if (!ImpactSettings)
   {
     return;
   }

   const EASResolvedHitReaction Reaction = ImpactSettings->ResolveMonsterReaction(
     ImpactContext.Preset, MonsterDefinition->StaggerResistance);
   if (Reaction == EASResolvedHitReaction::None || Reaction == EASResolvedHitReaction::VisualOnly)
   {
     return;
   }

   const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
   if (ImpactContext.Preset == EASImpactPreset::Rapid && CurrentTime < NextRapidImpactReactionTime)
   {
     return;
   }

   if (ApplyHitReact(Reaction) && ImpactContext.Preset == EASImpactPreset::Rapid)
   {
     NextRapidImpactReactionTime = CurrentTime + FMath::Max(0.0f, ImpactSettings->RapidReapplyInterval);
   }
}

bool AASMonsterCharacter::ApplyHitReact(const EASResolvedHitReaction Reaction)
{
   if (!HasAuthority() || IsDead() || !MonsterDefinition || !MonsterDefinition->HitReactEffect)
   {
     return false;
   }

   const UASCombatImpactSettings* ImpactSettings = GetDefault<UASCombatImpactSettings>();
   const float Duration = ImpactSettings ? ImpactSettings->GetMonsterReactionDuration(Reaction) : 0.0f;
   if (Duration <= 0.0f)
   {
     return false;
   }

   FGameplayEffectContextHandle EffectContext = OwnedAbilitySystemComponent->MakeEffectContext();
   EffectContext.AddSourceObject(this);
   const FGameplayEffectSpecHandle HitReactSpec = OwnedAbilitySystemComponent->MakeOutgoingSpec(
     MonsterDefinition->HitReactEffect, 1.0f, EffectContext);
   if (!HitReactSpec.IsValid())
   {
     return false;
   }
   HitReactSpec.Data->SetDuration(Duration, true);

   OwnedAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*HitReactSpec.Data.Get());

   FGameplayTagContainer AttackAbilityTags;
   AttackAbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
   OwnedAbilitySystemComponent->CancelAbilities(&AttackAbilityTags, nullptr, nullptr);

   if (AASMonsterAIController* AIController = Cast<AASMonsterAIController>(GetController()))
   {
     AIController->NotifyMonsterHitReact();
   }

   return true;
}

void AASMonsterCharacter::UnbindHealthDelegate()
{
    if (IsValid(OwnedAbilitySystemComponent) && HealthChangedHandle.IsValid())
    {
       OwnedAbilitySystemComponent
          ->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
          .Remove(HealthChangedHandle);
    }

    HealthChangedHandle.Reset();
}

void AASMonsterCharacter::UnbindDamageEffectDelegate()
{
    if (IsValid(OwnedAbilitySystemComponent) && DamageEffectAppliedHandle.IsValid())
    {
       OwnedAbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf.Remove(DamageEffectAppliedHandle);
    }

    DamageEffectAppliedHandle.Reset();
}

void AASMonsterCharacter::HandleGameplayEffectApplied(UAbilitySystemComponent* TargetAbilitySystemComponent,
    const FGameplayEffectSpec& AppliedEffect, FActiveGameplayEffectHandle ActiveEffectHandle)
{
    if (!HasAuthority())
    {
       return;
    }

    const float DamageMagnitude = AppliedEffect.GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f);
    if (DamageMagnitude >= 0.0f)
    {
       return;
    }

    const FGameplayEffectContextHandle& EffectContext = AppliedEffect.GetContext();
    AActor* InstigatorActor = EffectContext.GetEffectCauser();
    if (!IsValid(InstigatorActor))
    {
       InstigatorActor = EffectContext.GetOriginalInstigator();
    }
    if (!IsValid(InstigatorActor))
    {
       InstigatorActor = EffectContext.GetInstigator();
    }

    RecordDamageInstigator(InstigatorActor);

    if (IsDead() || bDeathHandlingQueued)
    {
       return;
    }

    NotifyDamagedBy(InstigatorActor);
}

void AASMonsterCharacter::QueueDeathHandling()
{
    if (bDeathHandlingQueued || IsDead() || !GetWorld())
    {
       return;
    }

    bDeathHandlingQueued = true;
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
       HandleDeath();
    }));
}

void AASMonsterCharacter::RecordDamageInstigator(AActor* InstigatorActor)
{
    LastDamageKillerController.Reset();

    AController* pController = Cast<AController>(InstigatorActor);
    if (!pController)
    {
       if (const APawn* Pawn = Cast<APawn>(InstigatorActor))
       {
          pController = Pawn->GetController();
       }
       else if (const APawn* OwnerPawn = InstigatorActor ? Cast<APawn>(InstigatorActor->GetOwner()) : nullptr)
       {
          pController = OwnerPawn->GetController();
       }
    }

    if (IsValid(pController) && pController->IsPlayerController())
    {
       LastDamageKillerController = pController;
    }
}

void AASMonsterCharacter::AwardExperienceToKiller() const
{
    const int32 Experience = MonsterDefinition ? MonsterDefinition->ExperienceReward : 0;
    AController* KillerController = LastDamageKillerController.Get();
    if (Experience <= 0 || !IsValid(KillerController))
    {
       return;
    }

    AASPlayerState* pPlayerState = KillerController->GetPlayerState<AASPlayerState>();
    if (UASProgressionComponent* Progression = pPlayerState ? pPlayerState->GetProgressionComponent() : nullptr)
    {
       Progression->AddExperience(Experience);
    }
}