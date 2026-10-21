#include "Characters/Enemy/ASMonsterCharacter.h"

#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
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
	// 일반 몬스터는 소유 클라이언트가 없으므로 전체 Effect 목록 대신 태그/큐/Attribute만 동기화한다.
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
	// Health/Combat AttributeSet의 OnRep은 BeginPlay 전에 실행될 수 있다.
	// 이때도 Actor-owned ASC를 반환해야 GAMEPLAYATTRIBUTE_REPNOTIFY가 복제 값을 안전하게 반영한다.
	return OwnedAbilitySystemComponent;
}

void AASMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 적은 ASC와 Avatar가 모두 자기 자신이다.
	InitAbilitySystem(OwnedAbilitySystemComponent, this);

	if (HasAuthority())
	{
		ApplyMonsterDefinition();

		// 초기 능력치와 Ability 가 바뀐 직후라 다음 복제 주기를 기다리지 않고 바로 내보낸다.
		ForceNetUpdate();

		// AI 는 이 신호를 받은 뒤에 시작한다. Definition 과 GAS 가 모두 준비된 시점이다.
		bMonsterInitialized = true;
		OnMonsterInitialized.Broadcast();
	}

	// 전용 서버에는 UI 객체를 만들지 않는다. 클라이언트와 Listen Server에서만 필요한 경우 지연 생성한다.
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
		// 다시 표시할 때 현재 Attribute 값으로 즉시 갱신한 뒤 변경 이벤트를 다시 구독한다.
		HealthBarWidget->SetHealthSource(this);
	}
	BindHealthBarToCameraZoom();
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
	// WBP의 RootSizeBox를 바꾸면 실제 화면 크기도 그대로 바뀌게 한다.
	// 위 DrawSize는 Widget의 DesiredSize가 아직 계산되지 않은 첫 프레임의 안전한 초기값이다.
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

	// 다시 불려도 구독이 겹치지 않게 먼저 푼다.
	UnbindHealthDelegate();
	HealthChangedHandle = OwnedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AASMonsterCharacter::HandleHealthChanged);

	UnbindDamageEffectDelegate();
	DamageEffectAppliedHandle = OwnedAbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf
		.AddUObject(this, &AASMonsterCharacter::HandleGameplayEffectApplied);

	// 타게팅 공격은 서버에서만 실행되지만 ActivationOwnedTag는 Minimal 복제로 모든 클라이언트에 전달된다.
	// 따라서 각 클라이언트가 자기 메시의 MID만 바꾸면 되고, 머티리얼 상태를 별도로 복제할 필요가 없다.
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
			// 이후 다른 시스템이 슬롯을 바꿨다면 그 값을 덮어쓰지 않는다.
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

		// 전투 전파는 한 번만 수행한다. NearbyMonster가 다시 주변을 순회하지 않는다.
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
	OwnedHealthAttributeSet->InitMaxHealth(InitialAttributes.MaxHealth);
	OwnedHealthAttributeSet->InitHealth(InitialAttributes.MaxHealth);
	OwnedCombatAttributeSet->InitMoveSpeed(InitialAttributes.MoveSpeed);
	OwnedCombatAttributeSet->InitAttackPower(InitialAttributes.AttackPower);
	GetCharacterMovement()->MaxWalkSpeed = InitialAttributes.MoveSpeed;

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
	// 죽음 판정은 서버만 한다. 피해 Effect delegate에서 막타 정보를 기록할 수 있도록
	// 실제 죽음 처리는 다음 틱으로 미룬다. 클라이언트는 복제된 Dead 태그로 알게 된다.
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

	// Minimal 복제 모드에서는 GameplayEffect가 주는 태그처럼 TagOnly 상태로 복제해야 한다.
	// 사망 GameplayEffect를 만들기 전까지는 이 API로 Dead 태그를 모든 클라이언트에 전달한다.
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

	// 사망 직전의 공격 몽타주/타격 이벤트가 계속되는 것을 막는다.
	FGameplayTagContainer AttackAbilityTags;
	AttackAbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	OwnedAbilitySystemComponent->CancelAbilities(&AttackAbilityTags, nullptr, nullptr);

	StopMovementAndAI();
	SetCanBeDamaged(false);
	// Actor 플래그만 끄면 Blueprint에 추가한 무기/장식 Mesh 등의 Primitive Component가 계속
	// Pawn을 Block하거나 NavMesh 장애물로 남을 수 있다. 모든 Primitive Component를 함께 끈다.
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
	// RVO는 충돌과 별도의 군집 회피 정보다. 사체가 회피 대상으로 남지 않게 명시적으로 해제한다.
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

void AASMonsterCharacter::ApplyHitReact()
{
	if (!HasAuthority() || IsDead() || !MonsterDefinition || !MonsterDefinition->HitReactEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = OwnedAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle HitReactSpec = OwnedAbilitySystemComponent->MakeOutgoingSpec(
		MonsterDefinition->HitReactEffect, 1.0f, EffectContext);
	if (!HitReactSpec.IsValid())
	{
		return;
	}

	OwnedAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*HitReactSpec.Data.Get());

	// 새 공격은 HitReact 태그로 막고, 이미 진행 중인 공격은 즉시 끝낸다.
	FGameplayTagContainer AttackAbilityTags;
	AttackAbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	OwnedAbilitySystemComponent->CancelAbilities(&AttackAbilityTags, nullptr, nullptr);

	if (AASMonsterAIController* AIController = Cast<AASMonsterAIController>(GetController()))
	{
		AIController->NotifyMonsterHitReact();
	}
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

	// 프로젝트의 피해 Effect는 Data.Damage를 음수로 넣어 Health에 더한다.
	const float DamageMagnitude = AppliedEffect.GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f);
	if (DamageMagnitude >= 0.0f)
	{
		return;
	}

	const FGameplayEffectContextHandle& EffectContext = AppliedEffect.GetContext();
	// 플레이어 ASC는 PlayerState에 있으므로 OriginalInstigator가 Pawn이 아닐 수 있다.
	// EffectCauser는 실제 시전자 Avatar(플레이어 캐릭터)이므로 타깃 컴포넌트가 유효한 공격자로 판정할 수 있다.
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

	// 치명타는 체력 변경 콜백에서 사망 처리가 예약됐으므로 경직과 어그로를 만들지 않는다.
	if (IsDead() || bDeathHandlingQueued)
	{
		return;
	}

	// 투사체가 파괴된 뒤에도 Effect Context의 EffectCauser(발사한 플레이어)를 사용한다.
	NotifyDamagedBy(InstigatorActor);
	ApplyHitReact();
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
	// 환경 피해나 스크립트 피해는 이전 플레이어의 막타 기록을 이어받지 않는다.
	LastDamageKillerController.Reset();

	AController* pController = Cast<AController>(InstigatorActor);
	if (!Controller)
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
