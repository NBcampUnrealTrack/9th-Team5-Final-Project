#pragma once

#include "CoreMinimal.h"
#include "Characters/ASCharacterBase.h"
#include "GameplayTagContainer.h"
#include "Mass/Monster/Targeting/ASMonsterTargetable.h"
#include "ASPlayerCharacter.generated.h"

class UASAbilitySet;
class UASEquipmentComponent;
class UASMonsterTargetableComponent;
class UASCharacterDefinition;
class UASCharacterRoster;
class UNiagaraSystem;
class USoundBase;
struct FOnAttributeChangeData;
class UInputAction;
class UInputMappingContext;
class UGameplayAbility;
struct FGameplayEventData;

/**
 * 플레이어가 조종하는 캐릭터.
 *
 * 이동은 클릭 방식이다. AASPlayerController 가 명령을 내린 머신에서 길을 찾아 따라가고,
 * 그 결과는 일반 이동 입력처럼 이동 컴포넌트가 서버로 보낸다.
 *
 * 카메라는 이 캐릭터에 없다. 화면이 캐릭터와 따로 움직여야 해서
 * AASPlayerCameraManager 가 시점을 담당한다.
 *
 * 사망은 Lyra 방식이다. 서버가 체력 0 을 보면 ASC 에 사망 이벤트를 보내고, 사망 어빌리티
 * (UASGameplayAbility_Death)가 StartDeath → 몽타주 → FinishDeath 를 진행한다. StartDeath 가
 * 붙이는 State.Dead 태그는 모든 머신에 복제되므로 각 머신이 태그를 보고 자기 몫(이동 정지, 연출)을
 * 한다. 리스폰은 FinishDeath 가 AASGameMode 에 알린 뒤 게임모드가 맡는다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API AASPlayerCharacter : public AASCharacterBase, public IASMonsterTargetable
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

	virtual bool IsMonsterTargetable_Implementation() const override;
	virtual FVector GetMonsterTargetLocation_Implementation() const override;
	virtual float GetMonsterTargetRadius_Implementation() const override;
	virtual EASMonsterTargetCapability GetMonsterTargetCapabilities_Implementation() const override;
	virtual float GetMonsterTargetBaseScore_Implementation() const override;

	/**
	 * 사망 1단계. 서버 전용. 사망 어빌리티가 부른다.
	 * 복제되는 State.Dead 태그를 붙이고, 그 태그를 받은 각 머신이 이동 정지·충돌 해제·연출을 한다.
	 */
	void StartDeath();

	/**
	 * 사망 2단계. 서버 전용. 연출(몽타주)이 끝난 뒤 사망 어빌리티가 부른다.
	 * 게임모드에 알려 데스카운트와 리스폰을 맡긴다. 게임모드가 이 캐릭터를 파괴하고 새로 만든다.
	 */
	void FinishDeath();
	
	/* 쓰러져서 부활을 기다리는 중인가 (멀티 전용). 모든 머신에서 같은 답을 준다. */
	UFUNCTION(BlueprintPure, Category = "Downed")
	bool IsDowned() const;
	
	/* 서버 전용. 멀티 규칙에서 체력이 0이 되면 사망 대신 쓰러진다. */
	void EnterDowned();
	
	/** 서버 전용. 쓰러진 시간이 다 되면 사망으로 넘어간다. */
	void BleedOut();
	
	/** 서버 전용. 부활 완료. 체력을 일부 채우고 일어난다. */
	void Revive();
	
	/**
     * 서버에서 호출, 모든 머신에서 실행. 멀티에서 사망 후 시체를 숨긴다.
	 * 파괴하지 않는 이유: 관전 입력(좌클릭)을 이 캐릭터가 받으므로 빙의를 유지해야 한다.
	 * 숨기기만 하면 틱·충돌·애니메이션은 계속 돌기 때문에 같이 끈다. 이 설정들은 복제되지 않아서 Multicast 로 각 머신이 직접 끈다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastHideCorpse();

	/** 누군가 부활시키는 중인가 (UI용, 모든 머신) */
	UFUNCTION(BlueprintPure, Category = "Downed")
	bool IsBeingRevived() const { return bBeingRevived; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UASEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "Mass|Monster|Targeting")
	UASMonsterTargetableComponent* GetMonsterTargetableComponent() const { return MonsterTargetableComponent; }

	/** Listen-server debug visualization for the normal weapon attack. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawWeaponAttackDebug(
		FVector Start,
		FVector End,
		float Radius,
		bool bIsMelee,
		bool bHit);
	
	UFUNCTION(Exec)
	void DebugAddExp(int32 Amount);
	
	// 클라이언트에서 입력한 DebugAddExp를 서버에서 실행
	UFUNCTION(Server, Reliable)
	void ServerDebugAddExp(int32 Amount);
	
	// 자기 캐릭터 체력을 0으로 (멀티면 쓰러짐, 싱글이면 사망). 클라이언트에서 입력해도 서버에서 처리
	UFUNCTION(Exec)
	void DebugKillSelf();
	
	UFUNCTION(Server, Reliable)
	void ServerDebugKillSelf();
	
	// 캐릭터 선택 테스트 (선택 화면 나오기 전까지). Index 는 DebugCharacterRoster 순서
	UFUNCTION(Exec)
	void DebugSelectCharacter(int32 Index);
	
	UFUNCTION(Exec)
	void DebugShowSelectedCharacter();

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
	
	/** 쓰러지거나 일어날 때 모든 머신에서 불린다. 쓰러진 자세, 남은 시간 UI 같은 보이는 것은 BP에서 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Downed")
	void OnDownedChanged(bool bDowned);

	/** 쓰러진 뒤 사망까지 걸리는 시간 (임시값) */
	UPROPERTY(EditDefaultsOnly, Category = "Downed", meta = (ClampMin = "1.0", Units = "s"))
	float DownedDuration = 30.f;
	
	/** 부활에 걸리는 시간 (임시값) */
	UPROPERTY(EditDefaultsOnly, Category = "Downed", meta = (ClampMin = "0.1", Units = "s"))
	float ReviveDuration = 5.f;

	/** 부활 시 회복하는 체력 비율 (임시값) */
	UPROPERTY(EditDefaultsOnly, Category = "Downed", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ReviveHealthRatio = 0.3f;

	/** 부활을 시작하고 유지할 수 있는 거리 (임시값) */
	UPROPERTY(EditDefaultsOnly, Category = "Downed", meta = (ClampMin = "50.0", Units = "cm"))
	float ReviveRange = 250.f;

	/** 부활이 시작되거나 끝날 때(완료/취소) 모든 머신에서 불린다. 진행 바 같은 UI는 BP에서 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Downed")
	void OnBeingRevivedChanged(bool bRevivedNow);
	
	/** 캐릭터 전용 입력 매핑 (부활 F키). 공용 IMC_Default 와 별도로 추가한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> CharacterMappingContext;

	/** 부활 입력 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ReviveAction;
	
	/* 관전 중 다음 파티원 화면으로 넘기기 (좌클릭) */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SpectateNextAction;
	
	// 레벨업 순간 호출 (서버, 클라 모두). 화면 문구 같은 보이는 것은 BP에서 붙인다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Progression")
	void OnLevelUp(int32 NewLevel);

private:
	void InitAbilitySystemFromPlayerState();
	void UnbindDeathDelegates();

	/**
	 * 서버 전용. 체력이 0 이 되면 ASC 에 GameplayEvent.Death 를 보낸다. Lyra 와 같다.
	 * 이벤트를 받은 사망 어빌리티가 StartDeath → 연출 → FinishDeath 를 진행한다.
	 * 어빌리티가 부여돼 있지 않으면 연출 없이 두 단계를 바로 실행한다.
	 */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** 사망 태그가 붙거나 떨어졌을 때. 모든 머신에서 불린다. */
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 이 머신에서 보이는 사망 처리. 서 있던 자리에 멈추고 더는 부딪히지 않는다. */
	void ApplyLocalDeath();

	/** ApplyLocalDeath 를 되돌린다. 사망 태그가 새 몸에 잘못 걸렸다가 떨어질 때 쓴다. */
	void RestoreFromDeath();
	
	/** 서버 전용. 사망 이벤트로 사망 어빌리티를 켠다. 어빌리티가 없으면 연출 없이 바로 사망 처리한다. */
	void RequestDeath(const FGameplayEventData& Payload);

	/** 쓰러짐 태그가 붙거나 떨어졌을 때. 모든 머신에서 불린다. */
	void HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount);

	FDelegateHandle DownedTagChangedHandle;
	FTimerHandle BleedOutTimerHandle;

	/** 서버 전용. 부활 시작 / 취소 / 진행 확인 */
