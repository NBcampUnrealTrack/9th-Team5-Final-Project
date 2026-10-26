#include "Core/Testing/ASCombatImpactTestLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Combat/ASCombatImpactSettings.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/World.h"
#include "GAS/ASAbilityTags.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackExecution.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Subsystems/ASMassMonsterSubsystem.h"
#include "MassEntitySubsystem.h"

namespace ASCombatImpactTestPrivate
{
	struct FHitStopRecord
	{
		int32 DispatchCount = 0;
		float LastDuration = 0.0f;
	};

#if !UE_BUILD_SHIPPING
	TMap<TWeakObjectPtr<const AActor>, FHitStopRecord>& GetHitStopRecords()
	{
		static TMap<TWeakObjectPtr<const AActor>, FHitStopRecord> Records;
		return Records;
	}
#endif

	FMassEntityManager* GetEntityManager(const UObject* WorldContextObject)
	{
		UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
	}

	UASMassMonsterSubsystem* GetMassMonsterSubsystem(const UObject* WorldContextObject)
	{
		UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UASMassMonsterSubsystem>() : nullptr;
	}

	/** 실제 판정 경로를 바꾸지 않고 태그 부여·해제 시각만 기록한다. 지속 시간은 해제 프레임의 DeltaTime 이내로 측정된다. */
	struct FTagTrack
	{
		int32 AddCount = 0;
		double FirstAddTime = -1.0;
		double LastRemoveTime = -1.0;
		float RemoveFrameDelta = 0.0f;
		bool bActive = false;

		double GetMeasuredDuration() const
		{
			return FirstAddTime >= 0.0 && LastRemoveTime >= FirstAddTime ? LastRemoveTime - FirstAddTime : -1.0;
		}
	};

	struct FPlayerImpactRecord
	{
		TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
		TArray<TPair<FGameplayTag, FDelegateHandle>> TagHandles;
		FTagTrack Staggered;
		FTagTrack KnockedDown;
		FTagTrack Protected;
		float HealthStart = 0.0f;
		bool bSuperArmorApplied = false;
		TWeakObjectPtr<UClass> AbilityClass;
		FString StartError;
	};

	TMap<TWeakObjectPtr<const APawn>, FPlayerImpactRecord>& GetPlayerImpactRecords()
	{
		static TMap<TWeakObjectPtr<const APawn>, FPlayerImpactRecord> Records;
		return Records;
	}

	FTagTrack* FindTagTrack(FPlayerImpactRecord& Record, const FGameplayTag& Tag)
	{
		if (Tag == ASGameplayTags::State_Staggered) return &Record.Staggered;
		if (Tag == ASGameplayTags::State_KnockedDown) return &Record.KnockedDown;
		if (Tag == ASGameplayTags::State_StaggerProtected) return &Record.Protected;
		return nullptr;
	}

	void StopPlayerImpactRecording(FPlayerImpactRecord& Record)
	{
		if (UAbilitySystemComponent* ASC = Record.AbilitySystem.Get())
		{
			for (const TPair<FGameplayTag, FDelegateHandle>& TagHandle : Record.TagHandles)
			{
				ASC->RegisterGameplayTagEvent(TagHandle.Key, EGameplayTagEventType::NewOrRemoved).Remove(TagHandle.Value);
			}
		}
		Record.TagHandles.Reset();
	}

