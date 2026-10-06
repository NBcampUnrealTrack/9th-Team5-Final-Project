#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ASPlayerState.generated.h"

class UASAbilitySystemComponent;
class UASAttributeSet;

/**
 * 플레이어의 어빌리티 시스템을 소유한다.
 *
 * 캐릭터가 아니라 여기에 두는 이유는 리스폰 때문이다. 캐릭터는 죽으면 파괴되고
 * 새로 생성되지만 PlayerState 는 접속해 있는 동안 계속 남는다. 여기에 두면
 * 어트리뷰트, 쿨타임, 부여된 어빌리티가 캐릭터가 바뀌어도 유지된다.
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

	UASAttributeSet* GetAttributeSet() const { return AttributeSet; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UASAttributeSet> AttributeSet;
};
