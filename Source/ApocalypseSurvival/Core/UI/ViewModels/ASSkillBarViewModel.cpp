#include "Core/UI/ViewModels/ASSkillBarViewModel.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/Input/ASInputTags.h"
#include "Core/UI/ViewModels/ASSkillSlotViewModel.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Skills/ASSkillAbility.h"
#include "TimerManager.h"

namespace ASSkillBarViewModelPrivate
{
	constexpr float CooldownUpdateInterval = 0.05f;
	constexpr float RebindInterval = 0.5f;

	//ASC 입력 처리와 같은 기준 (스펙에 붙인 태그 또는 어빌리티 에셋 태그)
	bool DoesSpecMatchInputTag(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag)
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return true;
		}

		return Spec.Ability != nullptr && Spec.Ability->GetAssetTags().HasTagExact(InputTag);
	}
}

void UASSkillBarViewModel::InitializeSlots()
{
	if (QSlot && WSlot && ESlot && RSlot && ShiftSlot)
	{
		return;
	}

	UASSkillSlotViewModel* NewQSlot = NewObject<UASSkillSlotViewModel>(this);
	UASSkillSlotViewModel* NewWSlot = NewObject<UASSkillSlotViewModel>(this);
	UASSkillSlotViewModel* NewESlot = NewObject<UASSkillSlotViewModel>(this);
	UASSkillSlotViewModel* NewRSlot = NewObject<UASSkillSlotViewModel>(this);
	UASSkillSlotViewModel* NewShiftSlot = NewObject<UASSkillSlotViewModel>(this);

	NewQSlot->InitializeSlot(FText::FromString(TEXT("Q")));
	NewWSlot->InitializeSlot(FText::FromString(TEXT("W")));
	NewESlot->InitializeSlot(FText::FromString(TEXT("E")));
	NewRSlot->InitializeSlot(FText::FromString(TEXT("R")));
	NewShiftSlot->InitializeSlot(FText::FromString(TEXT("Shift")));

	UE_MVVM_SET_PROPERTY_VALUE(QSlot, NewQSlot);
	UE_MVVM_SET_PROPERTY_VALUE(WSlot, NewWSlot);
	UE_MVVM_SET_PROPERTY_VALUE(ESlot, NewESlot);
	UE_MVVM_SET_PROPERTY_VALUE(RSlot, NewRSlot);
	UE_MVVM_SET_PROPERTY_VALUE(ShiftSlot, NewShiftSlot);
}

void UASSkillBarViewModel::Initialize(UAbilitySystemComponent* InAbilitySystemComponent)
{
	//리스폰이나 폰 교체로 다시 불려도 안전하도록 이전 구독부터 정리
	Deinitialize();
	InitializeSlots();

	if (!IsValid(InAbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	CachedWorld = InAbilitySystemComponent->GetWorld();

	SlotBindings.SetNum(5);
	SlotBindings[0].InputTag = ASGameplayTags::InputTag_Skill_Q;
	SlotBindings[1].InputTag = ASGameplayTags::InputTag_Skill_W;
	SlotBindings[2].InputTag = ASGameplayTags::InputTag_Skill_E;
	SlotBindings[3].InputTag = ASGameplayTags::InputTag_Skill_R;
	SlotBindings[4].InputTag = ASGameplayTags::InputTag_Dodge;

	RefreshSkillSlots();
}

void UASSkillBarViewModel::InitializeFromActor(AActor* InAbilitySystemOwner)
{
	Initialize(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InAbilitySystemOwner));
}

void UASSkillBarViewModel::Deinitialize()
{
	StopCooldownTimer();

	if (UWorld* World = CachedWorld.Get())
	{
		World->GetTimerManager().ClearTimer(RebindTimerHandle);
	}

	for (int32 SlotIndex = 0; SlotIndex < SlotBindings.Num(); ++SlotIndex)
	{
		UnbindSlot(SlotIndex);
	}

	SlotBindings.Reset();
	AbilitySystemComponent.Reset();
	CachedWorld.Reset();
}

void UASSkillBarViewModel::RefreshSkillSlots()
{
	if (!AbilitySystemComponent.IsValid())
	{
		return;
	}

	bool bAllBound = true;
	for (int32 SlotIndex = 0; SlotIndex < SlotBindings.Num(); ++SlotIndex)
	{
		if (!SlotBindings[SlotIndex].bBound)
		{
			BindSlot(SlotIndex);
		}

		bAllBound &= SlotBindings[SlotIndex].bBound;
	}

	UWorld* World = CachedWorld.Get();
	if (!World)
	{
		return;
	}

	//어빌리티는 서버에서 부여된 뒤 복제되므로 HUD가 먼저 뜨면 스펙이 아직 없을 수 있음
	//빈 슬롯이 있는 동안만 주기적으로 다시 찾음
	FTimerManager& TimerManager = World->GetTimerManager();
	if (bAllBound)
	{
		TimerManager.ClearTimer(RebindTimerHandle);
	}
	else if (!TimerManager.IsTimerActive(RebindTimerHandle))
	{
		TimerManager.SetTimer(RebindTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::RefreshSkillSlots),
			ASSkillBarViewModelPrivate::RebindInterval, true);
	}
}

