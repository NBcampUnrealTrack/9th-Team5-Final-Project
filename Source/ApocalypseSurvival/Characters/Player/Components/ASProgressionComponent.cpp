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
	DOREPLIFETIME(UASProgressionComponent, SkillPoints);
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

bool UASProgressionComponent::SpendSkillPoints(int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0 || SkillPoints < Amount)
	{
		return false;
	}
	
	SkillPoints -= Amount;
	OnSkillPointsChanged.Broadcast(SkillPoints);
	return true;
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
	
	if (const UASProgressionData* Data = GetProgressionData())
	{
		const int32 Gained = Data->GetSkillPointAtLevel(Level);
		if (Gained >0)
		{
			SkillPoints += Gained;
			UE_LOG(LogTemp, Log, TEXT("[Progression] 스킬포인트 +%d (보유 %d)"), Gained, SkillPoints);
			OnSkillPointsChanged.Broadcast(SkillPoints);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Progression] 레벨업: Lv.%d"), Level);
	OnLevelChanged.Broadcast(Level);
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

void UASProgressionComponent::OnRep_SkillPoints()
{
	OnSkillPointsChanged.Broadcast(SkillPoints);
}
