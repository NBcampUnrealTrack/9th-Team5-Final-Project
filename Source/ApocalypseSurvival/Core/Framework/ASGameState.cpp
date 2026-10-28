#include "Core/Framework/ASGameState.h"
#include "Net/UnrealNetwork.h"
#include "MapSystem/Zone/Core/ASMapStateComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/MovementComponent.h"
#include "MassSimulationSubsystem.h"
#include "Items/Combat/ASWeaponProjectile.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"
#include "Skills/Area/ASSkillAreaActor.h"
#include "Mass/Monster/Combat/ASMassMonsterProjectile.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

AASGameState::AASGameState()
{
	MapState = CreateDefaultSubobject<UASMapStateComponent>(TEXT("MapState"));
}

void AASGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AASGameState, CurrentPhase);
	DOREPLIFETIME(AASGameState, CurrentDay);
	DOREPLIFETIME(AASGameState, MonsterStatDifficulty);
	DOREPLIFETIME(AASGameState, PhaseEndServerTime);
	DOREPLIFETIME(AASGameState, WaveSpawnedCount);
	DOREPLIFETIME(AASGameState, WaveKilledCount);
	DOREPLIFETIME(AASGameState, TeamRevivesRemaining);
}

void AASGameState::SetPhase(EGamePhase NewPhase, float Duration)
{
	if (!HasAuthority() || IsGameEnded()) return;
	
	CurrentPhase = NewPhase;
	PhaseEndServerTime = (Duration > 0.f) ? GetServerWorldTimeSeconds() + Duration : 0.f;
	
	// 서버에서는 OnRep가 자동 호출되지 않으므로 직접 호출
	OnRep_CurrentPhase();
}

void AASGameState::SetCurrentDay(int32 NewDay)
{
	if (!HasAuthority()) return;
	CurrentDay = NewDay;
}

void AASGameState::SetMonsterStatDifficulty(const float NewDifficulty)
{
	if (!HasAuthority()) return;
	MonsterStatDifficulty = FMath::Max(0.0f, NewDifficulty);
}

void AASGameState::ResetWaveCounts()
{
	if (!HasAuthority()) return;
	WaveSpawnedCount = 0;
	WaveKilledCount = 0;
}

void AASGameState::AddWaveSpawned(int32 Count)
{
	if (!HasAuthority()) return;
	WaveSpawnedCount += Count;
}

void AASGameState::AddWaveKilled()
{
	if (!HasAuthority()) return;
	++WaveKilledCount;
}

void AASGameState::SetTeamRevivesRemaining(int32 NewCount)
{
	if (!HasAuthority()) return;
	TeamRevivesRemaining = FMath::Max(0, NewCount);
	OnRep_TeamRevivesRemaining();
}

bool AASGameState::ConsumeTeamRevive()
{
	if (!HasAuthority() || TeamRevivesRemaining <= 0) return false;
	
	--TeamRevivesRemaining;
	UE_LOG(LogTemp, Log, TEXT("[Revive] 팀 부활 사용, 남은 횟수 %d"), TeamRevivesRemaining);
	OnRep_TeamRevivesRemaining();
	return true;
}

void AASGameState::OnRep_TeamRevivesRemaining()
{
	OnTeamRevivesChanged.Broadcast(TeamRevivesRemaining);
}

float AASGameState::GetPhaseRemainingTime() const
{
	if (PhaseEndServerTime <= 0.f) return 0.f;
	return FMath::Max(0.f, PhaseEndServerTime - GetServerWorldTimeSeconds());
}

void AASGameState::OnRep_CurrentPhase()
{
	if (IsGameEnded()) StopGameplay();
	OnPhaseChanged.Broadcast(CurrentPhase);
}

void AASGameState::StopGameplay()
{
	if (bGameplayStopped) return;
	bGameplayStopped = true;

	// Keep world/network/UI ticking; only the gameplay simulation is stopped.
	if (UMassSimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UMassSimulationSubsystem>())
	{
		Simulation->PauseSimulation();
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const bool bPawn = Actor->IsA<APawn>();
		const bool bProjectile = Actor->IsA<AASWeaponProjectile>()
			|| Actor->IsA<AASSkillProjectileActor>() || Actor->IsA<AASMassMonsterProjectile>();
		if (!bPawn && !bProjectile && !Actor->IsA<AASSkillAreaActor>()) continue;

		if (UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
		{
			Abilities->CancelAllAbilities();
		}
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (AController* Controller = Pawn->GetController())
			{
				if (UPathFollowingComponent* Path = Controller->FindComponentByClass<UPathFollowingComponent>())
				{
					Path->AbortMove(*this, FPathFollowingResultFlags::ForcedScript);
				}
			}
			if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
			{
				AI->StopMovement();
				if (UBrainComponent* Brain = AI->GetBrainComponent()) Brain->StopLogic(TEXT("Game ended"));
			}
		}

		Actor->SetActorTickEnabled(false);
		Actor->SetActorEnableCollision(false);
		GetWorldTimerManager().ClearAllTimersForObject(Actor);
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* Component : Components)
		{
			if (UMovementComponent* Movement = Cast<UMovementComponent>(Component)) Movement->StopMovementImmediately();
			Component->SetComponentTickEnabled(false);
			GetWorldTimerManager().ClearAllTimersForObject(Component);
		}
	}
}
