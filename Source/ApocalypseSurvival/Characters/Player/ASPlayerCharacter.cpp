#include "Characters/Player/ASPlayerCharacter.h"

#include "Characters/Player/Components/ASEquipmentComponent.h"
#include "Characters/Player/Components/ASProgressionComponent.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/Data/ASCharacterRoster.h"
#include "Mass/Monster/Targeting/ASMonsterTargetableComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/Combat/ASCombatImpactSettings.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Team/ASTeamTypes.h"
#include "Core/Framework/ASCharacterSelectSubsystem.h"
#include "Core/Framework/ASGameState.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectExtension.h"
#include "GAS/ASAbilitySet.h"
#include "Modules/ASModuleComponent.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/ASAbilityTags.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Items/ASItemInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EngineUtils.h"

namespace ASPlayerCharacterTargeting
{
	constexpr float BaseScore = 10.0f;

	EASMonsterTargetCapability GetCapabilities()
	{
		return static_cast<EASMonsterTargetCapability>(
			static_cast<uint8>(EASMonsterTargetCapability::PrimaryTarget)
			| static_cast<uint8>(EASMonsterTargetCapability::ThreatSource)
			| static_cast<uint8>(EASMonsterTargetCapability::GlobalCandidate));
	}
}

AASPlayerCharacter::AASPlayerCharacter()
{
	TeamId = ASTeam::Player();

	EquipmentComponent = CreateDefaultSubobject<UASEquipmentComponent>(TEXT("EquipmentComponent"));
	MonsterTargetableComponent = CreateDefaultSubobject<UASMonsterTargetableComponent>(TEXT("MonsterTargetableComponent"));
	MonsterTargetableComponent->ConfigureTargetProfile(
		ASPlayerCharacterTargeting::GetCapabilities(),
		ASPlayerCharacterTargeting::BaseScore,
		42.0f);

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 카메라가 회전하지 않으므로 캐릭터는 컨트롤러 방향이 아니라 걷는 방향을 본다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0f, 1080.0f, 0.0f);
	MoveComp->bConstrainToPlane = true;
	MoveComp->bSnapToPlaneAtStart = true;

	// 클릭하면 바로 출발하고 바로 멈추게 한다. 롤 계열 캐릭터의 반응에 맞춘 값이다.
	MoveComp->MaxAcceleration = 8192.0f;
	MoveComp->BrakingDecelerationWalking = 8192.0f;

	// 길찾기 이동을 "가속(입력)"으로 굴리게 한다.
	// 기본값은 속도를 직접 넣는 방식인데, 클라이언트가 서버로 보내는 이동 기록에는
	// 그 속도가 들어가지 않는다. 그러면 서버는 캐릭터가 서 있다고 계산해서
	// 본인 클라이언트의 위치를 매 프레임 되돌려 놓는다.
	MoveComp->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}

void AASPlayerCharacter::MulticastDrawWeaponAttackDebug_Implementation(
	const FVector Start,
	const FVector End,
	const float Radius,
	const bool bIsMelee,
	const bool bHit)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor Color = bHit ? FColor::Red : FColor::Green;
	if (bIsMelee)
	{
		DrawDebugSphere(World, End, Radius, 16, Color, false, 1.0f, 0, 2.0f);
	}
	else
	{
		const FVector Direction = (End - Start).GetSafeNormal();
		const float Length = FVector::Distance(Start, End);
		const FVector Center = Start + Direction * (Length * 0.5f);
		const FQuat Rotation = Direction.IsNearlyZero()
			? FQuat::Identity
			: FRotationMatrix::MakeFromZ(Direction).ToQuat();

		if (Radius > 0.0f)
		{
			DrawDebugCapsule(
				World,
				Center,
				Length * 0.5f,
				Radius,
				Rotation,
				Color,
				false,
				1.0f,
				0,
				2.0f);
		}
		else
		{
			DrawDebugLine(World, Start, End, Color, false, 1.0f, 0, 2.0f);
		}
	}
}

