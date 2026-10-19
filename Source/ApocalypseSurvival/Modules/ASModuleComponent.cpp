#include "Modules/ASModuleComponent.h"

#include "Core/Framework/ASPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "GameplayEffect.h"
#include "Modules/Data/ASModuleSettings.h"
#include "Modules/GAS/ASModulePassiveAbility.h"
#include "Modules/GAS/ASModuleStatEffect.h"
#include "Modules/Persistence/ASModuleCollectionSubsystem.h"
#include "Net/UnrealNetwork.h"

UASModuleComponent::UASModuleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASModuleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UASModuleComponent, EquippedModules, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UASModuleComponent, bEffectsApplied, COND_OwnerOnly);
}

UASAbilitySystemComponent* UASModuleComponent::GetModuleASC() const
{
	const AASPlayerState* PlayerState = Cast<AASPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetASAbilitySystemComponent() : nullptr;
}

bool UASModuleComponent::ConfigureLoadout(const TArray<FASModuleInstanceData>& Instances, FString& OutError)
{
	OutError.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutError = TEXT("Only the server can configure a match module loadout.");
		return false;
	}
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	if (!Settings->ResolveInstanceLoadout(Instances, OutError)) return false;
	TArray<FASModuleInstanceData> Normalized = Instances;
	Normalized.SetNum(Settings->GetSlotCount());
	if (bEffectsApplied && Normalized != EquippedModules)
	{
		OutError = TEXT("The current run's modules are locked. ResetForNextRun before changing them.");
		return false;
	}
	EquippedModules = MoveTemp(Normalized);
	bLoadoutConfigured = true;
	GetOwner()->ForceNetUpdate();
	OnLoadoutChanged.Broadcast();
	return true;
}

bool UASModuleComponent::ApplyConfiguredEffects(FString& OutError)
{
	UASAbilitySystemComponent* ASC = GetModuleASC();
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	if (!ASC || !Settings->ResolveInstanceLoadout(EquippedModules, OutError)) return false;
	// Stage handles so any failed grant rolls the entire loadout back.
	TArray<FASAppliedModule> Pending;
	bool bSuccess = true;
	for (const FASModuleInstanceData& Instance : EquippedModules)
	{
		if (Instance.IsEmpty()) continue;
		const FASModuleBalanceRow* Module = Settings->FindModule(Instance.ModuleId, OutError);
		if (!Module) { bSuccess = false; break; }
		FASAppliedModule& Entry = Pending.AddDefaulted_GetRef();
		Entry.ModuleId = Instance.ModuleId;
		Entry.CoreValue = Module->CoreValue;
		const auto ApplyEffect = [&](FGameplayEffectSpecHandle Spec) -> bool
		{
			if (!Spec.IsValid()) return false;
			const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			Entry.Handles.AddGameplayEffectHandle(Handle);
			return Handle.IsValid();
		};
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const auto ApplyStat = [&](EASModuleStat Stat, float Value) -> bool
		{
			if (Stat == EASModuleStat::None) return true;
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UASModuleStatEffect::StaticClass(), 1.0f, Context);
			if (!Spec.IsValid()) return false;
			UASModuleStatEffect::SetMagnitude(*Spec.Data, Stat, Value);
			return ApplyEffect(Spec);
		};
		if (!ApplyStat(Module->CoreStat, Entry.CoreValue)) { bSuccess = false; break; }
		for (const FASModuleOptionRoll& Roll : Instance.Options)
		{
			EASModuleStat Stat;
			if (!Settings->ResolveOptionStat(Roll, Stat, OutError) || !ApplyStat(Stat, Roll.Value)) { bSuccess = false; break; }
		}
		if (!bSuccess) break;
		if (const UASAbilitySet* Set = Module->AbilitySet.LoadSynchronous())
		{
			for (const FASAbilitySet_GameplayEffect& Effect : Set->GetGrantedEffects())
			{
				FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Effect.GameplayEffect, Effect.EffectLevel, Context);
				if (Spec.IsValid()) Spec.Data->SetSetByCallerMagnitude(UASModuleStatEffect::GetCoreMagnitudeTag(), Entry.CoreValue);
				if (!ApplyEffect(Spec)) { bSuccess = false; break; }
			}
			if (!bSuccess) break;
			for (const FASAbilitySet_GameplayAbility& Ability : Set->GetGrantedAbilities())
			{
				FGameplayAbilitySpec Spec(Ability.Ability, Ability.AbilityLevel, INDEX_NONE, this);
				if (Ability.InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Ability.InputTag);
				const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
				Entry.Handles.AddAbilitySpecHandle(Handle);
				if (!Handle.IsValid()) { bSuccess = false; break; }
			}
		}
		if (!bSuccess) break;
	}
	if (!bSuccess)
	{
		for (FASAppliedModule& Entry : Pending) Entry.Handles.TakeFromAbilitySystem(ASC);
		if (OutError.IsEmpty()) OutError = TEXT("A module effect or ability could not be granted; the loadout was rolled back.");
		return false;
	}
	AppliedModules = MoveTemp(Pending);
	bEffectsApplied = true;
	GetOwner()->ForceNetUpdate();
	OnLoadoutChanged.Broadcast();
	return true;
}

