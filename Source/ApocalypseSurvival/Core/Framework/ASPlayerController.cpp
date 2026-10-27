#include "Core/Framework/ASPlayerController.h"

#include <Core/Input/ASInputTags.h>

#include "AbilitySystemGlobals.h"
#include "ApocalypseSurvival.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Characters/Player/Components/ASPlayerAttackMoveComponent.h"
#include "Core/Cheats/ASCheatManager.h"
#include "Core/Framework/ASPlayerCameraManager.h"
#include "Core/Framework/ASBossEncounter.h"
#include "Core/Input/ASInputConfig.h"
#include "Core/Settings/ASLocalSettingsSubsystem.h"
#include "Core/Framework/ASGameState.h"
#include "Core/UI/ASInGameMenuWidget.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"
#include "Skills/ASSkillTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "InputActionValue.h"
#include "MapSystem/UI/ASMapViewModel.h"
#include "MapSystem/UI/ASMapViewModelSubsystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Items/Interaction/ASItemInteractionComponent.h"

void AASPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
		for (UUserWidget* Widget : Widgets)
		{
			if (Widget->GetOwningPlayer() == this) Widget->RemoveFromParent();
		}
	}
	Super::EndPlay(EndPlayReason);
}

AASPlayerController::AASPlayerController()
{
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("PathFollowingComponent"));
	ItemInteractionComponent = CreateDefaultSubobject<UASItemInteractionComponent>(TEXT("ItemInteractionComponent"));
	InventoryComponent = CreateDefaultSubobject<UASInventoryComponent>(TEXT("InventoryComponent"));
	AttackMoveComponent = CreateDefaultSubobject<UASPlayerAttackMoveComponent>(TEXT("AttackMoveComponent"));
	PlayerCameraManagerClass = AASPlayerCameraManager::StaticClass();
	InGameMenuWidgetClass = UASInGameMenuWidget::StaticClass();
	CheatClass = UASCheatManager::StaticClass();
	InGameMenuWidgetClass = UASInGameMenuWidget::StaticClass();

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AASPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AttackMoveComponent->OnAttackRequested.AddUObject(this, &AASPlayerController::HandleAttackMoveRequest);

	// BeginPlay 가 아니라 여기서 연결한다. 클라이언트에서는 폰이 첫 복제 묶음에
	// 실려 와서 BeginPlay 보다 먼저 알림이 나갈 수 있기 때문이다.
	OnPossessedPawnChanged.AddDynamic(this, &AASPlayerController::HandlePossessedPawnChanged);
}

void AASPlayerController::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	AttackMoveComponent->CancelAttackMove();
	ItemInteractionComponent->CancelPendingInteraction();
	ItemInteractionComponent->CloseLoot();
	// 길찾기 컴포넌트는 자기가 움직일 폰의 이동 컴포넌트를 기억해 둬야 한다.
	// 엔진은 그 계기가 되는 알림(OnNewPawn)을 서버의 Possess 에서만 보낸다.
	// 클라이언트는 폰을 복제로 받기 때문에 알림이 오지 않고, 그대로 두면
	// "SimpleMove failed: movement not allowed" 로 이동이 전부 실패한다.
	// 이 알림은 서버와 클라이언트 양쪽에서 오므로 여기서 다시 찾게 한다.
	PathFollowingComponent->UpdateCachedComponents();

	// HUD 는 로컬 플레이어에게만 있다. 리슨 서버의 원격 플레이어 컨트롤러는 알리지 않는다.
	if (IsLocalController())
	{
		OnLocalPawnChanged.Broadcast(NewPawn);
	}
}

void AASPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 입력은 화면 앞에 사람이 있는 컨트롤러에만 건다.
	// 리슨 서버에는 다른 플레이어의 컨트롤러도 존재한다.
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		UASLocalSettingsSubsystem* Settings = GetLocalPlayer()->GetSubsystem<UASLocalSettingsSubsystem>();
		Subsystem->AddMappingContext(Settings ? Settings->GetRuntimeMappingContext(DefaultMappingContext) : DefaultMappingContext.Get(), 0);
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInputComponent))
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("'%s' has no EnhancedInputComponent. Input will not be bound."), *GetNameSafe(this));
		return;
	}

	// Started 는 누른 순간 한 번, Triggered 는 누르고 있는 동안 매 프레임 온다.
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Started, this, &AASPlayerController::OnMoveStarted);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AASPlayerController::OnMoveHeld);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AASPlayerController::OnMoveCompleted);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AASPlayerController::OnMoveCompleted);
	EnhancedInputComponent->BindAction(StopAction, ETriggerEvent::Started, this, &AASPlayerController::OnStop);

	EnhancedInputComponent->BindAction(CameraLockToggleAction, ETriggerEvent::Started, this, &AASPlayerController::OnCameraLockToggled);
	EnhancedInputComponent->BindAction(CameraCenterAction, ETriggerEvent::Started, this, &AASPlayerController::OnCameraCenterStarted);
	EnhancedInputComponent->BindAction(CameraCenterAction, ETriggerEvent::Completed, this, &AASPlayerController::OnCameraCenterCompleted);
	EnhancedInputComponent->BindAction(CameraCenterAction, ETriggerEvent::Canceled, this, &AASPlayerController::OnCameraCenterCompleted);
	EnhancedInputComponent->BindAction(CameraZoomAction, ETriggerEvent::Triggered, this, &AASPlayerController::OnCameraZoom);
	if (ToggleMapAction)
	{
		EnhancedInputComponent->BindAction(ToggleMapAction, ETriggerEvent::Started, this, &AASPlayerController::OnToggleMap);
	}

	// 어빌리티 입력은 목록을 돌며 건다. 키마다 함수를 만들지 않고, 태그를 인자로 같이 넘긴다.
	// Started 는 누른 순간 한 번이라 "한 번 누름 = 한 번 발동"이 된다.
	if (IsValid(AbilityInputConfig))
	{
		for (const FASInputTagAction& Entry : AbilityInputConfig->AbilityInputActions)
		{
			if (!IsValid(Entry.InputAction) || !Entry.InputTag.IsValid())
			{
				continue;
			}

			EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Started, this, &AASPlayerController::OnAbilityInputPressed, Entry.InputTag);
			EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Completed, this, &AASPlayerController::OnAbilityInputReleased, Entry.InputTag);
			EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Canceled, this, &AASPlayerController::OnAbilityInputReleased, Entry.InputTag);
		}
	}
}

bool AASPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (bBossIntroInputBlocked) return true;
	if (IsLocalController() && Params.Key == EKeys::Escape && Params.Event == IE_Pressed && !IsResultPhase())
	{
		ToggleInGameMenu();
		return true;
	}
	return Super::InputKey(Params);
}