void AASPlayerCharacter::MulticastDrawMeleeConeDebug_Implementation(
	const FVector Origin,
	const FVector Direction,
	const float Radius,
	const float AngleDegrees,
	const bool bHit)
{
	const UWorld* World = GetWorld();
	if (!World || Direction.IsNearlyZero())
	{
		return;
	}

	const FColor Color = bHit ? FColor::Red : FColor::Green;
	const float HalfAngle = AngleDegrees * 0.5f;

	// 부채꼴 테두리: 양쪽 직선 + 호를 선분으로 이어 그린다.
	constexpr int32 ArcSegments = 12;
	FVector PreviousPoint = Origin + Direction.RotateAngleAxis(-HalfAngle, FVector::UpVector) * Radius;
	DrawDebugLine(World, Origin, PreviousPoint, Color, false, 1.0f, 0, 2.0f);

	for (int32 Index = 1; Index <= ArcSegments; ++Index)
	{
		const float Angle = -HalfAngle + AngleDegrees * (static_cast<float>(Index) / ArcSegments);
		const FVector Point = Origin + Direction.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
		DrawDebugLine(World, PreviousPoint, Point, Color, false, 1.0f, 0, 2.0f);
		PreviousPoint = Point;
	}

	DrawDebugLine(World, Origin, PreviousPoint, Color, false, 1.0f, 0, 2.0f);
}

void AASPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();

	// 첫 빙의면 캐릭터 정의의 기본 스탯을 넣고(한 번만), 리스폰이면 이전 캐릭터의 사망 상태를 걷어 낸다.
	// ASC 연결보다 먼저 해야 한다. 연결할 때 이동 속도를 읽어 가고, 연결 직후 "이미 죽어 있나" 검사가 돈다.
	if (IsValid(ASPlayerState))
	{
		// 다른 곳(나중의 캐릭터 선택)에서 이미 정해 줬으면 그 값을 쓴다.
		if (!ASPlayerState->GetCharacterDefinition())
		{
			ASPlayerState->SetCharacterDefinition(CharacterDefinition);
		}
		
		ASPlayerState->ApplyCharacterDefinitionOnce();
		ASPlayerState->ResetForRespawn();
	}

	InitAbilitySystemFromPlayerState();

	if (IsValid(ASPlayerState))
	{
		ASPlayerState->GrantDefaultAbilitySetOnce(DefaultAbilitySet);
		ASPlayerState->GrantCharacterAbilitySetOnce();
		ASPlayerState->GetModuleComponent()->InitializeForAvatar();
	}

	// 리스폰이면 PlayerState 에 장착 기록이 남아 있다. 옛 몸과 함께 사라진 무기를 새 몸에 다시 단다.
	// ASC 연결 뒤여야 공격 어빌리티를 부여할 수 있어서 맨 마지막에 한다.
	if (IsValid(ASPlayerState))
	{
		if (UASItemInstance* EquippedItem = ASPlayerState->GetEquippedItemRecord())
		{
			EquipmentComponent->EquipItem(EquippedItem);
		}
	}
}

void AASPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitAbilitySystemFromPlayerState();
}

bool AASPlayerCharacter::IsDead() const
{
	const UASAbilitySystemComponent* ASC = Cast<UASAbilitySystemComponent>(GetAbilitySystemComponent());
	return IsValid(ASC) && ASC->IsDead();
}

bool AASPlayerCharacter::IsMonsterTargetable_Implementation() const
{
	// 쓰러진 플레이어는 부활을 기다리는 중이므로 몬스터의 추적·공격 후보가 아니다.
	return !IsDead() && !IsDowned();
}

FVector AASPlayerCharacter::GetMonsterTargetLocation_Implementation() const
{
	return GetActorLocation();
}

float AASPlayerCharacter::GetMonsterTargetRadius_Implementation() const
{
	return GetCapsuleComponent()->GetScaledCapsuleRadius();
}

EASMonsterTargetCapability AASPlayerCharacter::GetMonsterTargetCapabilities_Implementation() const
{
	return ASPlayerCharacterTargeting::GetCapabilities();
}

float AASPlayerCharacter::GetMonsterTargetBaseScore_Implementation() const
{
	return ASPlayerCharacterTargeting::BaseScore;
}

void AASPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ImpactReactionTimerHandle);
	GetWorldTimerManager().ClearTimer(StaggerProtectionTimerHandle);

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (AASPlayerState* ASPlayerState = Cast<AASPlayerState>(ASC->GetOwnerActor()))
		{
			ASPlayerState->GetModuleComponent()->SuspendForAvatar(this);
		}
	}
	// ASC 는 PlayerState 에 남으므로 파괴되는 캐릭터의 구독을 반드시 풀어야 한다.
	UnbindDeathDelegates();
	
	UnbindProgressionDelegates();

	Super::EndPlay(EndPlayReason);
}

