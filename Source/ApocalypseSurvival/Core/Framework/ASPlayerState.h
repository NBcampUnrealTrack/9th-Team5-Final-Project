#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ASAbilitySet.h"
#include "ASPlayerState.generated.h"

class UASAbilitySystemComponent;
class UASCombatAttributeSet;
class UASHealthAttributeSet;
class UASItemInstance;
class UASPlayerAttributeSet;
class UASCharacterDefinition;
class UASProgressionComponent;

class UASModuleComponent;

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
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 클라이언트에서 컨트롤러와 연결될 때 호출된다. 여기서 고른 캐릭터를 서버로 보낸다.
	virtual void ClientInitialize(AController* C) override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	// 서버 전용. 이 플레이어의 캐릭터 선택을 받았는가 (선택 없음을 받은 것도 포함)
	bool HasReceivedCharacterSelection() const { return bCharacterSelectionReceived; }

	// 서버 전용. 선택이 오지 않아 기본 캐릭터로 진행할 때 게임모드가 호출한다.
	void MarkCharacterSelectionReceived() { bCharacterSelectionReceived = true; }

	/** IAbilitySystemInterface. GAS 가 액터에서 ASC 를 찾을 때 쓰는 표준 경로다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UASAbilitySystemComponent* GetASAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Modules")
	UASModuleComponent* GetModuleComponent() const { return ModuleComponent; }

	/**
	 * 서버 전용. 캐릭터 기본 어빌리티 세트를 한 번만 부여한다.
	 * 어빌리티는 ASC 와 함께 여기 남으므로 리스폰 때 다시 부여하면 중복된다. 두 번째 호출부터는 무시한다.
	 */
	void GrantDefaultAbilitySetOnce(const UASAbilitySet* AbilitySet);
	
	// 서버전용. 캐릭터 정의의 전용 어빌리티 세트를 한번만 부여한다. 비어있으면 생략
	void GrantCharacterAbilitySetOnce();

	/**
	 * 서버 전용. 새 캐릭터에 빙의할 때 사망 상태를 걷어 낸다.
	 * ASC 가 여기 남아 있어서 죽은 채로 두면 새 캐릭터도 죽은 상태로 시작한다.
	 * 사망 태그를 떼고 체력·스태미나·마나를 최대로 채운다. 첫 빙의 때 불려도 해가 없다.
	 */
	void ResetForRespawn();

	/**
	 * 서버 전용. 장착 컴포넌트가 "지금 든 무기"를 여기에 적어 둔다.
	 * 장착 컴포넌트는 캐릭터와 함께 사라지므로, 리스폰한 캐릭터는 이 기록을 보고 다시 단다.
	 * 아이템 인스턴스는 컨트롤러의 인벤토리가 소유해 캐릭터보다 오래 산다.
	 */
	void SetEquippedItemRecord(UASItemInstance* Item);
	UASItemInstance* GetEquippedItemRecord() const;
	
	/*
	 * 서버 전용. 이 플레이어가 쓸 캐릭터 정의를 정한다.
	 * 이미 어트리뷰트에 적용된 뒤에는 무시한다 (게임 중 캐릭터 변경 없음).
	 */
	void SetCharacterDefinition(const UASCharacterDefinition* InDefinition);
	
	const UASCharacterDefinition* GetCharacterDefinition() const { return CharacterDefinition; }
	
	/*
	 * 서버 전용. 캐릭터 정의의 기본 스탯을 어트리뷰트에 한 번만 넣는다.
	 * 리스폰 때는 다시 넣지 않는다. 레벨업으로 오른 값이 초기화되면 안 되기 때문이다.
	 */
	void ApplyCharacterDefinitionOnce();
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	UASProgressionComponent* GetProgressionComponent() const { return ProgressionComponent; }

private:
	// 클라이언트 -> 서버. 고른 캐릭터 정의를 보낸다. 고르지 않았으면 nullptr
	UFUNCTION(Server, Reliable)
	void ServerSubmitCharacterSelection(const UASCharacterDefinition* InDefinition);
	
	// 클라이언트: 캐릭터 정의가 도착하면 성장 UI를 다시 갱신한다.
	UFUNCTION()
	void OnRep_CharacterDefinition();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modules", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASModuleComponent> ModuleComponent;

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
	
	// 캐릭터 전용 세트의 부여 기록
	UPROPERTY()
	FASAbilitySet_GrantedHandles CharacterAbilityHandles;
	
	bool bCharacterAbilitySetGranted = false;

	/** 약참조다. 인벤토리에서 버려진 아이템을 붙들어 두면 안 된다. */
	TWeakObjectPtr<UASItemInstance> EquippedItemRecord;
	
	/* 이 플레이어의 캐릭터 정의. 서버가 정하고 모든 클라이언트에 복제한다. */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterDefinition)
	TObjectPtr<const UASCharacterDefinition> CharacterDefinition;

	/* 서버 전용. 캐릭터 선택을 받았는가. 받기 전에는 게임모드가 스폰을 보류한다. */
	bool bCharacterSelectionReceived = false;

	/* 클라이언트 전용. 선택을 이미 보냈는가 (중복 전송 방지) */
	bool bCharacterSelectionSent = false;
	
	/* 캐릭터 정의를 어트리뷰트에 이미 넣었는가. 서버만 쓴다. */
	bool bCharacterDefinitionApplied = false;
	
	/* 레벨, 경험치, 스킬포인트. PlayerState에 있어서 리스폰 시 유지 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASProgressionComponent> ProgressionComponent;
};
