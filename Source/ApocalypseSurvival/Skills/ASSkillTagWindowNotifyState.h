#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "ASSkillTagWindowNotifyState.generated.h"

//몽타주의 특정 구간 동안 시전자에게 태그를 붙임 (기본: State.Invulnerable = 무적)
//구간이 시작되면 붙이고 끝나면 뗌. 몽타주가 끊겨도 엔진이 NotifyEnd를 불러서 떼짐
//각 화면에서 따로 붙임 (복제 안 함). 피해 판정은 서버에서 하므로 서버 쪽 태그가 실제 무적을 결정
UCLASS(meta = (DisplayName = "AS Skill Tag Window"))
class APOCALYPSESURVIVAL_API UASSkillTagWindowNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UASSkillTagWindowNotifyState();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	//구간 동안 붙일 태그 (무적 = State.Invulnerable)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AS|Skill", meta = (Categories = "State"))
	FGameplayTagContainer GrantedTags;
};
