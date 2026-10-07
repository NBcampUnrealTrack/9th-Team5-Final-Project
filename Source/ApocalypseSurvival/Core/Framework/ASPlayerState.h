#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ASAbilitySet.h"
#include "ASPlayerState.generated.h"

class UASAbilitySystemComponent;
class UASCombatAttributeSet;
class UASHealthAttributeSet;
class UASPlayerAttributeSet;

/**
 * 플레이어의 어빌리티 시스템을 소유한다.
 *
 * 캐릭터가 아니라 여기에 두는 이유는 리스폰 때문이다. 캐릭터는 죽으면 파괴되고
 * 새로 생성되지만 PlayerState 는 접속해 있는 동안 계속 남는다. 여기에 두면
 * 어트리뷰트, 쿨타임, 부여된 어빌리티가 캐릭터가 바뀌어도 유지된다.
 *
 * 어트리뷰트 세트 셋도 여기서 만든다. ASC 와 같은 액터의 기본 서브오브젝트라
 * ASC 가 초기화될 때 알아서 등록한다. 값을 읽을 때는 ASC->GetSet<세트>() 를 쓴다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API AASPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AASPlayerState();

	/** IAbilitySystemInterface. GAS 가 액터에서 ASC 를 찾을 때 쓰는 표준 경로다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UASAbilitySystemComponent* GetASAbilitySystemComponent() const { return AbilitySystemComponent; }

	/**
	 * 서버 전용. 캐릭터 기본 어빌리티 세트를 한 번만 부여한다.
	 * 어빌리티는 ASC 와 함께 여기 남으므로 리스폰 때 다시 부여하면 중복된다. 두 번째 호출부터는 무시한다.
	 */
	void GrantDefaultAbilitySetOnce(const UASAbilitySet* AbilitySet);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UASHealthAttributeSet> HealthAttributeSet;

	UPROPERTY()
	TObjectPtr<UASCombatAttributeSet> CombatAttributeSet;

	UPROPERTY()
	TObjectPtr<UASPlayerAttributeSet> PlayerAttributeSet;

	/** 기본 세트가 부여한 어빌리티 핸들. 비어 있으면 아직 부여 전이다. */
	UPROPERTY()
	FASAbilitySet_GrantedHandles DefaultAbilityHandles;
};