void AASPlayerCharacter::OnAbilitySystemInitialized()
{
	Super::OnAbilitySystemInitialized();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	// PlayerState 가 다시 복제되면 또 불릴 수 있다. 두 번 걸리지 않게 먼저 푼다.
	UnbindDeathDelegates();

	// 죽음 판정은 서버만 한다. 클라이언트는 아래 태그로 알게 된다.
	if (HasAuthority())
	{
		HealthChangedHandle = ASC
			->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
			.AddUObject(this, &AASPlayerCharacter::HandleHealthChanged);
	}

	DeadTagChangedHandle = ASC
		->RegisterGameplayTagEvent(ASGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AASPlayerCharacter::HandleDeadTagChanged);

	DownedTagChangedHandle = ASC
	->RegisterGameplayTagEvent(ASGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved)
	.AddUObject(this, &AASPlayerCharacter::HandleDownedTagChanged);

	StaggeredTagChangedHandle = ASC
		->RegisterGameplayTagEvent(ASGameplayTags::State_Staggered, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AASPlayerCharacter::HandleStaggeredTagChanged);

	// 늦게 접속한 클라이언트는 이미 죽어 있는 플레이어를 받을 수 있다. 태그 알림은 그때 오지 않는다.
	if (IsDead())
	{
		ApplyLocalDeath();
	}
	
	// 늦게 접속한 클라이언트는 이미 쓰러진 플레이어를 받을 수 있다.
	if (IsDowned())
	{
		HandleDownedTagChanged(ASGameplayTags::State_Downed, 1);
	}

	if (IsStaggered())
	{
		HandleStaggeredTagChanged(ASGameplayTags::State_Staggered, 1);
	}

	// 늦은 ASC 초기화나 리스폰 뒤에도 서버 Registry가 현재 상태와 일치해야 한다.
	RefreshMonsterTargetingRegistration();
	
	BindProgressionDelegates();
}

void AASPlayerCharacter::InitAbilitySystemFromPlayerState()
{
	AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();
	if (!IsValid(ASPlayerState))
	{
		return;
	}

	// ASC 의 주인은 PlayerState, 아바타는 이 캐릭터다.
	InitAbilitySystem(ASPlayerState->GetASAbilitySystemComponent(), ASPlayerState);
}

void AASPlayerCharacter::UnbindDeathDelegates()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC
				->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}

		if (DeadTagChangedHandle.IsValid())
		{
			ASC
				->RegisterGameplayTagEvent(ASGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
				.Remove(DeadTagChangedHandle);
		}
		
		if (DownedTagChangedHandle.IsValid())
		{
			ASC
				->RegisterGameplayTagEvent(ASGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved)
				.Remove(DownedTagChangedHandle);
		}

		if (StaggeredTagChangedHandle.IsValid())
		{
			ASC
				->RegisterGameplayTagEvent(ASGameplayTags::State_Staggered, EGameplayTagEventType::NewOrRemoved)
				.Remove(StaggeredTagChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	DeadTagChangedHandle.Reset();
	DownedTagChangedHandle.Reset();
	StaggeredTagChangedHandle.Reset();
}

void AASPlayerCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (Data.NewValue > 0.0f || !HasAuthority() || !IsValid(ASC) || IsDead() || IsDowned())
	{
		return;
	}

	// 멀티 규칙: 바로 죽지 않고 쓰러져서 파티원의 부활을 기다린다.
	const AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>();
	if (GameMode && GameMode->IsDownedEnabled())
	{
		EnterDowned();
		return;
	}

	// 누가 죽였는지를 이벤트에 싣는다. 효과로 깎인 경우에만 알 수 있고, 디버그 명령처럼 직접 바꾼 경우는 비어 있다.
	FGameplayEventData Payload;
	Payload.EventTag = ASGameplayTags::GameplayEvent_Death;
	Payload.Target = this;
	if (Data.GEModData != nullptr)
	{
		Payload.ContextHandle = Data.GEModData->EffectSpec.GetEffectContext();
		Payload.Instigator = Payload.ContextHandle.GetOriginalInstigator();
		Payload.OptionalObject = Data.GEModData->EffectSpec.Def;
	}

	RequestDeath(Payload);
}

void AASPlayerCharacter::RequestDeath(const FGameplayEventData& Payload)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || IsDead())
	{
		return;
	}

	// 사망 어빌리티가 이 이벤트로 켜진다. 돌려주는 값은 켜진 어빌리티 수다.
	const int32 ActivatedCount = ASC->HandleGameplayEvent(ASGameplayTags::GameplayEvent_Death, &Payload);
	if (ActivatedCount == 0)
	{
		// 어빌리티 세트에 사망 어빌리티가 없다. 연출 없이 바로 죽인다. 리스폰이 막히는 것보다 낫다.
		UE_LOG(LogTemp, Warning, TEXT("[Player] %s 사망 어빌리티가 없어 연출 없이 사망 처리한다. DA_AbilitySet_Player 에 GA_Death 를 넣어야 한다."), *GetName());
		ASC->CancelAllAbilities();
		StartDeath();
		FinishDeath();
	}
}