bool UASModuleComponent::GetModuleCoreValue(FName ModuleId, float& OutValue) const
{
	OutValue = 0.0f;
	for (const FASAppliedModule& Entry : AppliedModules)
	{
		if (Entry.ModuleId == ModuleId) { OutValue = Entry.CoreValue; return true; }
	}
	return false;
}

bool UASModuleComponent::GetAbilityCoreValue(FGameplayAbilitySpecHandle AbilityHandle, float& OutValue) const
{
	OutValue = 0.0f;
	for (const FASAppliedModule& Entry : AppliedModules)
	{
		if (Entry.Handles.GetAbilitySpecHandles().Contains(AbilityHandle)) { OutValue = Entry.CoreValue; return true; }
	}
	return false;
}
void UASModuleComponent::InitializeForAvatar()
{
	UASAbilitySystemComponent* ASC = GetModuleASC();
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || !IsValid(ASC->GetAvatarActor())) return;
	if (bEffectsApplied && InitializedAvatar == ASC->GetAvatarActor()) return;
	FString Error;
	if (!bLoadoutConfigured)
	{
		// Standalone/listen-host convenience. Remote clients need an authoritative lobby handoff.
		const APlayerController* PC = Cast<APlayerController>(GetOwner()->GetOwner());
		const UASModuleCollectionSubsystem* Collection = UASModuleCollectionSubsystem::Get(PC);
		const TArray<FASModuleInstanceData> Selection = Collection ? Collection->GetEquippedModules() : TArray<FASModuleInstanceData>();
		if (!ConfigureLoadout(Selection, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Modules] %s"), *Error);
			return;
		}
	}
	if (!bEffectsApplied && !ApplyConfiguredEffects(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Modules] %s"), *Error);
		return;
	}
	UpdatePassives(false);
	// Initial/respawn resources use the final maxima, including module bonuses.
	RestoreResourceBounds(true);
	InitializedAvatar = ASC->GetAvatarActor();
	UpdatePassives(true);
}

void UASModuleComponent::UpdatePassives(bool bActivate)
{
	UASAbilitySystemComponent* ASC = GetModuleASC();
	if (!ASC) return;
	TArray<FGameplayAbilitySpecHandle> Handles;
	for (const FASAppliedModule& Entry : AppliedModules)
	{
		for (const FGameplayAbilitySpecHandle& Handle : Entry.Handles.GetAbilitySpecHandles())
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			if (Spec && Spec->Ability && Spec->Ability->IsA<UASModulePassiveAbility>()) Handles.Add(Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		if (bActivate) ASC->TryActivateAbility(Handle);
		else ASC->CancelAbilityHandle(Handle);
	}
}
void UASModuleComponent::RestoreResourceBounds(bool bFillResources)
{
	UASAbilitySystemComponent* ASC = GetModuleASC();
	if (!ASC) return;
	const auto UpdateResource = [ASC, bFillResources](const FGameplayAttribute& Current, const FGameplayAttribute& Maximum)
	{
		if (!ASC->HasAttributeSetForAttribute(Current) || !ASC->HasAttributeSetForAttribute(Maximum)) return;
		const float MaxValue = FMath::Max(0.0f, ASC->GetNumericAttribute(Maximum));
		const float Value = bFillResources ? MaxValue : FMath::Clamp(ASC->GetNumericAttribute(Current), 0.0f, MaxValue);
		ASC->SetNumericAttributeBase(Current, Value);
	};
	UpdateResource(UASHealthAttributeSet::GetHealthAttribute(), UASHealthAttributeSet::GetMaxHealthAttribute());
	UpdateResource(UASPlayerAttributeSet::GetStaminaAttribute(), UASPlayerAttributeSet::GetMaxStaminaAttribute());
	UpdateResource(UASPlayerAttributeSet::GetManaAttribute(), UASPlayerAttributeSet::GetMaxManaAttribute());
}

void UASModuleComponent::SuspendForAvatar(AActor* Avatar)
{
	if (GetOwner() && GetOwner()->HasAuthority() && InitializedAvatar.Get() == Avatar)
	{
		UpdatePassives(false);
		InitializedAvatar.Reset();
	}
}

void UASModuleComponent::RemoveModuleEffects()
{
	UASAbilitySystemComponent* ASC = GetModuleASC();
	if (!ASC || !ASC->IsOwnerActorAuthoritative()) return;
	for (FASAppliedModule& Entry : AppliedModules) Entry.Handles.TakeFromAbilitySystem(ASC);
	AppliedModules.Reset();
	bEffectsApplied = false;
	InitializedAvatar.Reset();
}

void UASModuleComponent::ResetForNextRun()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	RemoveModuleEffects();
	RestoreResourceBounds(false);
	EquippedModules.Reset();
	bLoadoutConfigured = false;
	GetOwner()->ForceNetUpdate();
	OnLoadoutChanged.Broadcast();
}

void UASModuleComponent::OnRep_Loadout()
{
	OnLoadoutChanged.Broadcast();
}

void UASModuleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveModuleEffects();
	Super::EndPlay(EndPlayReason);
}
