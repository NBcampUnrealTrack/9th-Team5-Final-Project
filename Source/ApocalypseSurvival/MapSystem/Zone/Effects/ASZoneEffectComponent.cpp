#include "MapSystem/Zone/Effects/ASZoneEffectComponent.h"

#include "AbilitySystemComponent.h"
#include "ApocalypseSurvival.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "MapSystem/Shared/ASMapAbilityUtils.h"
#include "MapSystem/Zone/Core/ASMapFunctionLibrary.h"
#include "MapSystem/Zone/Core/ASMapStateComponent.h"
#include "MapSystem/Zone/Core/ASMapWorldSubsystem.h"
#include "MapSystem/Zone/Effects/ASZoneEffectDefinition.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace ASZoneEffectComponentPrivate
{
	uint8 GetTeamId(const APawn* Pawn)
	{
		const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(Pawn);
		if (!TeamAgent && Pawn)
		{
			TeamAgent = Cast<IGenericTeamAgentInterface>(Pawn->GetController());
		}
		return TeamAgent ? TeamAgent->GetGenericTeamId().GetId() : FGenericTeamId::NoTeam.GetId();
	}

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(
		TEXT("AS.Zone.EffectDump"),
		TEXT("Log tracked zone effects on the server/host."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
			const UASZoneEffectComponent* Component = GameMode ? GameMode->FindComponentByClass<UASZoneEffectComponent>() : nullptr;
			if (Component)
			{
				Component->DumpState();
			}
			else
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Zone: no zone effect component on the GameMode (server/host only)."));
			}
		}));
}

UASZoneEffectComponent::UASZoneEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	static ConstructorHelpers::FObjectFinder<UASZoneEffectDefinition> ZoneEffectDefinitionFinder(
		TEXT("/Game/ApocalypseSurvival/MapSystem/Data/DA_ZoneEffectDefinition.DA_ZoneEffectDefinition"));
	ZoneEffectDefinition = ZoneEffectDefinitionFinder.Object;
}

void UASZoneEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!UASMapFunctionLibrary::LevelHasMapZones(this))
	{
		return;
	}

	TArray<FString> Errors;
	if (!IsValid(ZoneEffectDefinition))
	{
		Errors.Add(TEXT("ZoneEffectDefinition is null."));
	}
	else
	{
		ZoneEffectDefinition->ValidateDefinition(Errors);
	}
	if (!Errors.IsEmpty())
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("Zone effects disabled: %s"), *FString::Join(Errors, TEXT("; ")));
		return;
	}

	bRunning = true;
	World->GetTimerManager().SetTimer(UpdateTimer, this, &UASZoneEffectComponent::UpdateEffects,
		ZoneEffectDefinition->UpdateInterval, true);
	// 이미 준비된 맵도 즉시 처리하고, 이후의 스냅샷 변경을 구독한다.
	UpdateEffects();
}

void UASZoneEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bRunning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	BindMapState(nullptr);
	RemoveAllEffects();
	Super::EndPlay(EndPlayReason);
}

void UASZoneEffectComponent::BindMapState(UASMapStateComponent* NewMapState)
{
	if (MapState.Get() == NewMapState)
	{
		return;
	}
	if (UASMapStateComponent* OldMapState = MapState.Get())
	{
		OldMapState->OnMapStateUpdated.RemoveDynamic(this, &UASZoneEffectComponent::HandleMapStateUpdated);
		OldMapState->OnMatchReset.RemoveDynamic(this, &UASZoneEffectComponent::HandleMatchReset);
	}
	MapState = NewMapState;
	if (NewMapState)
	{
		NewMapState->OnMapStateUpdated.AddDynamic(this, &UASZoneEffectComponent::HandleMapStateUpdated);
		NewMapState->OnMatchReset.AddDynamic(this, &UASZoneEffectComponent::HandleMatchReset);
	}
}