bool AASPlayerCharacter::IsDowned() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(ASGameplayTags::State_Downed);
}

bool AASPlayerCharacter::IsStaggered() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(ASGameplayTags::State_Staggered);
}

void AASPlayerCharacter::ApplyImpactReaction(const FASCombatImpactContext& ImpactContext)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || IsDead() || IsDowned())
	{
		return;
	}

	const UASCombatImpactSettings* ImpactSettings = GetDefault<UASCombatImpactSettings>();
	if (!ImpactSettings)
	{
		return;
	}

	const EASResolvedHitReaction Reaction = ImpactSettings->ResolvePlayerReaction(
		ImpactContext.Preset,
		ASC->HasMatchingGameplayTag(ASGameplayTags::State_SuperArmor),
		ASC->HasMatchingGameplayTag(ASGameplayTags::State_StaggerProtected));
	if (Reaction != EASResolvedHitReaction::StrongStagger
		&& Reaction != EASResolvedHitReaction::Knockdown)
	{
		return;
	}

	FGameplayTagContainer SurvivingTags;
	SurvivingTags.AddTag(ASGameplayTags::Ability_Behavior_SurvivesDeath);
	ASC->CancelAbilities(nullptr, &SurvivingTags);

	if (Reaction == EASResolvedHitReaction::Knockdown)
	{
		ASC->AddLooseGameplayTag(ASGameplayTags::State_KnockedDown, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Staggered, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	ASC->AddLooseGameplayTag(ASGameplayTags::State_StaggerProtected, 1,
		EGameplayTagReplicationState::TagAndCountToAll);

	GetWorldTimerManager().SetTimer(ImpactReactionTimerHandle, this, &AASPlayerCharacter::ClearImpactReaction,
		FMath::Max(KINDA_SMALL_NUMBER, ImpactSettings->GetPlayerReactionDuration(Reaction)), false);
	GetWorldTimerManager().SetTimer(StaggerProtectionTimerHandle, this, &AASPlayerCharacter::ClearStaggerProtection,
		FMath::Max(KINDA_SMALL_NUMBER, ImpactSettings->PlayerProtectionDuration), false);
}

void AASPlayerCharacter::EnterDowned()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || IsDead() || IsDowned())
	{
		return;
	}

	// 하던 공격, 스킬, 회피를 끊는다. 사망과 같이 SurvivesDeath 가 붙은 것만 남긴다.
	FGameplayTagContainer SurvivingTags;
	SurvivingTags.AddTag(ASGameplayTags::Ability_Behavior_SurvivesDeath);
	ASC->CancelAbilities(nullptr, &SurvivingTags);

	// 모든 머신에 복제되는 태그. 이 줄에서 서버의 HandleDownedTagChanged 가 바로 불린다.
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Downed, 1, EGameplayTagReplicationState::TagAndCountToAll);

	GetWorldTimerManager().SetTimer(BleedOutTimerHandle, this, &AASPlayerCharacter::BleedOut, DownedDuration, false);

	if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
	{
		GameMode->OnPlayerDowned(GetController());
	}
}

void AASPlayerCharacter::BleedOut()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || !IsDowned())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(BleedOutTimerHandle);

	// 사망을 먼저 시작해서 사망 태그가 붙은 뒤에 쓰러짐 태그를 뗀다. 그 사이 이동이 잠깐 살아나지 않게 하기 위해서다.
	FGameplayEventData Payload;
	Payload.EventTag = ASGameplayTags::GameplayEvent_Death;
	Payload.Target = this;
	RequestDeath(Payload);

	ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Downed, 1, EGameplayTagReplicationState::TagAndCountToAll);
}