UASSkillSlotViewModel* UASSkillBarViewModel::GetSlotByIndex(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0: return QSlot;
	case 1: return WSlot;
	case 2: return ESlot;
	case 3: return RSlot;
	case 4: return ShiftSlot;
	default: return nullptr;
	}
}

void UASSkillBarViewModel::BindSlot(int32 SlotIndex)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	UASSkillSlotViewModel* SlotViewModel = GetSlotByIndex(SlotIndex);
	if (!ASC || !SlotViewModel || !SlotBindings.IsValidIndex(SlotIndex))
	{
		return;
	}

	FASSkillSlotBinding& Binding = SlotBindings[SlotIndex];

	const FGameplayAbilitySpec* FoundSpec = nullptr;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (ASSkillBarViewModelPrivate::DoesSpecMatchInputTag(Spec, Binding.InputTag))
		{
			FoundSpec = &Spec;
			break;
		}
	}

	if (!FoundSpec || !FoundSpec->Ability)
	{
		return;
	}

	Binding.bBound = true;

	//스킬이 아닌 어빌리티(UASSkillAbility가 아닌 것)는 아이콘 없이 표시
	const UASSkillAbility* SkillAbility = Cast<UASSkillAbility>(FoundSpec->Ability);
	SlotViewModel->SetSkillIcon(SkillAbility ? SkillAbility->GetSkillIcon() : nullptr);
	SlotViewModel->SetHasSkill(true);

	if (const FGameplayTagContainer* CooldownTags = FoundSpec->Ability->GetCooldownTags())
	{
		Binding.CooldownTags = *CooldownTags;
	}

	for (const FGameplayTag& CooldownTag : Binding.CooldownTags)
	{
		const FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleCooldownTagChanged, SlotIndex);
		Binding.CooldownTagHandles.Emplace(CooldownTag, Handle);
	}

	//리스폰 등으로 연결 시점에 이미 쿨타임 중일 수 있음
	if (!Binding.CooldownTags.IsEmpty() && ASC->HasAnyMatchingGameplayTags(Binding.CooldownTags))
	{
		Binding.bOnCooldown = true;
		SlotViewModel->SetOnCooldown(true);
		StartCooldownTimer();
		UpdateCooldowns();
	}
}