void UASZoneEffectComponent::UpdateEffects()
{
	UWorld* World = GetWorld();
	if (!bRunning || bUpdating || !World || World->GetNetMode() == NM_Client || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	// GAS의 적용/제거 알림에서 맵 이벤트가 발생해도 순회에 재진입하지 않는다.
	TGuardValue<bool> UpdatingGuard(bUpdating, true);
	UASMapStateComponent* State = UASMapFunctionLibrary::GetMapStateComponent(this);
	BindMapState(State);
	if (!State || !State->GetSnapshot().bMapReady || !State->GetSnapshot().bMapRunning || !IsValid(ZoneEffectDefinition))
	{
		RemoveAllEffects();
		return;
	}
	WarnUnknownZoneRows();

	// PlayerState의 ASC가 새 몸을 가리키기 전에 붙었던 두 효과를 먼저 제거한다.
	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		UAbilitySystemComponent* ASC = It.Key().Get();
		APawn* Pawn = It.Value().Pawn.Get();
		if (!ASC || !Pawn || ASC->GetAvatarActor() != Pawn
			|| ASMapAbilityUtils::FindAbilitySystemComponent(Pawn) != ASC)
		{
			FEntry Entry = It.Value();
			It.RemoveCurrent();
			ReleaseEntry(ASC, Entry);
		}
	}

	TSet<TWeakObjectPtr<UAbilitySystemComponent>> Seen;
	for (APawn* Pawn : TActorRange<APawn>(World))
	{
		if (!IsValid(Pawn))
		{
			continue;
		}
		UAbilitySystemComponent* ASC = ASMapAbilityUtils::FindAbilitySystemComponent(Pawn);
		// 이전 몸도 같은 ASC를 반환할 수 있으므로 새 아바타의 항목을 건드리지 않는다.
		if (!IsValid(ASC) || ASC->GetAvatarActor() != Pawn)
		{
			continue;
		}
		Seen.Add(ASC);
		const FName ZoneId = UASMapFunctionLibrary::FindZoneIdAtLocation(this, Pawn->GetActorLocation());
		const FASZoneRuntimeState* Zone = ZoneId.IsNone() ? nullptr : State->GetSnapshot().FindZone(ZoneId);
		if (!Zone || Zone->DangerLevel < ASMap::MinDangerLevel || Zone->DangerLevel > ASMap::MaxDangerLevel
			|| ASC->HasAnyMatchingGameplayTags(ZoneEffectDefinition->BlockedTags))
		{
			RemoveEntry(ASC);
			continue;
		}

		const uint8 TeamId = ASZoneEffectComponentPrivate::GetTeamId(Pawn);
		const FASZoneEffectRow* BaseRow = ZoneEffectDefinition->FindBaseRow(TeamId);
		const FASZoneEffectRow* Row = ZoneEffectDefinition->FindZoneRow(ZoneId, TeamId);
		FEntry& Entry = Entries.FindOrAdd(ASC);
		Entry.Pawn = Pawn;

		// 기본 행과 위험도가 같으면 구역 이동만으로는 기본 효과를 다시 적용하지 않는다.
		FEffectSlot& DangerSlot = Entry.DangerEffect;
		SyncSlot(ASC, DangerSlot, BaseRow, Zone->DangerLevel);

		// 구역 슬롯은 선택된 행의 키, GE, 위험도가 바뀔 때만 다시 적용한다.
		FEffectSlot& ZoneSlot = Entry.ZoneEffect;
		SyncSlot(ASC, ZoneSlot, Row, Zone->DangerLevel);

		if (!DangerSlot.Handle.IsValid() && !ZoneSlot.Handle.IsValid())
		{
			Entries.Remove(ASC);
		}
	}

	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			UAbilitySystemComponent* ASC = It.Key().Get();
			FEntry Entry = It.Value();
			It.RemoveCurrent();
			ReleaseEntry(ASC, Entry);
		}
	}
}

