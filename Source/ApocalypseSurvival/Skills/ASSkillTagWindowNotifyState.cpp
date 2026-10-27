#include "Skills/ASSkillTagWindowNotifyState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/ASAbilityTags.h"

namespace ASSkillTagWindowPrivate
{
	UAbilitySystemComponent* GetOwnerASC(const USkeletalMeshComponent* MeshComp)
	{
		//에디터 미리보기 등 ASC 없는 경우 nullptr
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		return Owner ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) : nullptr;
	}
}

UASSkillTagWindowNotifyState::UASSkillTagWindowNotifyState()
{
	GrantedTags.AddTag(ASGameplayTags::State_Invulnerable);
}

void UASSkillTagWindowNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UAbilitySystemComponent* ASC = ASSkillTagWindowPrivate::GetOwnerASC(MeshComp))
	{
		//개수로 관리되므로 다른 무적(회피 등)과 겹쳐도 먼저 끝난 쪽이 상대 것을 지우지 않음
		ASC->AddLooseGameplayTags(GrantedTags);
	}
}

void UASSkillTagWindowNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UAbilitySystemComponent* ASC = ASSkillTagWindowPrivate::GetOwnerASC(MeshComp))
	{
		ASC->RemoveLooseGameplayTags(GrantedTags);
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UASSkillTagWindowNotifyState::GetNotifyName_Implementation() const
{
	//몽타주 타임라인에 붙인 태그 이름 표시 (예: State.Invulnerable)
	return GrantedTags.IsEmpty() ? TEXT("AS Skill Tag Window") : GrantedTags.ToStringSimple();
}
