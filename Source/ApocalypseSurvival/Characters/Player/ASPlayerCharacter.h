#pragma once

#include "CoreMinimal.h"
#include "Characters/ASCharacterBase.h"
#include "GameplayTagContainer.h"
#include "ASPlayerCharacter.generated.h"

class UASAbilitySet;
class UASEquipmentComponent;
struct FOnAttributeChangeData;

/**
 * 플레이어가 조종하는 캐릭터.
 *
 * 이동은 클릭 방식이다. AASPlayerController 가 명령을 내린 머신에서 길을 찾아 따라가고,
 * 그 결과는 일반 이동 입력처럼 이동 컴포넌트가 서버로 보낸다.
 *
 * 카메라는 이 캐릭터에 없다. 화면이 캐릭터와 따로 움직여야 해서
 * AASPlayerCameraManager 가 시점을 담당한다.
 *
 * 사망은 서버가 체력 0 을 보고 State.Dead 태그를 붙이는 것으로 시작한다. 태그는 모든
 * 머신에 복제되므로 각 머신이 태그를 보고 자기 몫(이동 정지, 연출)을 한다. 리스폰은
 * AASGameMode 가 맡고, 이 캐릭터는 죽었다고 알리기만 한다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API AASPlayerCharacter : public AASCharacterBase
{
	GENERATED_BODY()

public:
	AASPlayerCharacter();

	/** 서버: 빙의되는 시점에는 PlayerState 가 이미 있다. 기본 어빌리티도 여기서 부여한다. */
	virtual void PossessedBy(AController* NewController) override;

	/** 클라이언트: PlayerState 는 복제로 도착한다. */
	virtual void OnRep_PlayerState() override;

	/** 죽어 있는가. 모든 머신에서 같은 답을 준다. */
	UFUNCTION(BlueprintPure, Category = "Death")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UASEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** ASC 연결이 끝난 뒤. 체력과 사망 태그 구독을 여기서 건다. */
	virtual void OnAbilitySystemInitialized() override;

	/**
	 * 사망 연출을 시작할 때 모든 머신에서 불린다. 래그돌, 몽타주 같은 보이는 것은 BP 에서 붙인다.
	 * 이동 정지와 충돌 해제는 C++ 이 먼저 해 둔다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void OnDeathStarted();

private:
	void InitAbilitySystemFromPlayerState();
	void UnbindDeathDelegates();

	/** 서버 전용. 체력이 0 이 되면 사망을 시작한다. */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** 사망 태그가 붙거나 떨어졌을 때. 모든 머신에서 불린다. */
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	/**
	 * 서버 전용. 사망 태그를 붙이고 실행 중인 어빌리티를 끊은 뒤 게임모드에 알린다.
	 * 게임모드가 대기 시간 뒤 이 캐릭터를 파괴하고 새로 만든다.
	 */
	void StartDeath();

	/** 이 머신에서 보이는 사망 처리. 서 있던 자리에 멈추고 더는 부딪히지 않는다. */
	void ApplyLocalDeath();

	/** ApplyLocalDeath 를 되돌린다. 사망 태그가 새 몸에 잘못 걸렸다가 떨어질 때 쓴다. */
	void RestoreFromDeath();

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle DeadTagChangedHandle;

	/**
	 * 무기를 몸에 다는 컴포넌트. 인벤토리(컨트롤러)가 인터페이스로 이 컴포넌트를 찾아 장착을 맡긴다.
	 * 액터 부착과 공격 어빌리티 부여는 캐릭터 수명을 따라야 해서 여기에 둔다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASEquipmentComponent> EquipmentComponent;

	/**
	 * 이 캐릭터가 처음부터 갖는 어빌리티(회피, 스킬). BP 자식에서 지정한다.
	 * 부여는 PlayerState 가 한 번만 하므로 리스폰해도 중복되지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TObjectPtr<const UASAbilitySet> DefaultAbilitySet;
};
