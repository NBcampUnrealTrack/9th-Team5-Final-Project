#include "Skills/AnimNotify_ASSkillEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Skills/ASSkillTags.h"

UAnimNotify_ASSkillEvent::UAnimNotify_ASSkillEvent()
{
	EventTag = ASGameplayTags::Event_Skill_Hit;
}

void UAnimNotify_ASSkillEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;

	// 에디터 미리보기 등 ASC 없는 경우 무시
	if (!Owner || !EventTag.IsValid() || !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner))
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}

FString UAnimNotify_ASSkillEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("AS Skill Event");
}