	FString EnumToString(const EASImpactPreset Value)
	{
		return StaticEnum<EASImpactPreset>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	FString EnumToString(const EASResolvedHitReaction Value)
	{
		return StaticEnum<EASResolvedHitReaction>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	FString EnumToString(const EASStaggerResistance Value)
	{
		return StaticEnum<EASStaggerResistance>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	bool IsControlReaction(const EASResolvedHitReaction Reaction)
	{
		return Reaction == EASResolvedHitReaction::WeakStagger
			|| Reaction == EASResolvedHitReaction::StrongStagger
			|| Reaction == EASResolvedHitReaction::Knockdown;
	}

	// 기대값은 판정 함수(Resolve*/Get*Duration)를 거치지 않고 설정 필드를 직접 읽는다.
	float GetExpectedMonsterDuration(const UASCombatImpactSettings& Settings, const EASResolvedHitReaction Reaction)
	{
		switch (Reaction)
		{
		case EASResolvedHitReaction::WeakStagger: return Settings.WeakStaggerDuration;
		case EASResolvedHitReaction::StrongStagger: return Settings.StrongStaggerDuration;
		case EASResolvedHitReaction::Knockdown: return Settings.KnockdownDuration;
		default: return 0.0f;
		}
	}

	float GetExpectedMeleeHitStop(const UASCombatImpactSettings& Settings, const EASImpactPreset Preset)
	{
		switch (Preset)
		{
		case EASImpactPreset::Standard: return Settings.MeleeStandardHitStop;
		case EASImpactPreset::Heavy: return Settings.MeleeHeavyHitStop;
		case EASImpactPreset::Knockdown: return Settings.MeleeKnockdownHitStop;
		default: return 0.0f;
		}
	}

	/** 측정값이 [Expected, Expected + 해제 프레임 DeltaTime] 범위인지 본다. 타이머는 만료된 프레임에 실행된다. */
	bool IsDurationWithinFrame(const FTagTrack& Track, const float Expected)
	{
		constexpr double Epsilon = 0.005;
		const double Measured = Track.GetMeasuredDuration();
		return Measured >= Expected - Epsilon && Measured <= Expected + Track.RemoveFrameDelta + Epsilon;
	}

	/** UASSkillAbility::ImpactPreset은 protected라 리플렉션으로 CDO 값만 읽는다. */
	bool GetAbilityImpactPreset(const UClass* AbilityClass, EASImpactPreset& OutPreset)
	{
		const FEnumProperty* Property = AbilityClass
			? FindFProperty<FEnumProperty>(AbilityClass, TEXT("ImpactPreset"))
			: nullptr;
		if (!Property)
		{
			return false;
		}

		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(AbilityClass->GetDefaultObject());
		OutPreset = static_cast<EASImpactPreset>(Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr));
		return true;
	}

	EASImpactPreset GetWeaponImpactPreset(const UASItemDefinition* WeaponDefinition)
	{
		return WeaponDefinition && WeaponDefinition->AttackProfile
			? WeaponDefinition->AttackProfile->ImpactPreset
			: EASImpactPreset::None;
	}

	void PlaceMonster(AASMonsterCharacter* Monster, const FVector& Location, const FVector& LookAt)
	{
		if (!IsValid(Monster))
		{
			return;
		}

		UASCombatImpactTestLibrary::StopMonsterAI(Monster);
		const float Yaw = (LookAt - Location).GetSafeNormal2D().Rotation().Yaw;
		Monster->SetActorLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f), false, nullptr,
			ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	void Report(const UObject* WorldContextObject, const bool bPass, const FString& CaseName, const FString& Details)
	{
		const FString Message = FString::Printf(TEXT("[ImpactTest] %s %s: %s"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), *CaseName, *Details);
		UKismetSystemLibrary::PrintString(WorldContextObject, Message, true, true,
			bPass ? FLinearColor(0.2f, 1.0f, 0.2f) : FLinearColor(1.0f, 0.2f, 0.2f), 15.0f);
	}

	/** 판정 대상 플레이어의 정면 방향. 모든 근접 케이스는 이 방향으로 ExecuteHit를 실행한다. */
	const FVector TestAimDirection = FVector::ForwardVector;
}

#if !UE_BUILD_SHIPPING
void FASCombatImpactTestDiagnostics::RecordMeleeHitStopDispatch(const AActor* Character, const float Duration)
{
	if (!Character)
	{
		return;
	}

	ASCombatImpactTestPrivate::FHitStopRecord& Record = ASCombatImpactTestPrivate::GetHitStopRecords().FindOrAdd(Character);
	++Record.DispatchCount;
	Record.LastDuration = Duration;
}
#endif

bool UASCombatImpactTestLibrary::RunMeleeExecuteHit(APawn* Player, UASItemDefinition* WeaponDefinition,
	const FVector AimDirection, FString& OutError)
{
	OutError.Reset();
	if (!IsValid(Player) || !Player->HasAuthority() || !WeaponDefinition)
	{
		OutError = TEXT("Player must be a valid authority pawn and WeaponDefinition must be set.");
		return false;
	}

	const AASPlayerController* Controller = Cast<AASPlayerController>(Player->GetController());
	UASInventoryComponent* Inventory = Controller ? Controller->GetInventoryComponent() : nullptr;
	UASItemInstance* Weapon = Inventory ? Inventory->FindFirstItem(WeaponDefinition) : nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
	const UASWeaponAttackProfile* Profile = WeaponDefinition->AttackProfile;
	UClass* ExecutionClass = Profile ? Profile->ExecutionClass.Get() : nullptr;
	const UASWeaponAttackExecution* Execution = ExecutionClass
		? ExecutionClass->GetDefaultObject<UASWeaponAttackExecution>()
		: nullptr;
	if (!Inventory || !Weapon || !ASC || !Execution)
	{
		OutError = FString::Printf(TEXT("Context failed: Inventory=%d Weapon=%d ASC=%d Execution=%d"),
			Inventory != nullptr, Weapon != nullptr, ASC != nullptr, Execution != nullptr);
		return false;
	}

	// UASGameplayAbility_CharacterPrimaryAttack::BuildExecutionContext와 같은 구성으로 실제 ExecuteHit만 호출한다.
	FASWeaponAttackExecutionContext Context;
	Context.Avatar = Player;
	Context.AbilitySystem = ASC;
	Context.Inventory = Inventory;
	Context.Weapon = Weapon;
	Context.Definition = WeaponDefinition;
	Context.Profile = Profile;
	Context.AimDirection = AimDirection;
	Execution->ExecuteHit(Context);
	return true;
}

void UASCombatImpactTestLibrary::ResetMeleeHitStopDiagnostics(AActor* Character)
{
#if !UE_BUILD_SHIPPING
	ASCombatImpactTestPrivate::GetHitStopRecords().Remove(Character);
#endif
}

void UASCombatImpactTestLibrary::GetMeleeHitStopDiagnostics(AActor* Character, int32& OutDispatchCount, float& OutLastDuration)
{
	OutDispatchCount = 0;
	OutLastDuration = 0.0f;
#if !UE_BUILD_SHIPPING
	if (const ASCombatImpactTestPrivate::FHitStopRecord* Record = ASCombatImpactTestPrivate::GetHitStopRecords().Find(Character))
	{
		OutDispatchCount = Record->DispatchCount;
		OutLastDuration = Record->LastDuration;
	}
#endif
}

bool UASCombatImpactTestLibrary::GetMonsterHitReactState(AASMonsterCharacter* Monster, float& OutTotalDuration, float& OutRemaining)
{
	OutTotalDuration = 0.0f;
	OutRemaining = 0.0f;
	const UAbilitySystemComponent* ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		FGameplayTagContainer(ASGameplayTags::State_Monster_HitReact));
	const float WorldTime = ASC->GetWorld() ? ASC->GetWorld()->GetTimeSeconds() : 0.0f;
	bool bFound = false;
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (!Effect)
		{
			continue;
		}

		const float Remaining = Effect->GetTimeRemaining(WorldTime);
		if (!bFound || Remaining > OutRemaining)
		{
			OutTotalDuration = Effect->GetDuration();
			OutRemaining = Remaining;
			bFound = true;
		}
	}
	return bFound;
}