void AASPlayerController::OnAbilityInputPressed(const FGameplayTag InputTag)
{
	if (bBossIntroInputBlocked) return;
	if (IsInGameMenuOpen()) return;
	// A는 일반 공격 어빌리티를 즉시 발동하지 않고, 다음 좌클릭을 기다리는 공격 명령으로 해석한다.
	// 준비가 실제로 시작된 뒤에만 공격 이동 상태를 바꾼다. 발동 실패(쿨타임·비용 등)는
	// 이전 이동/공격 이동 명령을 그대로 유지해야 한다.
	if (InputTag.MatchesTagExact(ASGameplayTags::InputTag_Attack_Prepare))
	{
		if (UASAbilitySystemComponent* AbilitySystemComponent = GetPawnAbilitySystemComponent())
		{
			const bool bWasTargeting = AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Targeting);
			AbilitySystemComponent->AbilityInputTagPressed(InputTag);

			if (bWasTargeting)
			{
				// A를 다시 누르는 것은 현재 조준/공격 이동 명령 취소다.
				AttackMoveComponent->CancelAttackMove();
			}
			else if (AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Targeting))
			{
				// Primary Attack이 실제로 조준 상태에 들어간 경우에만 준비 상태를 만든다.
				AttackMoveComponent->ArmAttackMove();
			}
		}
		return;
	}

	// 좌클릭은 조준 확정이다. A를 누른 상태라면 이동 입력이 아니라 여기서
	// 공격 이동 대상 또는 투사체 방향을 기록한 뒤 기존 GAS 확정 흐름으로 넘긴다.
	if (InputTag.MatchesTagExact(ASGameplayTags::InputTag_Attack_Primary) && AttackMoveComponent->IsArmed())
	{
		FVector AttackDestination;
		if (GetCursorWorldLocation(AttackDestination))
		{
			AttackMoveComponent->ConfirmAttackMove(AttackDestination);

			// 적을 아직 찾지 못한 공격 이동은 좌클릭 확정을 보류한다.
			// 컴포넌트가 이동 중 사거리 안의 적을 찾았을 때만 확정한다.
			if (AttackMoveComponent->HasActiveCommand())
			{
				// A 조준은 이번 좌클릭으로 끝난다. 적이 없어서 이동만 시작하더라도
				// 사거리 인디케이터를 남겨 두지 않는다.
				if (UASAbilitySystemComponent* AbilitySystemComponent = GetPawnAbilitySystemComponent())
				{
					AbilitySystemComponent->LocalInputCancel();
				}
				return;
			}
		}
	}
	else if (InputTag.MatchesTagExact(ASGameplayTags::InputTag_Attack_Primary)
		&& AttackMoveComponent->HasActiveCommand())
	{
		// 진행 중인 공격 이동에 다시 좌클릭하면 허공 공격으로 새지 않고 명령을 취소한다.
		AttackMoveComponent->CancelAttackMove();
		return;
	}

	if (UASAbilitySystemComponent* AbilitySystemComponent = GetPawnAbilitySystemComponent())
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AASPlayerController::OnAbilityInputReleased(const FGameplayTag InputTag)
{
	if (UASAbilitySystemComponent* AbilitySystemComponent = GetPawnAbilitySystemComponent())
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
}

void AASPlayerController::ASEquipWeapon()
{
	for (const UASItemInstance* Item : InventoryComponent->GetItemInstances())
	{
		const UASItemDefinition* Definition = IsValid(Item) ? Item->GetItemDefinition() : nullptr;
		if (IsValid(Definition) && Definition->ItemType == EASItemType::Weapon)
		{
			UE_LOG(LogTemp, Log, TEXT("[Equip] %s 장착 요청"), *Definition->DisplayName.ToString());
			ItemInteractionComponent->RequestEquipItem(Item->GetInstanceId());
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Equip] 인벤토리에 무기가 없다. 상자에서 먼저 주워야 한다."));
}

void AASPlayerController::ASUnequipWeapon()
{
	ItemInteractionComponent->RequestUnequipItem();
}

bool AASPlayerController::IsPawnDead() const
{
	const AASPlayerCharacter* PlayerCharacter = Cast<AASPlayerCharacter>(GetPawn());
	return IsValid(PlayerCharacter) && PlayerCharacter->IsDead();
}

UASAbilitySystemComponent* AASPlayerController::GetPawnAbilitySystemComponent() const
{
	return Cast<UASAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn()));
}

void AASPlayerController::CancelMovementForCommittedAbility()
{
	// 이 컨트롤러가 실제로 움직임을 지시한 로컬 폰에서만 기존 명령을 정리한다.
	// 원격 플레이어의 서버 컨트롤러까지 멈추게 하면 클라이언트 예측 이동과 경쟁할 수 있다.
	if (!IsLocalController())
	{
		return;
	}

	AttackMoveComponent->CancelAttackMove();
	StopMovement();
}

void AASPlayerController::HandleAttackMoveRequest(const FASPrimaryAttackRequest& Request)
{
	PendingAttackMoveTarget = Request.Target;
	PendingAttackMoveTargetLocation = Request.TargetLocation;
	PendingAttackMoveMassTargetNetworkId = Request.MassTargetNetworkId;
	bHasPendingAttackMoveRequest = true;
	TryActivatePrimaryAttack();
}

bool AASPlayerController::ConsumeAttackMoveRequest(AActor*& OutTarget, FVector& OutTargetLocation,
	uint32& OutMassTargetNetworkId)
{
	if (!bHasPendingAttackMoveRequest)
	{
		return false;
	}

	OutTarget = PendingAttackMoveTarget.Get();
	OutTargetLocation = PendingAttackMoveTargetLocation;
	OutMassTargetNetworkId = PendingAttackMoveMassTargetNetworkId;
	PendingAttackMoveTarget.Reset();
	PendingAttackMoveTargetLocation = FVector::ZeroVector;
	PendingAttackMoveMassTargetNetworkId = 0;
	bHasPendingAttackMoveRequest = false;
	return true;
}

