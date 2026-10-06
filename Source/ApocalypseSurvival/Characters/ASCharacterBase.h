#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ASCharacterBase.generated.h"

class UASAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * 전투에 참여하는 모든 캐릭터의 베이스.
 *
 * 이 클래스는 ASC 를 직접 만들지 않는다. 플레이어는 AASPlayerState 의 것을 빌려 쓰고,
 * 적은 자기 것을 가질 수 있어서 소유 위치가 서로 다르기 때문이다.
 * 자식 클래스가 자기 머신에서 ASC 를 알게 된 시점에 InitAbilitySystem 을 부른다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API AASCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AASCharacterBase();

	/** IAbilitySystemInterface. GAS 가 액터에서 ASC 를 찾을 때 쓰는 표준 경로다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 이 캐릭터를 ASC 의 아바타(월드에서 실제로 움직이는 몸)로 연결한다.
	 *
	 * 여러 번 불려도 안전하다. PlayerState 가 한 번 이상 복제되면 다시 불릴 수 있다.
	 * 어트리뷰트 세트는 넘기지 않는다. ASC 에 이미 등록돼 있으니 ASC 에서 찾는다.
	 *
	 * @param InOwnerActor  ASC 를 소유한 액터. 플레이어는 PlayerState 다.
	 */
	void InitAbilitySystem(UASAbilitySystemComponent* InAbilitySystemComponent, AActor* InOwnerActor);

	/** InitAbilitySystem 이 연결을 마친 뒤 모든 머신에서 불린다. */
	virtual void OnAbilitySystemInitialized();

private:
	void UnbindAttributeDelegates();
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& Data);

	/** 소유자는 따로 있고 여기서는 가리키기만 한다. 저장할 값이 아니라서 Transient 다. */
	UPROPERTY(Transient)
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle MoveSpeedChangedHandle;
};