void UASCombatImpactTestLibrary::StopMonsterAI(AASMonsterCharacter* Monster)
{
	if (AASMonsterAIController* Controller = Monster ? Cast<AASMonsterAIController>(Monster->GetController()) : nullptr)
	{
		Controller->StopAI(TEXT("CombatImpactTest"));
	}
}

bool UASCombatImpactTestLibrary::ActivateMonsterAbility(AASMonsterCharacter* Monster,
	const TSubclassOf<UGameplayAbility> AbilityClass, AActor* Target)
{
	UAbilitySystemComponent* ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !AbilityClass || !Monster->HasAuthority())
	{
		return false;
	}

	if (UASMonsterTargetingComponent* Targeting = Monster->GetTargetingComponent())
	{
		Targeting->SetTargetActor(Target);
	}
	return ASC->TryActivateAbilityByClass(AbilityClass);
}

void UASCombatImpactTestLibrary::RestoreFullHealth(AActor* Target)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	ASC->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(),
		ASC->GetNumericAttribute(UASHealthAttributeSet::GetMaxHealthAttribute()));
}

float UASCombatImpactTestLibrary::GetHealth(AActor* Target)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	return ASC ? ASC->GetNumericAttribute(UASHealthAttributeSet::GetHealthAttribute()) : -1.0f;
}

int64 UASCombatImpactTestLibrary::SpawnMassTestMonster(UObject* WorldContextObject, UMassEntityConfigAsset* EntityConfig,
	const FVector Location, const EASMassMonsterWorldBehavior WorldBehavior)
{
	UASMassMonsterSubsystem* MassMonsters = ASCombatImpactTestPrivate::GetMassMonsterSubsystem(WorldContextObject);
	if (!MassMonsters || !EntityConfig)
	{
		return 0;
	}

	const FTransform SpawnTransform(Location);
	TArray<FMassEntityHandle> Entities;
	MassMonsters->SpawnMonsters(*EntityConfig, MakeArrayView(&SpawnTransform, 1), WorldBehavior, Entities);
	return Entities.Num() > 0 ? static_cast<int64>(Entities[0].AsNumber()) : 0;
}

int32 UASCombatImpactTestLibrary::ApplyMassImpact(UObject* WorldContextObject, const int64 EntityId,
	const EASImpactPreset Preset, const float Damage, AActor* Instigator)
{
	UASMassMonsterSubsystem* MassMonsters = ASCombatImpactTestPrivate::GetMassMonsterSubsystem(WorldContextObject);
	if (!MassMonsters || EntityId == 0)
	{
		return 0;
	}

	FASCombatImpactContext ImpactContext;
	ImpactContext.Preset = Preset;
	return MassMonsters->ApplyDamageToTarget(static_cast<uint64>(EntityId), Damage, Instigator, ImpactContext);
}