void UASZoneEffectComponent::WarnUnknownZoneRows()
{
	if (bCheckedZoneRows)
	{
		return;
	}
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	// 맵이 준비되고 실행된 후 최초 한 번만 검사한다. 매치 초기화로 경고를 반복하지 않는다.
	bCheckedZoneRows = true;
	const UDataTable* Table = ZoneEffectDefinition->ZoneEffectTable;
	if (!IsValid(Table) || Table->GetRowStruct() != FASZoneEffectRow::StaticStruct())
	{
		return;
	}

	TArray<FString> UnknownRows;
	for (const auto& Pair : Table->GetRowMap())
	{
		const FASZoneEffectRow* Row = reinterpret_cast<const FASZoneEffectRow*>(Pair.Value);
		if (Row && !Row->ZoneId.IsNone() && !Subsystem->FindZone(Row->ZoneId))
		{
			UnknownRows.Add(FString::Printf(TEXT("%s (ZoneId=%s)"), *Pair.Key.ToString(), *Row->ZoneId.ToString()));
		}
	}
	if (!UnknownRows.IsEmpty())
	{
		UnknownRows.Sort();
		UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Zone: ZoneEffectTable '%s' has rows with no zone in the level: %s"),
			*Table->GetName(), *FString::Join(UnknownRows, TEXT(", ")));
	}
}