void AASPlayerController::TryActivatePrimaryAttack()
{
	UASAbilitySystemComponent* AbilitySystemComponent = GetPawnAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!Spec.IsActive() && Spec.Ability
			&& Spec.Ability->GetClass()->IsChildOf(UASGameplayAbility_CharacterPrimaryAttack::StaticClass()))
		{
			AbilitySystemComponent->TryActivateAbility(Spec.Handle);
			return;
		}
	}
}

void AASPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	if (!IsLocalController() || !IsValid(GetLocalPlayer()))
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	// 클라이언트에도 콘솔 명령 진입점을 만든다. 실제 상태 변경은 서버 RPC가 맡는다.
	EnableCheats();
#endif

	// 커서가 보이는 채로 플레이하고, 커서는 창 밖으로 나가면 안 된다.
	// 가장자리 스크롤은 커서가 테두리에서 멈춰야 동작하고,
	// 전투 중에 다른 창을 클릭하면 게임이 포커스를 잃는다.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AASPlayerController::ServerDebugGod_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (AASPlayerCharacter* PlayerCharacter = Cast<AASPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ToggleDebugInvulnerability();
	}
	else
	{
		ClientMessage(TEXT("[DebugGod] No player character is possessed."));
	}
#endif
}

void AASPlayerController::ServerDebugGiveWeapon_Implementation(FName WeaponName)
{
#if !UE_BUILD_SHIPPING
	UASCheatManager::GiveWeaponToPlayer(this, WeaponName);
#endif
}

void AASPlayerController::PlayerTick(const float DeltaTime)
{
	if (bBossIntroInputBlocked) return;
	Super::PlayerTick(DeltaTime);
	if (IsInGameMenuOpen())
	{
		if (IsResultPhase()) CloseInGameMenu();
		return;
	}

	UpdateCameraEdgeScroll(DeltaTime);
}

bool AASPlayerController::IsInGameMenuOpen() const
{
	return InGameMenuWidget && InGameMenuWidget->IsInViewport();
}

bool AASPlayerController::IsResultPhase() const
{
	const AASGameState* State = GetWorld() ? GetWorld()->GetGameState<AASGameState>() : nullptr;
	return State && (State->GetCurrentPhase() == EGamePhase::Victory || State->GetCurrentPhase() == EGamePhase::Defeat);
}