bool UASCombatImpactTestLibrary::GetMassHitReaction(UObject* WorldContextObject, const int64 EntityId,
	EASResolvedHitReaction& OutReaction, float& OutRemaining, int32& OutSequence,
	float& OutRapidCooldownRemaining, float& OutHealth)
{
	OutReaction = EASResolvedHitReaction::None;
	OutRemaining = 0.0f;
	OutSequence = 0;
	OutRapidCooldownRemaining = 0.0f;
	OutHealth = 0.0f;

	FMassEntityManager* EntityManager = ASCombatImpactTestPrivate::GetEntityManager(WorldContextObject);
	const FMassEntityHandle Entity = FMassEntityHandle::FromNumber(static_cast<uint64>(EntityId));
	if (!EntityManager || EntityId == 0 || !EntityManager->IsEntityValid(Entity))
	{
		return false;
	}

	const FASMassMonsterHitReactionFragment* HitReaction =
		EntityManager->GetFragmentDataPtr<FASMassMonsterHitReactionFragment>(Entity);
	if (!HitReaction)
	{
		return false;
	}

	const double WorldTime = WorldContextObject->GetWorld()->GetTimeSeconds();
	OutReaction = HitReaction->Reaction;
	OutRemaining = static_cast<float>(FMath::Max(0.0, HitReaction->ReactionEndTimeSeconds - WorldTime));
	OutSequence = HitReaction->ReactionSequence;
	OutRapidCooldownRemaining = static_cast<float>(FMath::Max(0.0, HitReaction->NextRapidReactionTimeSeconds - WorldTime));
	if (const FASMassMonsterHealthFragment* Health = EntityManager->GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity))
	{
		OutHealth = Health->CurrentHealth;
	}
	return true;
}

void UASCombatImpactTestLibrary::DestroyMassTestMonster(UObject* WorldContextObject, const int64 EntityId)
{
	if (UASMassMonsterSubsystem* MassMonsters = ASCombatImpactTestPrivate::GetMassMonsterSubsystem(WorldContextObject))
	{
		MassMonsters->DestroyMonster(FMassEntityHandle::FromNumber(static_cast<uint64>(EntityId)));
	}
}