void UASSkillBarViewModel::UnbindSlot(int32 SlotIndex)
{
	if (!SlotBindings.IsValidIndex(SlotIndex))
	{
		return;
	}

	FASSkillSlotBinding& Binding = SlotBindings[SlotIndex];

	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& TagHandle : Binding.CooldownTagHandles)
		{
			ASC->UnregisterGameplayTagEvent(TagHandle.Value, TagHandle.Key, EGameplayTagEventType::NewOrRemoved);
		}
	}

	Binding.CooldownTagHandles.Reset();
	Binding.CooldownTags.Reset();
	Binding.bBound = false;
	Binding.bOnCooldown = false;

	if (UASSkillSlotViewModel* SlotViewModel = GetSlotByIndex(SlotIndex))
	{
		SlotViewModel->SetSkillIcon(nullptr);
		SlotViewModel->SetHasSkill(false);
		SlotViewModel->SetOnCooldown(false);
		SlotViewModel->SetCooldownPercent(0.0f);
	}
}

void UASSkillBarViewModel::HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount, int32 SlotIndex)
{
	UASSkillSlotViewModel* SlotViewModel = GetSlotByIndex(SlotIndex);
	if (!SlotViewModel || !SlotBindings.IsValidIndex(SlotIndex))
	{
		return;
	}

	FASSkillSlotBinding& Binding = SlotBindings[SlotIndex];

	//쿨타임 태그가 여러 개일 수 있어서 하나라도 남아 있으면 쿨타임 중
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	const bool bNowOnCooldown = ASC && ASC->HasAnyMatchingGameplayTags(Binding.CooldownTags);

	Binding.bOnCooldown = bNowOnCooldown;
	SlotViewModel->SetOnCooldown(bNowOnCooldown);

	if (bNowOnCooldown)
	{
		StartCooldownTimer();
		UpdateCooldowns();
	}
	else
	{
		SlotViewModel->SetCooldownPercent(0.0f);
	}
}

void UASSkillBarViewModel::UpdateCooldowns()
{
	bool bAnyOnCooldown = false;

	for (int32 SlotIndex = 0; SlotIndex < SlotBindings.Num(); ++SlotIndex)
	{
		const FASSkillSlotBinding& Binding = SlotBindings[SlotIndex];
		if (!Binding.bOnCooldown)
		{
			continue;
		}

		if (UASSkillSlotViewModel* SlotViewModel = GetSlotByIndex(SlotIndex))
		{
			SlotViewModel->SetCooldownPercent(GetCooldownRemainingPercent(Binding.CooldownTags));
		}

		bAnyOnCooldown = true;
	}

	//쿨타임 중인 슬롯이 없으면 타이머를 멈춰서 평소에는 연산하지 않음
	if (!bAnyOnCooldown)
	{
		StopCooldownTimer();
	}
}

float UASSkillBarViewModel::GetCooldownRemainingPercent(const FGameplayTagContainer& CooldownTags) const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC || CooldownTags.IsEmpty())
	{
		return 0.0f;
	}

	//쿨타임 GE가 부여하는 태그로 GE를 찾아 가장 오래 남은 것을 기준으로 함
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);

	float LongestRemaining = 0.0f;
	float LongestDuration = 0.0f;
	for (const TPair<float, float>& RemainingAndDuration : ASC->GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (RemainingAndDuration.Key > LongestRemaining)
		{
			LongestRemaining = RemainingAndDuration.Key;
			LongestDuration = RemainingAndDuration.Value;
		}
	}

	return LongestDuration > 0.0f ? FMath::Clamp(LongestRemaining / LongestDuration, 0.0f, 1.0f) : 0.0f;
}

void UASSkillBarViewModel::StartCooldownTimer()
{
	UWorld* World = CachedWorld.Get();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (!TimerManager.IsTimerActive(CooldownTimerHandle))
	{
		TimerManager.SetTimer(CooldownTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::UpdateCooldowns),
			ASSkillBarViewModelPrivate::CooldownUpdateInterval, true);
	}
}

void UASSkillBarViewModel::StopCooldownTimer()
{
	if (UWorld* World = CachedWorld.Get())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}
}