void AASPlayerController::ToggleInGameMenu()
{
	if (bBossIntroInputBlocked) return;
	if (!IsLocalController()) return;
	if (IsInGameMenuOpen())
	{
		CloseInGameMenu();
		return;
	}
	if (IsResultPhase() || !InGameMenuWidgetClass) return;
	InGameMenuWidget = CreateWidget<UASInGameMenuWidget>(this, InGameMenuWidgetClass);
	if (!InGameMenuWidget) return;
	bCursorVisibleBeforeMenu = bShowMouseCursor;
	OnMoveCompleted();
	OnStop();
	bHasPendingAttackMoveRequest = false;
	OnCameraCenterCompleted();
	ItemInteractionComponent->CloseLoot();
	if (UASAbilitySystemComponent* Abilities = GetPawnAbilitySystemComponent())
	{
		if (Abilities->HasMatchingGameplayTag(ASGameplayTags::State_Targeting)) Abilities->LocalInputCancel();
		if (AbilityInputConfig)
		{
			for (const FASInputTagAction& Entry : AbilityInputConfig->AbilityInputActions)
			{
				Abilities->AbilityInputTagReleased(Entry.InputTag);
			}
		}
	}
	InGameMenuWidget->AddToViewport(20);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(InGameMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AASPlayerController::CloseInGameMenu()
{
	if (!IsLocalController() || !InGameMenuWidget) return;
	InGameMenuWidget->RemoveFromParent();
	InGameMenuWidget = nullptr;
	// The result screen owns focus once the match has ended.
	if (IsResultPhase()) return;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = bCursorVisibleBeforeMenu;
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

AASPlayerCameraManager* AASPlayerController::GetASPlayerCameraManager() const
{
	return Cast<AASPlayerCameraManager>(PlayerCameraManager);
}

bool AASPlayerController::GetCursorWorldLocation(FVector& OutLocation) const
{
	FHitResult Hit;
	if (!GetHitResultUnderCursor(CursorTraceChannel, true, Hit))
	{
		return false;
	}

	OutLocation = Hit.Location;
	return true;
}

void AASPlayerController::RequestMoveTo(const FVector& Destination)
{
	if (bBossIntroInputBlocked) return;
	if (!IsValid(GetPawn()))
	{
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!IsValid(NavSys))
	{
		return;
	}

	// 벽이나 이동할 수 없는 곳을 클릭해도 무시하지 않고,
	// 갈 수 있는 가장 가까운 지점까지 가게 한다.
	FNavLocation NavLocation;
	if (!NavSys->ProjectPointToNavigation(Destination, NavLocation, NavProjectExtent))
	{
		return;
	}

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, NavLocation.Location);
	LastMoveRequestTime = GetWorld()->GetTimeSeconds();
}

void AASPlayerController::ServerAcknowledgeBossIntro_Implementation(AASBossEncounter* Encounter)
{
	if (IsValid(Encounter) && Encounter->GetWorld() == GetWorld())
	{
		Encounter->AcknowledgePlayer(this);
	}
}

void AASPlayerController::SetBossIntroInputBlocked(bool bBlocked)
{
	if (bBossIntroInputBlocked == bBlocked) return;
	if (bBlocked)
	{
		bFullTickBeforeBossIntro = bShouldPerformFullTickWhenPaused;
		bTickWhenPausedBeforeBossIntro = PrimaryActorTick.bTickEvenWhenPaused;
	}
	// Camera-manager updates still need the controller's full paused tick; PlayerTick blocks gameplay input separately.
	bShouldPerformFullTickWhenPaused = bBlocked ? true : bFullTickBeforeBossIntro;
	PrimaryActorTick.bTickEvenWhenPaused = bBlocked ? true : bTickWhenPausedBeforeBossIntro;
	// Releases that occur while the overlay owns focus must not leave gameplay keys held down.
	FlushPressedKeys();
	bBossIntroInputBlocked = bBlocked;
}

void AASPlayerController::OnMoveStarted()
{
	if (IsInGameMenuOpen()) return;
	bInteractionClickHeld = false;
	if (IsPawnDead() || ItemInteractionComponent->IsLootOpen()) return;

	// MoveAction은 우클릭 기본 이동 전용이다. 조준 상태와 무관하게 항상 이동한다.
	if (UASAbilitySystemComponent* AbilitySystemComponent = GetPawnAbilitySystemComponent();
		AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Targeting))
	{
		AbilitySystemComponent->LocalInputCancel();
	}
	AttackMoveComponent->NotifyExternalMoveRequested();
	FHitResult Hit;
	if (GetHitResultUnderCursor(CursorTraceChannel, true, Hit)
		&& ItemInteractionComponent->TryInteractWith(Hit.GetActor()))
	{
		bInteractionClickHeld = true;
		return;
	}
	ItemInteractionComponent->CancelPendingInteraction();
	FVector Destination;
	if (!GetCursorWorldLocation(Destination))
	{
		return;
	}

	RequestMoveTo(Destination);

	if (IsValid(MoveCursorFX))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MoveCursorFX, Destination);
	}
}

void AASPlayerController::OnMoveHeld()
{
	if (IsInGameMenuOpen()) return;
	if (AttackMoveComponent->HasActiveCommand()) return;
	if (IsPawnDead() || bInteractionClickHeld || ItemInteractionComponent->HasPendingInteraction() || ItemInteractionComponent->IsLootOpen()) return;
	// 매 프레임 길을 다시 찾는 것은 낭비라서 누르고 있는 동안에는 간격을 둔다.
	if (GetWorld()->GetTimeSeconds() - LastMoveRequestTime < HeldMoveInterval)
	{
		return;
	}

	FVector Destination;
	if (GetCursorWorldLocation(Destination))
	{
		RequestMoveTo(Destination);
	}
}