void StartRevive(AASPlayerCharacter* Reviver);
	void CancelRevive();
	void TickRevive();

	UFUNCTION()
	void OnRep_BeingRevived();

	UPROPERTY(ReplicatedUsing = OnRep_BeingRevived)
	bool bBeingRevived = false;

	/** 서버 전용. 지금 부활시키는 중인 파티원 */
	TWeakObjectPtr<AASPlayerCharacter> ActiveReviver;

	FTimerHandle ReviveTickTimerHandle;
	float ReviveElapsed = 0.f;
	
	/** 로컬: F키. 가까운 쓰러진 파티원을 찾아 서버에 부활을 요청한다. */
	void OnRevivePressed();
	AASPlayerCharacter* FindReviveTarget() const;

	UFUNCTION(Server, Reliable)
	void ServerRequestRevive(AASPlayerCharacter* Target);

	/** 서버 전용. 부활시키는 사람이 어빌리티를 쓰면 부활 취소 */
	void HandleReviverAbilityActivated(UGameplayAbility* Ability);

	/** 서버 전용. 부활시키는 사람 정보와 구독을 정리한다. */
	void ClearReviver();

	TWeakObjectPtr<UAbilitySystemComponent> ReviverAbilitySystem;
	FDelegateHandle ReviverAbilityActivatedHandle;
	
	/* 로컬: 좌클릭. 사망(관전) 중이면 서버에 다음 파티원 관전을 요청한다. */
	void OnSpectateNextPressed();
	
	UFUNCTION(Server, Reliable)
	void ServerRequestSpectateNext();
	
	// 성장 컴포넌트(PlayerState) 레벨업 이벤트 구독. ASC 연결 때 걸고 EndPlay에서 푼다
	void BindProgressionDelegates();
	void UnbindProgressionDelegates();
	
	UFUNCTION()
	void HandleLevelChanged(int32 NewLevel);

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle DeadTagChangedHandle;

	/** FinishDeath 가 두 번 불려도 게임모드에 한 번만 알리기 위한 표시. 서버만 쓴다. */
	bool bDeathFinished = false;

	/**
	 * 무기를 몸에 다는 컴포넌트. 인벤토리(컨트롤러)가 인터페이스로 이 컴포넌트를 찾아 장착을 맡긴다.
	 * 이 컴포넌트는 장착 상태와 액터 부착만 맡고, 공격 어빌리티는 기본 AbilitySet이 소유한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASEquipmentComponent> EquipmentComponent;

	/** 서버 Target Registry에 이 Pawn을 범용 몬스터 Primary 타깃으로 등록한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass|Monster|Targeting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMonsterTargetableComponent> MonsterTargetableComponent;

	/**
	 * 이 캐릭터가 처음부터 갖는 어빌리티(회피, 스킬). BP 자식에서 지정한다.
	 * 부여는 PlayerState 가 한 번만 하므로 리스폰해도 중복되지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TObjectPtr<const UASAbilitySet> DefaultAbilitySet;
	
	/*
	 * 이 캐릭터의 기본 스탯, 성장 데이터. BP 자식에서 지정한다.
	 * 캐릭터 선택 기능이 생기면 게임모드가 PlayerState에 먼저 정해 주고, 이 값은 대체값이 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Character")
	TObjectPtr<const UASCharacterDefinition> CharacterDefinition;
	
	// DebugSelectCharacter가 쓰는 캐릭터 목록 (테스트용)
	UPROPERTY(EditDefaultsOnly, Category = "Character|Debug")
	TObjectPtr<const UASCharacterRoster> DebugCharacterRoster;
	
	// 레벨업 이펙트, 사운드 (리소스 나오면 BP에서 지정, 비어있으면 생략)
	UPROPERTY(EditDefaultsOnly, Category = "Progression|LevelUp")
	TObjectPtr<UNiagaraSystem> LevelUpEffect;
	
	UPROPERTY(EditDefaultsOnly, Category = "Progression|LevelUp")
	TObjectPtr<USoundBase> LevelUpSound;
};