void AASPlayerCharacter::HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshMonsterTargetingRegistration();

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();

	if (NewCount > 0)
	{
		// 진행 중인 길찾기를 끊고 제자리에 멈춘다. 충돌은 유지해서 파티원이 다가갈 수 있게 한다.
		if (AController* OwnController = GetController())
		{
			OwnController->StopMovement();
		}
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		OnDownedChanged(true);
	}
	else
	{
		// 부활이면 다시 걷는다. 시간 초과로 사망하는 중이면 사망 처리가 이동을 맡는다.
		if (!IsDead() && !IsStaggered())
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
		OnDownedChanged(false);
	}
}

void AASPlayerCharacter::HandleStaggeredTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (NewCount > 0)
	{
		if (AController* OwnController = GetController())
		{
			OwnController->StopMovement();
		}
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		OnStaggerChanged(true);
	}
	else
	{
		if (!IsDead() && !IsDowned())
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
		OnStaggerChanged(false);
	}
}

void AASPlayerCharacter::ClearImpactReaction()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC))
	{
		return;
	}

	ASC->RemoveLooseGameplayTag(ASGameplayTags::State_KnockedDown, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Staggered, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
}

void AASPlayerCharacter::ClearStaggerProtection()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (HasAuthority() && IsValid(ASC))
	{
		ASC->RemoveLooseGameplayTag(ASGameplayTags::State_StaggerProtected, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void AASPlayerCharacter::RefreshMonsterTargetingRegistration()
{
	if (!HasAuthority() || !MonsterTargetableComponent)
	{
		return;
	}

	// 등록을 해제하면 이미 이 Handle을 보유한 Mass 몬스터도 다음 ResolveTarget부터 즉시 실패한다.
	MonsterTargetableComponent->SetTargetingEnabled(!IsDead() && !IsDowned());
}

void AASPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AASPlayerCharacter, bBeingRevived);
}

void AASPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 캐릭터 전용 매핑을 추가한다. 리스폰으로 새 캐릭터가 붙어도 같은 에셋이라 중복되지 않는다.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (CharacterMappingContext)
			{
				Subsystem->AddMappingContext(CharacterMappingContext, 1);
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ReviveAction)
		{
			EnhancedInput->BindAction(ReviveAction, ETriggerEvent::Started, this, &AASPlayerCharacter::OnRevivePressed);
		}
		if (SpectateNextAction)
		{
			EnhancedInput->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &AASPlayerCharacter::OnSpectateNextPressed);
		}
	}
}

void AASPlayerCharacter::OnRevivePressed()
{
	if (IsDead() || IsDowned())
	{
		return;
	}

	AASPlayerCharacter* Target = FindReviveTarget();
	if (!Target)
	{
		return;
	}

	// 부활 중에 움직이면 취소되므로 이동을 먼저 멈춘다.
	if (AController* OwnController = GetController())
	{
		OwnController->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();

	ServerRequestRevive(Target);
}

AASPlayerCharacter* AASPlayerCharacter::FindReviveTarget() const
{
	AASPlayerCharacter* BestTarget = nullptr;
	float BestDistanceSq = FMath::Square(ReviveRange);

	for (TActorIterator<AASPlayerCharacter> It(GetWorld()); It; ++It)
	{
		AASPlayerCharacter* Other = *It;
		if (Other == this || !Other->IsDowned() || Other->IsDead() || Other->IsBeingRevived())
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Other->GetActorLocation(), GetActorLocation());
		if (DistanceSq <= BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestTarget = Other;
		}
	}

	return BestTarget;
}

void AASPlayerCharacter::ServerRequestRevive_Implementation(AASPlayerCharacter* Target)
{
	// 클라이언트 요청이라 서버에서 다시 확인한다.
	if (!Target || Target == this || IsDead() || IsDowned())
	{
		return;
	}
	
	// 팀 부활 횟수가 남아 있어야 부활을 시작할 수 있다. (차감은 완료할 때)
	const AASGameState* GS = GetWorld()->GetGameState<AASGameState>();
	if (!GS || GS->GetTeamRevivesRemaining() <= 0)
	{
		return;
	}

	if (FVector::DistSquared(Target->GetActorLocation(), GetActorLocation()) > FMath::Square(Target->ReviveRange))
	{
		return;
	}

	Target->StartRevive(this);
}

