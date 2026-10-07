#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "ASSkillEventNotify.generated.h"

//몽타주 타격 타이밍에 스킬 이벤트를 보냄
UCLASS(meta = (DisplayName = "AS Skill Event"))
class APOCALYPSESURVIVAL_API UASSkillEventNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UASSkillEventNotify();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AS|Skill", meta = (Categories = "Event"))
	FGameplayTag EventTag;
};
