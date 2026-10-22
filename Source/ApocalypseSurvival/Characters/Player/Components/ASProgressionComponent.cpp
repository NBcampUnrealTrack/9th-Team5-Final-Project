#include "ASProgressionComponent.h"
#include "AbilitySystemGlobals.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "Characters/Player/Data/ASProgressionData.h"
#include "core/Framework/ASPlayerState.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Net/UnrealNetwork.h"

UASProgressionComponent::UASProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASProgressionComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UASProgressionComponent, Level);
	DOREPLIFETIME(UASProgressionComponent, CurrentExp);
	DOREPLIFETIME(UASProgressionComponent, UnlockedSkillUpgradeTierCount);
}

void UASProgressionComponent::AddExperience(int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0 || IsMaxLevel())
	{
		return;
	}
	
	const UASProgressionData* Data = GetProgressionData();
	if (!Data || Data->ExpToNextLevel.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Progression] 성장 데이터가 없어 경험치를 줄 수 없음 (캐릭터 정의의 ProgressionData 확인)"));
		return;
	}
	
	CurrentExp += Amount;
	
	// 한 번에 많이 받으면 여러 레벨이 오를 수 있다
	while (!IsMaxLevel() && CurrentExp >= Data->GetExpToNextLevel(Level))
	{
		CurrentExp -= Data->GetExpToNextLevel(Level);
		LevelUp();
	}
	
	// 최대 레벨이면 남은 경험치는 버린다
	if (IsMaxLevel())
	{
		CurrentExp = 0;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Progression] 경험치 +%d -> Lv.%d (%d/%d)"), Amount, Level, CurrentExp, GetExpToNextLevel());
	OnExpChanged.Broadcast(CurrentExp, GetExpToNextLevel());
}

int32 UASProgressionComponent::GetExpToNextLevel() const
{
	const UASProgressionData* Data = GetProgressionData();
	return Data ? Data->GetExpToNextLevel(Level) : 0;
}

bool UASProgressionComponent::IsMaxLevel() const
{
	const UASProgressionData* Data = GetProgressionData();
	return Data && Level >= Data->MaxLevel;
}

void UASProgressionComponent::LevelUp()
{
	++Level;
	
	if (const UASCharacterDefinition* Definition = GetCharacterDefinition())
	{
		ApplyLevelGrowth(Definition);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Progression] 레벨업: Lv.%d"), Level);
	OnLevelChanged.Broadcast(Level);
	
	// 이 레벨에서 열리는 강화 단계가 있으면 알린다
	if (const UASProgressionData* Data = GetProgressionData())
	{
		const int32 OldCount = UnlockedSkillUpgradeTierCount;
		UnlockedSkillUpgradeTierCount = Data->GetUnlockedSkillUpgradeTierCount(Level);
		BroadcastSkillUpgradeUnlocked(OldCount, UnlockedSkillUpgradeTierCount);
	}
}

void UASProgressionComponent::ApplyLevelGrowth(const UASCharacterDefinition* Definition) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	
	// 죽어 있는 동안 레벨업하면 체력을 채우지 않는다 (사망 중 부활 방지)
	const UASAbilitySystemComponent* ASASC = Cast<UASAbilitySystemComponent>(ASC);
	const bool bDead = ASASC && ASASC->IsDead();
	
	for (const FASAttributeGrowEntry& Entry : Definition->LevelGrowth)
	{
		if (!Entry.Attribute.IsValid() || !ASC->HasAttributeSetForAttribute(Entry.Attribute) || FMath::IsNearlyZero(Entry.ValuePerLevel))
		{
			continue;
		}
		
		// 기본값에 증가량만 더한다 (버프, 디버프는 효과로 걸리므로 섞이지 않는다)
		ASC->SetNumericAttributeBase(Entry.Attribute, ASC->GetNumericAttributeBase(Entry.Attribute) + Entry.ValuePerLevel);
		
		// 임시 규칙: 최대 체력이 오른 만큼 현재 체력도 회복 (밸런싱 때 조정)
		if (!bDead && Entry.Attribute == UASHealthAttributeSet::GetMaxHealthAttribute())
		{
			const FGameplayAttribute HealthAttribute = UASHealthAttributeSet::GetHealthAttribute();
			ASC->SetNumericAttributeBase(HealthAttribute, ASC->GetNumericAttributeBase(HealthAttribute) + Entry.ValuePerLevel);
		}
	}
}

const UASCharacterDefinition* UASProgressionComponent::GetCharacterDefinition() const
{
	const AASPlayerState* PlayerState = Cast<AASPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetCharacterDefinition() : nullptr;
}

const UASProgressionData* UASProgressionComponent::GetProgressionData() const
{
	const UASCharacterDefinition* Definition = GetCharacterDefinition();
	return Definition ? Definition->ProgressionData.Get() : nullptr;
}

UAbilitySystemComponent* UASProgressionComponent::GetAbilitySystemComponent() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void UASProgressionComponent::OnRep_Level()
{
	OnLevelChanged.Broadcast(Level);
}

void UASProgressionComponent::OnRep_CurrentExp()
{
	OnExpChanged.Broadcast(CurrentExp, GetExpToNextLevel());
}

void UASProgressionComponent::OnRep_UnlockedSkillUpgradeTierCount(int32 OldCount)
{
	BroadcastSkillUpgradeUnlocked(OldCount, UnlockedSkillUpgradeTierCount);
}

void UASProgressionComponent::BroadcastSkillUpgradeUnlocked(int32 FromCount, int32 ToCount)
{
	const UASProgressionData* Data = GetProgressionData();
	if (!Data)
	{
		return;
	}
	
	for (int32 TierIndex = FMath::Max(0, FromCount); TierIndex < ToCount; ++TierIndex)
	{
		if (!Data->SkillUpgradeTiers.IsValidIndex(TierIndex))
		{
			break;
		}
		
		const FASSkillUpgradeTier& Tier = Data->SkillUpgradeTiers[TierIndex];
		UE_LOG(LogTemp, Log, TEXT("[Progression] 스킬 강화 단계 %d 열림: Lv.%d %s (%d개 중 선택)"), TierIndex, Tier.Level, *Tier.SkillSlotTag.ToString(), Tier.ChoiceCount);
		OnSkillUpgradeUnlocked.Broadcast(TierIndex, Tier.SkillSlotTag, Tier.ChoiceCount);
	}
}

bool UASProgressionComponent::GetSkillUpgradeTier(int32 TierIndex, FASSkillUpgradeTier& OutTier) const
{
	const UASProgressionData* Data = GetProgressionData();
	if (!Data || !Data->SkillUpgradeTiers.IsValidIndex(TierIndex))
	{
		return false;
	}
	
	OutTier = Data->SkillUpgradeTiers[TierIndex];
	return true;
}

void UASProgressionComponent::NotifyProgressionDataChanged()
{
	// 레벨 이벤트는 레벨업 연출이 붙어 있어서 보내지 않는다. 필요 경험치만 다시 알린다.
	OnExpChanged.Broadcast(CurrentExp, GetExpToNextLevel());
}