void AASPlayerCharacter::StartRevive(AASPlayerCharacter* Reviver)
{
	if (!HasAuthority() || !Reviver || !IsDowned() || IsDead() || bBeingRevived)
	{
		return;
	}

	ActiveReviver = Reviver;
	ReviveElapsed = 0.f;
	bBeingRevived = true;
	OnRep_BeingRevived(); // 서버는 OnRep 이 자동으로 불리지 않는다.

	// 부활시키는 사람이 공격, 스킬, 회피를 쓰면 취소한다.
	if (UAbilitySystemComponent* ReviverASC = Reviver->GetAbilitySystemComponent())
	{
		ReviverAbilitySystem = ReviverASC;
		ReviverAbilityActivatedHandle = ReviverASC->AbilityActivatedCallbacks.AddUObject(this, &AASPlayerCharacter::HandleReviverAbilityActivated);
	}

	// 부활받는 동안은 사망까지 남은 시간이 줄지 않는다.
	GetWorldTimerManager().PauseTimer(BleedOutTimerHandle);

	constexpr float TickInterval = 0.1f;
	GetWorldTimerManager().SetTimer(ReviveTickTimerHandle, this, &AASPlayerCharacter::TickRevive, TickInterval, true);

	UE_LOG(LogTemp, Log, TEXT("[Downed] %s 가 %s 부활 시작"), *GetNameSafe(Reviver), *GetName());
}

void AASPlayerCharacter::TickRevive()
{
	const AASPlayerCharacter* Reviver = ActiveReviver.Get();

	// 부활시키는 사람이 움직이거나, 멀어지거나, 쓰러지거나, 죽으면 취소. 범위와 속도에 약간 여유를 둔다.
	constexpr float RangeMargin = 50.f;
	constexpr float MoveSpeedThreshold = 50.f;
	const bool bStillValid = IsDowned() && Reviver && !Reviver->IsDead() && !Reviver->IsDowned()
		&& Reviver->GetVelocity().SizeSquared2D() <= FMath::Square(MoveSpeedThreshold)
		&& FVector::DistSquared(Reviver->GetActorLocation(), GetActorLocation()) <= FMath::Square(ReviveRange + RangeMargin);

	if (!bStillValid)
	{
		CancelRevive();
		return;
	}

	ReviveElapsed += GetWorldTimerManager().GetTimerRate(ReviveTickTimerHandle);
	if (ReviveElapsed >= ReviveDuration)
	{
		Revive();
	}
}

void AASPlayerCharacter::HandleReviverAbilityActivated(UGameplayAbility* Ability)
{
	UE_LOG(LogTemp, Log, TEXT("[Downed] 부활 중 어빌리티 사용(%s)"), *GetNameSafe(Ability));
	CancelRevive();
}

void AASPlayerCharacter::ClearReviver()
{
	// ASC 는 PlayerState 에 남으므로 구독을 반드시 푼다.
	if (UAbilitySystemComponent* ReviverASC = ReviverAbilitySystem.Get())
	{
		ReviverASC->AbilityActivatedCallbacks.Remove(ReviverAbilityActivatedHandle);
	}

	ReviverAbilitySystem.Reset();
	ReviverAbilityActivatedHandle.Reset();
	ActiveReviver.Reset();
}

void AASPlayerCharacter::CancelRevive()
{
	if (!HasAuthority() || !bBeingRevived)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ReviveTickTimerHandle);
	ClearReviver();
	bBeingRevived = false;
	OnRep_BeingRevived();

	// 멈춰 둔 사망 타이머를 이어서 진행
	GetWorldTimerManager().UnPauseTimer(BleedOutTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("[Downed] %s 부활 취소"), *GetName());
}

