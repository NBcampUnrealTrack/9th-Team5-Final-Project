#include "GAS/Characters/Enemies/Abilities/ASMonsterPatternAbility.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "GameFramework/Character.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"

UASMonsterPatternAbility::UASMonsterPatternAbility()
{
	bCanHitPlayers = true;
	bAimAtCursor = false;
	bFaceAimDirection = true;
	ImpactPreset = EASImpactPreset::Standard;
	FGameplayTagContainer AssetTagsToSet = GetAssetTags();
	AssetTagsToSet.AddTag(ASGameplayTags::Ability_Monster_Attack);
	SetAssetTags(AssetTagsToSet);
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Monster_Attacking);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Monster_HitReact);
}

void UASMonsterPatternAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!HasSkillAuthority() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BeginPatternWindup();
	if (WindupDuration <= 0.0f)
	{
		BeginPatternEffect();
		return;
	}

	ACharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character) || !Character->GetWorld())
	{
		FinishPattern(true);
		return;
	}

	Character->GetWorldTimerManager().SetTimer(WindupTimerHandle, this,
		&UASMonsterPatternAbility::BeginPatternEffect, WindupDuration, false);
}

void UASMonsterPatternAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = GetOwnerCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(WindupTimerHandle);
	}
	EndPattern(bWasCancelled);
	CleanupPresentation();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASMonsterPatternAbility::BeginPatternWindup()
{
}

bool UASMonsterPatternAbility::ExecutePatternEffect()
{
	OnSkillEffect();
	return true;
}

void UASMonsterPatternAbility::EndPattern(bool bWasCancelled)
{
}

void UASMonsterPatternAbility::StartWindupPresentation()
{
}

void UASMonsterPatternAbility::StartActionPresentation()
{
}

void UASMonsterPatternAbility::StartRecoveryPresentation()
{
}

bool UASMonsterPatternAbility::PlayPresentationMontage(const FName StartSectionName)
{
	return PlayPresentationMontage(AnimMontage, MontagePlayRate, StartSectionName);
}

bool UASMonsterPatternAbility::PlayPresentationMontage(
	const FASMonsterPresentationMontage& PresentationMontage)
{
	return PlayPresentationMontage(PresentationMontage.Montage.Get(), PresentationMontage.PlayRate,
		PresentationMontage.StartSection);
}

bool UASMonsterPatternAbility::PlayPresentationMontage(UAnimMontage* Montage, const float PlayRate,
	const FName StartSectionName)
{
	if (!HasSkillAuthority() || !Montage)
	{
		return false;
	}

	if (!StartSectionName.IsNone() && Montage->GetSectionIndex(StartSectionName) == INDEX_NONE)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return false;
	}

	const float MontageDuration = ASC->PlayMontage(
		this, GetCurrentActivationInfo(), Montage, FMath::Max(0.01f, PlayRate), StartSectionName);
	if (MontageDuration <= 0.0f)
	{
		return false;
	}

	bPresentationMontageStarted = true;
	ActivePresentationMontage = Montage;
	return true;
}

bool UASMonsterPatternAbility::JumpPresentationMontageSection(const FName SectionName)
{
	UAnimMontage* Montage = ActivePresentationMontage.Get();
	if (!bPresentationMontageStarted || SectionName.IsNone() || !Montage
		|| Montage->GetSectionIndex(SectionName) == INDEX_NONE)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->IsAnimatingAbility(this) || ASC->GetCurrentMontage() != Montage)
	{
		return false;
	}

	ASC->CurrentMontageJumpToSection(SectionName);
	return true;
}

void UASMonsterPatternAbility::StopPresentationMontage(const float OverrideBlendOutTime)
{
	if (!bPresentationMontageStarted)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		ASC && ASC->IsAnimatingAbility(this))
	{
		ASC->CurrentMontageStop(OverrideBlendOutTime);
	}

	bPresentationMontageStarted = false;
	ActivePresentationMontage.Reset();
}

void UASMonsterPatternAbility::ExecutePresentationCue(const FGameplayTag& CueTag,
	const FGameplayCueParameters& CueParameters)
{
	if (!HasSkillAuthority() || !CueTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters ResolvedParameters = CueParameters;
		InitializePresentationCueParameters(ResolvedParameters);
		ASC->ExecuteGameplayCue(CueTag, ResolvedParameters);
	}
}

void UASMonsterPatternAbility::AddPresentationCue(const FGameplayTag& CueTag,
	const FGameplayCueParameters& CueParameters)
{
	if (!HasSkillAuthority() || !CueTag.IsValid() || ActivePresentationCueTags.Contains(CueTag))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters ResolvedParameters = CueParameters;
		InitializePresentationCueParameters(ResolvedParameters);
		ASC->AddGameplayCue(CueTag, ResolvedParameters);
		ActivePresentationCueTags.Add(CueTag);
	}
}

void UASMonsterPatternAbility::RemovePresentationCue(const FGameplayTag& CueTag)
{
	if (!CueTag.IsValid() || ActivePresentationCueTags.Remove(CueTag) == 0)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveGameplayCue(CueTag);
	}
}

void UASMonsterPatternAbility::CleanupPresentation()
{
	StopPresentationMontage();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		for (const FGameplayTag& CueTag : ActivePresentationCueTags)
		{
			ASC->RemoveGameplayCue(CueTag);
		}
	}

	ActivePresentationCueTags.Reset();
}

void UASMonsterPatternAbility::InitializePresentationCueParameters(
	FGameplayCueParameters& CueParameters) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!CueParameters.Instigator.IsValid())
	{
		CueParameters.Instigator = Avatar;
	}
	if (!CueParameters.EffectCauser.IsValid())
	{
		CueParameters.EffectCauser = Avatar;
	}
	if (!CueParameters.SourceObject.IsValid())
	{
		CueParameters.SourceObject = this;
	}
	CueParameters.AbilityLevel = GetAbilityLevel();
}

void UASMonsterPatternAbility::FinishPattern(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UASMonsterPatternAbility::BeginPatternEffect()
{
	if (IsActive() && ExecutePatternEffect())
	{
		FinishPattern(false);
	}
}