void AASPlayerController::OnMoveCompleted()
{
	bInteractionClickHeld = false;
}

void AASPlayerController::OnStop()
{
	AttackMoveComponent->CancelAttackMove();
	ItemInteractionComponent->CancelPendingInteraction();
	StopMovement();
}

void AASPlayerController::OnCameraLockToggled()
{
	if (AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager())
	{
		CameraManager->SetLockedToPawn(!CameraManager->IsLockedToPawn());
	}
}

void AASPlayerController::OnCameraCenterStarted()
{
	if (AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager())
	{
		CameraManager->SetCentering(true);
	}
}

void AASPlayerController::OnCameraCenterCompleted()
{
	if (AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager())
	{
		CameraManager->SetCentering(false);
	}
}

void AASPlayerController::OnCameraZoom(const FInputActionValue& Value)
{
	if (AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager())
	{
		CameraManager->AddZoomInput(Value.Get<float>());
	}
}

void AASPlayerController::OnToggleMap()
{
	if (UASMapViewModelSubsystem* Subsystem = UASMapViewModelSubsystem::Get(this))
	{
		if (UASMapViewModel* ViewModel = Subsystem->GetMapViewModel())
		{
			ViewModel->ToggleExpandedMap();
		}
	}
}

void AASPlayerController::UpdateCameraEdgeScroll(const float DeltaTime)
{
	if (ItemInteractionComponent->IsLootOpen()) return;
	AASPlayerCameraManager* CameraManager = GetASPlayerCameraManager();
	if (!IsValid(CameraManager))
	{
		return;
	}

	// 게임 창이 활성 상태가 아니면(Alt+Tab 등) 스크롤하지 않는다.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const FViewport* Viewport = IsValid(LocalPlayer) && IsValid(LocalPlayer->ViewportClient) ? LocalPlayer->ViewportClient->Viewport : nullptr;
	if (Viewport == nullptr || !FSlateApplication::Get().IsActive())
	{
		return;
	}

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}

	// GetMousePosition 이 아니라 하드웨어 커서 위치를 직접 읽는다.
	// 뷰포트는 커서가 밖으로 나갔다는 알림을 받으면 기억해 둔 커서 위치를 지우고,
	// 다음에 마우스가 움직일 때에야 되살린다. 이 알림은 화면 가장자리에서 나는데,
	// 가장자리에 붙은 커서는 더 움직이지 않는다. 그래서 GetMousePosition 을 쓰면
	// 플레이어가 마우스를 흔들 때까지 스크롤이 멈춘다.
	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
	const FVector2D Normalized = Viewport->VirtualDesktopPixelToViewport(FIntPoint(FMath::RoundToInt32(CursorPos.X), FMath::RoundToInt32(CursorPos.Y)));
	const double MouseX = Normalized.X * ViewportSize.X;
	const double MouseY = Normalized.Y * ViewportSize.Y;

	// 커서는 뷰포트에 갇혀 있어서 밖으로 나가도 몇 픽셀 수준이다.
	// 그보다 멀리 있으면 잠금이 풀린 것이고(PIE 의 Shift+F1 등),
	// 플레이어가 다른 일을 하고 있다는 뜻이다.
	if (MouseX < -EdgeScrollOvershoot || MouseX > ViewportSize.X + EdgeScrollOvershoot
		|| MouseY < -EdgeScrollOvershoot || MouseY > ViewportSize.Y + EdgeScrollOvershoot)
	{
		return;
	}

	FVector2D PanDirection = FVector2D::ZeroVector;
	if (MouseX <= EdgeScrollMargin)
	{
		PanDirection.X = -1.0;
	}
	else if (MouseX >= ViewportSize.X - EdgeScrollMargin)
	{
		PanDirection.X = 1.0;
	}

	// 뷰포트의 Y 는 아래로 갈수록 커진다. 화면 위쪽이 +Y 가 되도록 뒤집는다.
	if (MouseY <= EdgeScrollMargin)
	{
		PanDirection.Y = 1.0;
	}
	else if (MouseY >= ViewportSize.Y - EdgeScrollMargin)
	{
		PanDirection.Y = -1.0;
	}

	CameraManager->AddPanInput(PanDirection, DeltaTime);
}