void AASPlayerCharacter::Revive()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || !IsDowned() || IsDead())
	{
		return;
	}
	
	// 완료하는 순간 팀 부활 횟수를 쓴다. 그사이 다른 사람이 마지막 횟수를 썼으면 부활 실패
	AASGameState* GS = GetWorld()->GetGameState<AASGameState>();
	if (!GS || !GS->ConsumeTeamRevive())
	{
		CancelRevive();
		return;
	}

	GetWorldTimerManager().ClearTimer(ReviveTickTimerHandle);
	GetWorldTimerManager().ClearTimer(BleedOutTimerHandle);
	ClearReviver();
	bBeingRevived = false;
	OnRep_BeingRevived();

	// 체력을 먼저 채운 뒤 쓰러짐 태그를 뗀다. 체력이 0 인 채로 태그가 떨어지면 다시 쓰러질 수 있다.
	const float MaxHealth = ASC->GetNumericAttribute(UASHealthAttributeSet::GetMaxHealthAttribute());
	ASC->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(), MaxHealth * ReviveHealthRatio);
	ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Downed, 1, EGameplayTagReplicationState::TagAndCountToAll);

	UE_LOG(LogTemp, Log, TEXT("[Downed] %s 부활 완료 (체력 %.0f)"), *GetName(), MaxHealth * ReviveHealthRatio);
	
	// 이번 부활로 팀 부활 횟수를 다 썼으면, 쓰러져 있는 다른 사람은 더 살릴 수 없으므로 바로 사망 처리
	if (GS->GetTeamRevivesRemaining() <= 0)
	{
		if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
		{
			GameMode->KillUnrevivablePlayers();
		}
	}
}

void AASPlayerCharacter::OnRep_BeingRevived()
{
	OnBeingRevivedChanged(bBeingRevived);
}

void AASPlayerCharacter::OnSpectateNextPressed()
{
	// 살아 있을 때 좌클릭은 공격이다. 사망(관전) 중일 때만 처리한다.
	if (!IsDead())
	{
		return;
	}
	
	ServerRequestSpectateNext();
}

void AASPlayerCharacter::MulticastHideCorpse_Implementation()
{
	// 붙어 있는 액터(무기 등)도 같이 숨기고 끈다.
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);
	for (AActor* Attached : AttachedActors)
	{
		if (IsValid(Attached))
		{
			Attached->SetActorHiddenInGame(true);
			Attached->SetActorEnableCollision(false);
			Attached->SetActorTickEnabled(false);
		}
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);

	// 메시 애니메이션, 이동 컴포넌트 같은 컴포넌트 틱도 끈다.
	TInlineComponentArray<UActorComponent*> Components(this);
	for (UActorComponent* Component : Components)
	{
		Component->SetComponentTickEnabled(false);
	}
}

void AASPlayerCharacter::ServerRequestSpectateNext_Implementation()
{
	if (!IsDead())
	{
		return;
	}
	
	if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
	{
		GameMode->SpectateNext(Cast<APlayerController>(GetController()));
	}
}

void AASPlayerCharacter::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshMonsterTargetingRegistration();

	if (NewCount > 0)
	{
		ApplyLocalDeath();
	}
	else
	{
		// 클라이언트에서는 새 캐릭터가 태그 제거보다 먼저 도착할 수 있다. 그러면 새 몸이
		// 연결 직후 검사에서 죽은 것으로 보여 사망 처리가 걸리고, 뒤늦게 태그가 떨어진다.
		// 그때 되돌려 주지 않으면 충돌이 꺼진 채 걷기 시작해 바닥을 뚫고 떨어진다.
		RestoreFromDeath();
	}
}

void AASPlayerCharacter::StartDeath()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || IsDead())
	{
		return;
	}

	// 모든 머신에 복제되는 태그다. Minimal 방식은 Mixed 모드에서 본인 클라이언트가 받지 못해 쓰지 않는다.
	// 이 줄에서 서버의 HandleDeadTagChanged 가 바로 불린다. 다른 어빌리티 취소는 사망 어빌리티가 맡는다.
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
}

void AASPlayerCharacter::FinishDeath()
{
	if (!HasAuthority() || !IsDead() || bDeathFinished)
	{
		return;
	}
	bDeathFinished = true;

	// 데스카운트와 리스폰은 게임모드 규칙이다. 테스트 게임모드처럼 다른 클래스면 리스폰은 없다.
	if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
	{
		GameMode->OnPlayerDied(GetController());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] %s 사망. 게임모드가 AASGameMode 가 아니라 리스폰하지 않는다."), *GetName());
	}
}

void AASPlayerCharacter::ApplyLocalDeath()
{
	// 진행 중인 길찾기를 끊는다. 명령을 내린 머신의 컨트롤러가 들고 있다.
	if (AController* OwnController = GetController())
	{
		OwnController->StopMovement();
	}

	// 이동을 완전히 끈다. 중력도 같이 꺼지므로 충돌을 꺼도 바닥으로 빠지지 않는다.
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();

	// 시체가 길을 막거나 몬스터 공격 판정에 걸리지 않게 한다.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OnDeathStarted();
}

