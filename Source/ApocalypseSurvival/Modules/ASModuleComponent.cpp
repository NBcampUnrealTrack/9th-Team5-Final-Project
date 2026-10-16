#include "Modules/ASModuleComponent.h"

#include "Core/Framework/ASPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "GameplayEffect.h"
#include "Modules/Data/ASModuleDefinition.h"
#include "Modules/Data/ASModuleSettings.h"
#include "Modules/GAS/ASModulePassiveAbility.h"
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
	DOREPLIFETIME_CONDITION(UASModuleComponent, EquippedModuleIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UASModuleComponent, bEffectsApplied, COND_OwnerOnly);
}

UASAbilitySystemComponent* UASModuleComponent::GetModuleASC() const
{
	const AASPlayerState* PlayerState = Cast<AASPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetASAbilitySystemComponent() : nullptr;
}

bool UASModuleComponent::ConfigureLoadout(const TArray<FName>& ModuleIds, FString& OutError)
{
	OutError.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutError = TEXT("Only the server can configure a match module loadout.");
		return false;
	}
	const UASModuleSettings* Settings = GetDefault<UASModuleSettings>();
	TArray<UASModuleDefinition*> Definitions;
	if (!Settings->ResolveLoadout(ModuleIds, Definitions, OutError)) return false;
	TArray<FName> Normalized = ModuleIds;
	Normalized.SetNum(Settings->GetSlotCount());
	if (bEffectsApplied && Normalized != EquippedModuleIds)
	{
		OutError = TEXT("The current run's modules are locked. ResetForNextRun before changing them.");
		return false;
	}
	EquippedModuleIds = MoveTemp(Normalized);
	bLoadoutConfigured = true;
	GetOwner()->ForceNetUpdate();
	OnLoadoutChanged.Broadcast();
	return true;
}

bool UASModuleComponent::ApplyConfiguredEffects(FString& OutError)
{
	UASAbilitySystemComponent* ASC = GetModuleASC();
	TArray<UASModuleDefinition*> Definitions;
	if (!GetDefault<UASModuleSettings>()->ResolveLoadout(EquippedModuleIds, Definitions, OutError)) return false;
	// Stage handles separately: a rejected effect must not leave a partially applied loadout.
	TArray<FASAppliedModule> Pending;
	bool bSuccess = true;
	for (UASModuleDefinition* Definition : Definitions)
	{
		FASAppliedModule& Entry = Pending.AddDefaulted_GetRef();
		Entry.Definition = Definition;
		for (const FASAbilitySet_GameplayEffect& Effect : Definition->AbilitySet->GetGrantedEffects())
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(Definition);
			const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(
				Effect.GameplayEffect->GetDefaultObject<UGameplayEffect>(), Effect.EffectLevel, Context);
			Entry.Handles.AddGameplayEffectHandle(Handle);
			if (!Handle.IsValid()) { bSuccess = false; break; }
		}
		if (!bSuccess) break;
		for (const FASAbilitySet_GameplayAbility& Ability : Definition->AbilitySet->GetGrantedAbilities())
		{
			FGameplayAbilitySpec Spec(Ability.Ability, Ability.AbilityLevel, INDEX_NONE, Definition);
			if (Ability.InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Ability.InputTag);
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
			Entry.Handles.AddAbilitySpecHandle(Handle);
			if (!Handle.IsValid()) { bSuccess = false; break; }
		}
		if (!bSuccess) break;
	}
	if (!bSuccess)
	{
		for (FASAppliedModule& Entry : Pending) Entry.Handles.TakeFromAbilitySystem(ASC);
		OutError = TEXT("A module effect or ability could not be granted; the loadout was rolled back.");
		return false;
	}
	AppliedModules = MoveTemp(Pending);
	bEffectsApplied = true;
	GetOwner()->ForceNetUpdate();
	OnLoadoutChanged.Broadcast();
	return true;
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
		const TArray<FName> Selection = Collection ? Collection->GetEquippedModuleIds() : TArray<FName>();
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
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->IsA<UASModulePassiveAbility>()) continue;
		if (AppliedModules.ContainsByPredicate([&Spec](const FASAppliedModule& Entry)
			{ return Spec.SourceObject.Get() == Entry.Definition.Get(); }))
		{
			Handles.Add(Spec.Handle);
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
	EquippedModuleIds.Reset();
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