void UASCombatImpactTestLibrary::StartActorMonsterAttackCase(AASMonsterCharacter* Monster, APawn* Player,
	const TSubclassOf<UGameplayAbility> AbilityClass, const float Distance, const bool bPlayerSuperArmor)
{
	using namespace ASCombatImpactTestPrivate;

	if (!IsValid(Player))
	{
		return;
	}

	FPlayerImpactRecord& Record = GetPlayerImpactRecords().FindOrAdd(Player);
	StopPlayerImpactRecording(Record);
	Record = FPlayerImpactRecord();
	Record.AbilityClass = AbilityClass.Get();

	UAbilitySystemComponent* PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
	if (!IsValid(Monster) || !Monster->HasAuthority() || !PlayerASC || !AbilityClass)
	{
		Record.StartError = TEXT("invalid monster/player ASC/ability class");
		return;
	}

	if (PlayerASC->HasMatchingGameplayTag(ASGameplayTags::State_Staggered)
		|| PlayerASC->HasMatchingGameplayTag(ASGameplayTags::State_StaggerProtected))
	{
		Record.StartError = TEXT("precondition: player already Staggered/StaggerProtected");
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	PlaceMonster(Monster, PlayerLocation + TestAimDirection * Distance, PlayerLocation);
	RestoreFullHealth(Monster);
	if (Monster->IsHitReacting())
	{
		Record.StartError = TEXT("precondition: monster is HitReacting and cannot attack");
		return;
	}

	RestoreFullHealth(Player);
	Record.HealthStart = GetHealth(Player);
	Record.AbilitySystem = PlayerASC;
	const TWeakObjectPtr<const APawn> WeakPlayer(Player);
	const FGameplayTag TrackedTags[] = {
		ASGameplayTags::State_Staggered, ASGameplayTags::State_KnockedDown, ASGameplayTags::State_StaggerProtected };
	for (const FGameplayTag& Tag : TrackedTags)
	{
		const FDelegateHandle Handle = PlayerASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddLambda([WeakPlayer](const FGameplayTag ChangedTag, const int32 NewCount)
			{
				const APawn* RecordedPlayer = WeakPlayer.Get();
				FPlayerImpactRecord* PlayerRecord = RecordedPlayer ? GetPlayerImpactRecords().Find(WeakPlayer) : nullptr;
				FTagTrack* Track = PlayerRecord ? FindTagTrack(*PlayerRecord, ChangedTag) : nullptr;
				if (!Track)
				{
					return;
				}

				const UWorld* World = RecordedPlayer->GetWorld();
				const double Now = World ? World->GetTimeSeconds() : 0.0;
				if (NewCount > 0 && !Track->bActive)
				{
					++Track->AddCount;
					if (Track->FirstAddTime < 0.0)
					{
						Track->FirstAddTime = Now;
					}
					Track->bActive = true;
				}
				else if (NewCount <= 0 && Track->bActive)
				{
					Track->LastRemoveTime = Now;
					Track->RemoveFrameDelta = World ? World->GetDeltaSeconds() : 0.0f;
					Track->bActive = false;
				}
			});
		Record.TagHandles.Emplace(Tag, Handle);
	}

	if (bPlayerSuperArmor)
	{
		UAbilitySystemBlueprintLibrary::AddLooseGameplayTags(Player,
			FGameplayTagContainer(ASGameplayTags::State_SuperArmor), true);
		Record.bSuperArmorApplied = true;
	}

	if (!ActivateMonsterAbility(Monster, AbilityClass, Player))
	{
		Record.StartError = FString::Printf(TEXT("TryActivateAbilityByClass(%s) failed"), *GetNameSafe(AbilityClass.Get()));
	}
}

bool UASCombatImpactTestLibrary::FinishActorMonsterAttackCase(const FString& CaseName, AASMonsterCharacter* Monster,
	APawn* Player, const EASImpactPreset ExpectedPreset, const bool bExpectStagger, const bool bExpectKnockdown)
{
	using namespace ASCombatImpactTestPrivate;

	FPlayerImpactRecord Record;
	if (FPlayerImpactRecord* Found = IsValid(Player) ? GetPlayerImpactRecords().Find(Player) : nullptr)
	{
		StopPlayerImpactRecording(*Found);
		Record = MoveTemp(*Found);
		GetPlayerImpactRecords().Remove(Player);
	}
	else
	{
		Record.StartError = TEXT("StartActorMonsterAttackCase was not called for this player");
	}

	if (Record.bSuperArmorApplied && IsValid(Player))
	{
		UAbilitySystemBlueprintLibrary::RemoveLooseGameplayTags(Player,
			FGameplayTagContainer(ASGameplayTags::State_SuperArmor), true);
	}

	const UASCombatImpactSettings* Settings = GetDefault<UASCombatImpactSettings>();
	EASImpactPreset AbilityPreset = EASImpactPreset::None;
	const bool bHasPreset = GetAbilityImpactPreset(Record.AbilityClass.Get(), AbilityPreset);
	const float Damage = Record.HealthStart - GetHealth(Player);
	const float ExpectedStaggerDuration = bExpectKnockdown ? Settings->PlayerKnockdownDuration : Settings->PlayerStaggerDuration;

	bool bPass = Record.StartError.IsEmpty() && bHasPreset && AbilityPreset == ExpectedPreset && Damage > 0.0f;
	if (bExpectStagger)
	{
		bPass &= Record.Staggered.AddCount == 1 && !Record.Staggered.bActive
			&& Record.KnockedDown.AddCount == (bExpectKnockdown ? 1 : 0) && !Record.KnockedDown.bActive
			&& Record.Protected.AddCount == 1 && !Record.Protected.bActive
			&& IsDurationWithinFrame(Record.Staggered, ExpectedStaggerDuration)
			&& IsDurationWithinFrame(Record.Protected, Settings->PlayerProtectionDuration);
	}
	else
	{
		bPass &= Record.Staggered.AddCount == 0 && Record.KnockedDown.AddCount == 0 && Record.Protected.AddCount == 0;
	}

	const FString ErrorText = Record.StartError.IsEmpty() ? FString() : FString::Printf(TEXT(" error=\"%s\""), *Record.StartError);
	const FString Details = FString::Printf(
		TEXT("target=%s source=%s ability=%s preset=%s superArmor=%d | measured: damage=%.2f Staggered{count=%d dur=%.3f} ")
		TEXT("KnockedDown{count=%d dur=%.3f} StaggerProtected{count=%d dur=%.3f} frameDt=%.3f%s | expected: preset=%s damage>0 ")
		TEXT("Staggered{count=%d dur=%.3f} KnockedDown{count=%d} StaggerProtected{count=%d dur=%.3f}"),
		*GetNameSafe(Player), Monster ? *Monster->GetActorNameOrLabel() : TEXT("None"),
		*GetNameSafe(Record.AbilityClass.Get()), bHasPreset ? *EnumToString(AbilityPreset) : TEXT("<missing>"),
		Record.bSuperArmorApplied ? 1 : 0, Damage,
		Record.Staggered.AddCount, Record.Staggered.GetMeasuredDuration(),
		Record.KnockedDown.AddCount, Record.KnockedDown.GetMeasuredDuration(),
		Record.Protected.AddCount, Record.Protected.GetMeasuredDuration(), Record.Staggered.RemoveFrameDelta,
		*ErrorText, *EnumToString(ExpectedPreset),
		bExpectStagger ? 1 : 0, bExpectStagger ? ExpectedStaggerDuration : 0.0f,
		bExpectKnockdown ? 1 : 0,
		bExpectStagger ? 1 : 0, bExpectStagger ? Settings->PlayerProtectionDuration : 0.0f);
	Report(Player, bPass, CaseName, Details);
	return bPass;
}

bool UASCombatImpactTestLibrary::RunActorMonsterReactionCase(const FString& CaseName, APawn* Player,
	AASMonsterCharacter* Monster, AASMonsterCharacter* OtherMonster, UASItemDefinition* WeaponDefinition,
	const EASResolvedHitReaction ExpectedReaction)
{
	using namespace ASCombatImpactTestPrivate;

	if (!IsValid(Player) || !IsValid(Monster) || !Monster->GetAbilitySystemComponent())
	{
		Report(Player, false, CaseName, TEXT("invalid player or monster"));
		return false;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	PlaceMonster(Monster, PlayerLocation + TestAimDirection * 130.0f, PlayerLocation);
	PlaceMonster(OtherMonster, PlayerLocation - TestAimDirection * 600.0f, PlayerLocation);
	RestoreFullHealth(Monster);

	const bool bPreconditionHitReact = Monster->IsHitReacting();
	const float HealthBefore = GetHealth(Monster);
	ResetMeleeHitStopDiagnostics(Player);
	FString Error;
	const bool bExecuted = !bPreconditionHitReact && RunMeleeExecuteHit(Player, WeaponDefinition, TestAimDirection, Error);
	if (bPreconditionHitReact)
	{
		Error = TEXT("precondition: monster already HitReacting");
	}

	const float Damage = HealthBefore - GetHealth(Monster);
	const bool bHitReactTag = Monster->GetAbilitySystemComponent()->HasMatchingGameplayTag(ASGameplayTags::State_Monster_HitReact);
	float GEDuration = 0.0f;
	float GERemaining = 0.0f;
	GetMonsterHitReactState(Monster, GEDuration, GERemaining);
	int32 HitStopCount = 0;
	float HitStopDuration = 0.0f;
	GetMeleeHitStopDiagnostics(Player, HitStopCount, HitStopDuration);

	const UASCombatImpactSettings* Settings = GetDefault<UASCombatImpactSettings>();
	const bool bExpectHitReact = IsControlReaction(ExpectedReaction);
	const float ExpectedDuration = GetExpectedMonsterDuration(*Settings, ExpectedReaction);
	bool bPass = bExecuted && Damage > 0.0f && bHitReactTag == bExpectHitReact;
	if (bExpectHitReact)
	{
		bPass &= FMath::IsNearlyEqual(GEDuration, ExpectedDuration, 0.001f);
	}

	const UASMonsterDefinition* Definition = Monster->GetMonsterDefinition();
	const FString ErrorText = Error.IsEmpty() ? FString() : FString::Printf(TEXT(" error=\"%s\""), *Error);
	const FString Details = FString::Printf(
		TEXT("target=%s resistance=%s weapon=%s preset=%s | measured: damage=%.2f HitReactTag=%d geDuration=%.3f remaining=%.3f ")
		TEXT("hitStopDispatch=%d%s | expected: reaction=%s damage>0 HitReactTag=%d geDuration=%.3f"),
		*Monster->GetActorNameOrLabel(), Definition ? *EnumToString(Definition->StaggerResistance) : TEXT("<none>"),
		*GetNameSafe(WeaponDefinition), *EnumToString(GetWeaponImpactPreset(WeaponDefinition)),
		Damage, bHitReactTag ? 1 : 0, GEDuration, GERemaining, HitStopCount, *ErrorText,
		*EnumToString(ExpectedReaction), bExpectHitReact ? 1 : 0, ExpectedDuration);
	Report(Player, bPass, CaseName, Details);
	return bPass;
}

bool UASCombatImpactTestLibrary::RunMeleeHitStopCase(const FString& CaseName, APawn* Player,
	UASItemDefinition* WeaponDefinition, const TArray<AASMonsterCharacter*>& TargetsInCone,
	const TArray<AASMonsterCharacter*>& OutOfConeMonsters)
{
	using namespace ASCombatImpactTestPrivate;

	if (!IsValid(Player))
	{
		Report(Player, false, CaseName, TEXT("invalid player"));
		return false;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector Lateral = FVector::CrossProduct(FVector::UpVector, TestAimDirection);
	TArray<float> InConeHealthBefore;
	for (int32 Index = 0; Index < TargetsInCone.Num(); ++Index)
	{
		const float LateralOffset = (Index - (TargetsInCone.Num() - 1) * 0.5f) * 90.0f;
		PlaceMonster(TargetsInCone[Index], PlayerLocation + TestAimDirection * 130.0f + Lateral * LateralOffset, PlayerLocation);
		RestoreFullHealth(TargetsInCone[Index]);
		InConeHealthBefore.Add(GetHealth(TargetsInCone[Index]));
	}
	TArray<float> OutOfConeHealthBefore;
	for (int32 Index = 0; Index < OutOfConeMonsters.Num(); ++Index)
	{
		PlaceMonster(OutOfConeMonsters[Index], PlayerLocation - TestAimDirection * 600.0f + Lateral * (Index * 200.0f), PlayerLocation);
		RestoreFullHealth(OutOfConeMonsters[Index]);
		OutOfConeHealthBefore.Add(GetHealth(OutOfConeMonsters[Index]));
	}

	ResetMeleeHitStopDiagnostics(Player);
	FString ExecuteError;
	const bool bExecuted = RunMeleeExecuteHit(Player, WeaponDefinition, TestAimDirection, ExecuteError);
	int32 DispatchCount = 0;
	float DispatchDuration = 0.0f;
	GetMeleeHitStopDiagnostics(Player, DispatchCount, DispatchDuration);

	int32 DamagedInCone = 0;
	for (int32 Index = 0; Index < TargetsInCone.Num(); ++Index)
	{
		DamagedInCone += GetHealth(TargetsInCone[Index]) < InConeHealthBefore[Index] ? 1 : 0;
	}
	int32 DamagedOutOfCone = 0;
	for (int32 Index = 0; Index < OutOfConeMonsters.Num(); ++Index)
	{
		DamagedOutOfCone += GetHealth(OutOfConeMonsters[Index]) < OutOfConeHealthBefore[Index] ? 1 : 0;
	}

	const EASImpactPreset Preset = GetWeaponImpactPreset(WeaponDefinition);
	const bool bExpectHit = TargetsInCone.Num() > 0;
	const int32 ExpectedDispatchCount = bExpectHit ? 1 : 0;
	const float ExpectedDuration = bExpectHit ? GetExpectedMeleeHitStop(*GetDefault<UASCombatImpactSettings>(), Preset) : 0.0f;
	bool bPass = bExecuted && DispatchCount == ExpectedDispatchCount
		&& DamagedInCone == TargetsInCone.Num() && DamagedOutOfCone == 0;
	if (bExpectHit)
	{
		bPass &= FMath::IsNearlyEqual(DispatchDuration, ExpectedDuration, 0.0001f);
	}

	FString TargetNames;
	for (const AASMonsterCharacter* Target : TargetsInCone)
	{
		if (!TargetNames.IsEmpty())
		{
			TargetNames += TEXT(",");
		}
		TargetNames += Target ? Target->GetActorNameOrLabel() : FString(TEXT("None"));
	}
	const FString ErrorText = ExecuteError.IsEmpty() ? FString() : FString::Printf(TEXT(" error=\"%s\""), *ExecuteError);
	const FString Details = FString::Printf(
		TEXT("attacker=%s targets=[%s] weapon=%s preset=%s | measured: MulticastPlayMeleeHitStop{count=%d duration=%.3f} ")
		TEXT("damagedInCone=%d damagedOutOfCone=%d%s | expected: count=%d duration=%.3f damagedInCone=%d damagedOutOfCone=0"),
		*GetNameSafe(Player), *TargetNames, *GetNameSafe(WeaponDefinition), *EnumToString(Preset),
		DispatchCount, DispatchDuration, DamagedInCone, DamagedOutOfCone, *ErrorText,
		ExpectedDispatchCount, ExpectedDuration, TargetsInCone.Num());
	Report(Player, bPass, CaseName, Details);
	return bPass;
}

bool UASCombatImpactTestLibrary::RunMassImpactCase(UObject* WorldContextObject, const FString& CaseName,
	const int64 EntityId, const EASImpactPreset Preset, const EASResolvedHitReaction ExpectedReaction)
{
	using namespace ASCombatImpactTestPrivate;

	FMassEntityManager* EntityManager = GetEntityManager(WorldContextObject);
	const FMassEntityHandle Entity = FMassEntityHandle::FromNumber(static_cast<uint64>(EntityId));
	const bool bValidEntity = EntityManager && EntityId != 0 && EntityManager->IsEntityValid(Entity);
	const FASMassMonsterHitReactionFragment* HitReaction = bValidEntity
		? EntityManager->GetFragmentDataPtr<FASMassMonsterHitReactionFragment>(Entity)
		: nullptr;
	const FASMassMonsterHealthFragment* Health = bValidEntity
		? EntityManager->GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity)
		: nullptr;
	const FASMassMonsterConfigFragment* Config = bValidEntity
		? EntityManager->GetConstSharedFragmentDataPtr<FASMassMonsterConfigFragment>(Entity)
		: nullptr;
	if (!HitReaction || !Health || !Config)
	{
		Report(WorldContextObject, false, CaseName, FString::Printf(TEXT("target=MassEntity(%lld) invalid or missing fragments"), EntityId));
		return false;
	}

	const FASMassMonsterHitReactionFragment Before = *HitReaction;
	const float HealthBefore = Health->CurrentHealth;
	const FName MonsterTypeId = Config->MonsterTypeId;
	const EASStaggerResistance Resistance = Config->StaggerResistance;
	constexpr float TestDamage = 1.0f;
	const int32 HitCount = ApplyMassImpact(WorldContextObject, EntityId, Preset, TestDamage, nullptr);

	// 피해 적용과 같은 프레임에 읽으므로 남은 시간은 적용된 지속 시간과 같다. 포인터는 Entity 구조 변경에 대비해 다시 찾는다.
	const double Now = WorldContextObject->GetWorld()->GetTimeSeconds();
	HitReaction = EntityManager->IsEntityValid(Entity)
		? EntityManager->GetFragmentDataPtr<FASMassMonsterHitReactionFragment>(Entity)
		: nullptr;
	Health = HitReaction ? EntityManager->GetFragmentDataPtr<FASMassMonsterHealthFragment>(Entity) : nullptr;
	if (!HitReaction || !Health)
	{
		Report(WorldContextObject, false, CaseName, FString::Printf(TEXT("target=MassEntity(%lld) lost after impact"), EntityId));
		return false;
	}

	const FASMassMonsterHitReactionFragment& After = *HitReaction;
	const float Remaining = static_cast<float>(FMath::Max(0.0, After.ReactionEndTimeSeconds - Now));
	const float RapidCooldown = static_cast<float>(FMath::Max(0.0, After.NextRapidReactionTimeSeconds - Now));
	const float RapidCooldownBefore = static_cast<float>(FMath::Max(0.0, Before.NextRapidReactionTimeSeconds - Now));
	const float Damage = HealthBefore - Health->CurrentHealth;

	const UASCombatImpactSettings* Settings = GetDefault<UASCombatImpactSettings>();
	const bool bExpectApplied = IsControlReaction(ExpectedReaction);
	const float ExpectedDuration = GetExpectedMonsterDuration(*Settings, ExpectedReaction);
	const bool bExpectRapidCooldown = bExpectApplied && Preset == EASImpactPreset::Rapid;
	bool bPass = HitCount == 1 && FMath::IsNearlyEqual(Damage, TestDamage, 0.001f);
	if (bExpectApplied)
	{
		bPass &= After.ReactionSequence == static_cast<uint8>(Before.ReactionSequence + 1)
			&& After.Reaction == ExpectedReaction
			&& FMath::IsNearlyEqual(Remaining, ExpectedDuration, 0.001f);
		bPass &= bExpectRapidCooldown
			? FMath::IsNearlyEqual(RapidCooldown, Settings->RapidReapplyInterval, 0.001f)
			: After.NextRapidReactionTimeSeconds == Before.NextRapidReactionTimeSeconds;
	}
	else
	{
		bPass &= After.ReactionSequence == Before.ReactionSequence
			&& After.Reaction == Before.Reaction
			&& After.ReactionEndTimeSeconds == Before.ReactionEndTimeSeconds
			&& After.NextRapidReactionTimeSeconds == Before.NextRapidReactionTimeSeconds;
	}

	const FString ExpectedRapidText = bExpectRapidCooldown
		? FString::Printf(TEXT("%.3f"), Settings->RapidReapplyInterval)
		: FString(TEXT("unchanged"));
	const FString Details = FString::Printf(
		TEXT("target=MassEntity(%lld,%s) resistance=%s preset=%s | measured: hits=%d damage=%.2f reaction=%s(before %s) remaining=%.3f ")
		TEXT("sequence=%d->%d rapidCooldown=%.3f(before %.3f) | expected: hits=1 damage=%.2f %s reaction=%s remaining=%.3f rapidCooldown=%s"),
		EntityId, *MonsterTypeId.ToString(), *EnumToString(Resistance), *EnumToString(Preset),
		HitCount, Damage, *EnumToString(After.Reaction), *EnumToString(Before.Reaction), Remaining,
		Before.ReactionSequence, After.ReactionSequence, RapidCooldown, RapidCooldownBefore,
		TestDamage, bExpectApplied ? TEXT("sequence+1") : TEXT("sequence unchanged"),
		bExpectApplied ? *EnumToString(ExpectedReaction) : *EnumToString(Before.Reaction),
		bExpectApplied ? ExpectedDuration : static_cast<float>(FMath::Max(0.0, Before.ReactionEndTimeSeconds - Now)),
		*ExpectedRapidText);
	Report(WorldContextObject, bPass, CaseName, Details);
	return bPass;
}