void AASPlayerCharacter::RestoreFromDeath()
{
	// ApplyLocalDeath 가 끈 것을 같은 순서로 되돌린다. 충돌을 먼저 켜야 걷기 모드가 바닥을 찾는다.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetDefaultMovementMode();
}

void AASPlayerCharacter::DebugAddExp(int32 Amount)
{
	// 경험치는 서버만 바꿀 수 있다. 클라이언트에서 입력하면 서버에 요청한다.
	if (!HasAuthority())
	{
		ServerDebugAddExp(Amount);
		return;
	}
	
	AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();
	UASProgressionComponent* Progression = ASPlayerState ? ASPlayerState->GetProgressionComponent() : nullptr;
	if (!Progression)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] 성장 컴포넌트를 찾지 못함"));
		return;
	}
	
	Progression->AddExperience(Amount);
}

void AASPlayerCharacter::ServerDebugAddExp_Implementation(int32 Amount)
{
	DebugAddExp(Amount);
}

void AASPlayerCharacter::DebugKillSelf()
{
	if (!HasAuthority())
	{
		ServerDebugKillSelf();
		return;
	}
	
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		// 실제 피해와 같은 경로(체력 0 -> 쓰러짐 또는 사망)를 탄다.
		ASC->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(), 0.f);
	}
}

void AASPlayerCharacter::ServerDebugKillSelf_Implementation()
{
	DebugKillSelf();
}

void AASPlayerCharacter::DebugSelectCharacter(int32 Index)
{
	UGameInstance* GameInstance = GetGameInstance();
	UASCharacterSelectSubsystem* SelectSubsystem = GameInstance ? GameInstance->GetSubsystem<UASCharacterSelectSubsystem>() : nullptr;
	if (!SelectSubsystem)
	{
		return;
	}
	
	const UASCharacterDefinition* Definition = DebugCharacterRoster ? DebugCharacterRoster->GetCharacterAt(Index) : nullptr;
	if (!Definition)
	{
		const FString Message = FString::Printf(TEXT("[CharacterSelect] %d 번 캐릭터가 없다. BP_PlayerCharacter의 DebugCharacterRoster를 확인."), Index);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, Message);
		}
		return;
	}
	
	SelectSubsystem->SetSelectedCharacter(Definition);
	DebugShowSelectedCharacter();
}

void AASPlayerCharacter::DebugShowSelectedCharacter()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UASCharacterSelectSubsystem* SelectSubsystem = GameInstance ? GameInstance->GetSubsystem<UASCharacterSelectSubsystem>() : nullptr;
	const UASCharacterDefinition* Definition = SelectSubsystem ? SelectSubsystem->GetSelectedCharacter() : nullptr;
	
	const FString Message = Definition
		? FString::Printf(TEXT("[CharacterSelect] 선택: %s / 캐릭터 BP: %s"), *GetNameSafe(Definition), *GetNameSafe(SelectSubsystem->GetSelectedPawnClass().Get()))
		: FString(TEXT("[CharacterSelect] 선택 없음"));
	
	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, Message);
	}
}

void AASPlayerCharacter::BindProgressionDelegates()
{
	const AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();
	if (UASProgressionComponent* Progression = ASPlayerState ? ASPlayerState->GetProgressionComponent() : nullptr)
	{
		Progression->OnLevelChanged.AddUniqueDynamic(this, &ThisClass::HandleLevelChanged);
	}
}

void AASPlayerCharacter::UnbindProgressionDelegates()
{
	const AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();
	if (UASProgressionComponent* Progression = ASPlayerState ? ASPlayerState->GetProgressionComponent() : nullptr)
	{
		Progression->OnLevelChanged.RemoveDynamic(this, &ThisClass::HandleLevelChanged);
	}
}

void AASPlayerCharacter::HandleLevelChanged(int32 NewLevel)
{
	// 화면이 없는 데디케이티드 서버는 연출이 필요 없다
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	
	if (LevelUpEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(LevelUpEffect, GetRootComponent(), NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	
	if (LevelUpSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LevelUpSound, GetActorLocation());
	}
	
	OnLevelUp(NewLevel);
}