FActiveGameplayEffectHandle UASZoneEffectComponent::ApplyEffect(UAbilitySystemComponent* ASC,
	TSubclassOf<UGameplayEffect> Effect, const TArray<FASZoneEffectMagnitude>& Magnitudes, int32 DangerLevel)
{
	if (!Effect)
	{
		return FActiveGameplayEffectHandle();
	}
	// 시작 시 검증을 통과했어도 런타임 편집으로 바뀐 지속 정책을 다시 확인한다.
	if (Effect.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Infinite)
	{
		UClass* EffectClass = Effect.Get();
		if (!WarnedNonInfiniteEffects.Contains(EffectClass))
		{
			WarnedNonInfiniteEffects.Add(EffectClass);
			UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Zone: skipping effect '%s': zone effects must have Infinite duration (periodic effects are allowed)."),
				*GetNameSafe(EffectClass));
		}
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Effect, DangerLevel, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	for (const FASZoneEffectMagnitude& Magnitude : Magnitudes)
	{
		float Value = 0.0f;
		if (!UASZoneEffectDefinition::GetMagnitudeForLevel(Magnitude, DangerLevel, Value))
		{
			return FActiveGameplayEffectHandle();
		}
		Spec.Data->SetSetByCallerMagnitude(Magnitude.DataTag, Value);
	}
	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UASZoneEffectComponent::SyncSlot(UAbilitySystemComponent* ASC, FEffectSlot& Slot,
	const FASZoneEffectRow* Row, int32 DangerLevel)
{
	if (Row && Row->Effect)
	{
		if (Slot.ZoneId != Row->ZoneId || Slot.TeamId != Row->TeamId
			|| Slot.Effect != Row->Effect || Slot.DangerLevel != DangerLevel
			|| !ASC->GetActiveGameplayEffect(Slot.Handle))
		{
			RemoveSlot(ASC, Slot);
			Slot.Handle = ApplyEffect(ASC, Row->Effect, Row->Magnitudes, DangerLevel);
			if (Slot.Handle.IsValid())
			{
				Slot.ZoneId = Row->ZoneId;
				Slot.TeamId = Row->TeamId;
				Slot.Effect = Row->Effect;
				Slot.DangerLevel = DangerLevel;
			}
		}
	}
	else
	{
		RemoveSlot(ASC, Slot);
	}
}

void UASZoneEffectComponent::RemoveSlot(UAbilitySystemComponent* ASC, FEffectSlot& Slot)
{
	const FActiveGameplayEffectHandle Handle = Slot.Handle;
	// 제거 알림에서 조회할 때는 이 슬롯이 이미 비어 있어야 한다.
	Slot = FEffectSlot();
	if (IsValid(ASC) && Handle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}
}

void UASZoneEffectComponent::ReleaseEntry(UAbilitySystemComponent* ASC, FEntry& Entry)
{
	RemoveSlot(ASC, Entry.DangerEffect);
	RemoveSlot(ASC, Entry.ZoneEffect);
}

void UASZoneEffectComponent::RemoveEntry(UAbilitySystemComponent* ASC)
{
	FEntry Entry;
	if (Entries.RemoveAndCopyValue(ASC, Entry))
	{
		ReleaseEntry(ASC, Entry);
	}
}

void UASZoneEffectComponent::RemoveAllEffects()
{
	// 제거 알림이 발생할 때는 이미 추적 목록이 비어 있어야 한다.
	auto OldEntries = MoveTemp(Entries);
	Entries.Reset();
	for (auto& Pair : OldEntries)
	{
		UAbilitySystemComponent* ASC = Pair.Key.Get();
		ReleaseEntry(ASC, Pair.Value);
	}
}

void UASZoneEffectComponent::HandleMapStateUpdated()
{
	UpdateEffects();
}

void UASZoneEffectComponent::HandleMatchReset()
{
	if (!bRunning || bUpdating)
	{
		return;
	}
	TGuardValue<bool> UpdatingGuard(bUpdating, true);
	RemoveAllEffects();
}

int32 UASZoneEffectComponent::GetAppliedDangerLevel(const AActor* Actor) const
{
	UAbilitySystemComponent* ASC = ASMapAbilityUtils::FindAbilitySystemComponent(Actor);
	const FEntry* Entry = IsValid(ASC) ? Entries.Find(ASC) : nullptr;
	const APawn* QueriedPawn = Cast<APawn>(Actor);
	return Entry && Entry->Pawn.IsValid() && ASC->GetAvatarActor() == Entry->Pawn.Get()
		&& (!QueriedPawn || QueriedPawn == Entry->Pawn.Get())
		&& ASC->GetActiveGameplayEffect(Entry->DangerEffect.Handle) ? Entry->DangerEffect.DangerLevel : 0;
}

FName UASZoneEffectComponent::GetAppliedZoneEffectZone(const AActor* Actor) const
{
	UAbilitySystemComponent* ASC = ASMapAbilityUtils::FindAbilitySystemComponent(Actor);
	const FEntry* Entry = IsValid(ASC) ? Entries.Find(ASC) : nullptr;
	const APawn* QueriedPawn = Cast<APawn>(Actor);
	return Entry && Entry->Pawn.IsValid() && ASC->GetAvatarActor() == Entry->Pawn.Get()
		&& (!QueriedPawn || QueriedPawn == Entry->Pawn.Get())
		&& ASC->GetActiveGameplayEffect(Entry->ZoneEffect.Handle) ? Entry->ZoneEffect.ZoneId : NAME_None;
}

void UASZoneEffectComponent::DumpState() const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}
	UE_LOG(LogApocalypseSurvival, Log, TEXT("AS.Zone: %d tracked ASC(s)."), Entries.Num());
	for (const auto& Pair : Entries)
	{
		const FEntry& Entry = Pair.Value;
		const APawn* Pawn = Entry.Pawn.Get();
		const UAbilitySystemComponent* ASC = Pair.Key.Get();
		UE_LOG(LogApocalypseSurvival, Log, TEXT("  Pawn=%s Team=%u Base(DangerLevel=%d Handle=%s Valid=%s Active=%s) Zone(ZoneId=%s Handle=%s Valid=%s Active=%s)"),
			*GetNameSafe(Pawn), ASZoneEffectComponentPrivate::GetTeamId(Pawn), Entry.DangerEffect.DangerLevel,
			*Entry.DangerEffect.Handle.ToString(), Entry.DangerEffect.Handle.IsValid() ? TEXT("true") : TEXT("false"),
			ASC && ASC->GetActiveGameplayEffect(Entry.DangerEffect.Handle) ? TEXT("true") : TEXT("false"),
			*Entry.ZoneEffect.ZoneId.ToString(), *Entry.ZoneEffect.Handle.ToString(),
			Entry.ZoneEffect.Handle.IsValid() ? TEXT("true") : TEXT("false"),
			ASC && ASC->GetActiveGameplayEffect(Entry.ZoneEffect.Handle) ? TEXT("true") : TEXT("false"));
	}
}
