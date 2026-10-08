#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GAS/ASAbilitySet.h"
#include "Items/Equipment/ASItemEquipmentInterface.h"
#include "ASEquipmentComponent.generated.h"

class UASAbilitySystemComponent;
class UASItemDefinition;
class UASItemInstance;

/**
 * 캐릭터 몸에 무기를 다는 컴포넌트. 플레이어 캐릭터에 붙는다.
 *
 * 인벤토리(컨트롤러)는 "무엇을 들기로 했는가"만 알고, 실제로 액터를 소켓에 붙이고
 * 공격 어빌리티를 부여하는 일은 여기서 한다. Lyra 의 인벤토리/장착 매니저 분리와 같다.
 * 인벤토리는 IASItemEquipmentInterface 로 이 컴포넌트를 찾아 부른다.
 *
 * 캐릭터에 두는 이유는 수명이다. 장착 액터는 메시에 붙어 캐릭터와 함께 사라지고,
 * 어빌리티는 PlayerState 의 ASC 에 남는다. 이 컴포넌트가 캐릭터와 같이 죽으면서
 * 어빌리티를 회수하므로 둘의 수명이 어긋나지 않는다. 무엇을 들고 있었는지는
 * PlayerState 에 적어 두고, 리스폰한 캐릭터가 그걸 보고 다시 단다.
 *
 * 모든 변경은 서버에서만 한다. 장착 액터는 복제되는 액터라 클라이언트에도 보인다.
 */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASEquipmentComponent : public UActorComponent, public IASItemEquipmentInterface
{
	GENERATED_BODY()

public:
	UASEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 서버 전용. 무기 아이템을 몸에 단다. 들고 있던 것은 먼저 뗀다.
	 * 에셋을 못 읽거나 소켓에 못 붙이면 false 를 돌려주고, 그때는 이전 장착이 그대로 남는다.
	 */
	bool EquipItem(UASItemInstance* Item);

	/** 서버 전용. 장착 액터를 없애고 부여했던 어빌리티를 회수한다. 리스폰 기록도 지운다. */
	void UnequipItem();

	/** 서버에서만 유효하다. 아이템 인스턴스는 인벤토리가 본인에게만 복제하므로 다른 클라이언트는 알 수 없다. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UASItemInstance* GetEquippedItem() const { return EquippedItem; }

	/** 몸에 붙어 있는 무기 액터. 모든 머신에서 볼 수 있다. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	AActor* GetEquippedActor() const { return EquippedActor; }

	// ── IASItemEquipmentInterface. 인벤토리가 부르는 입구다. 검증은 인벤토리가 끝내고 넘긴다. ──
	virtual bool EquipInventoryItem_Implementation(UASInventoryComponent* Inventory, UASItemInstance* Item, APawn* Avatar) override;
	virtual bool UnequipInventoryItem_Implementation(UASInventoryComponent* Inventory, APawn* Avatar) override;
	virtual UASItemInstance* GetEquippedInventoryItem_Implementation(const UASInventoryComponent* Inventory) const override;
	virtual AActor* GetEquippedInventoryActor_Implementation(const UASInventoryComponent* Inventory) const override;

protected:
	/** 캐릭터가 사라질 때 몸에서 떼기만 한다. 리스폰 기록은 남겨 새 캐릭터가 다시 달 수 있게 한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 액터 파괴와 어빌리티 회수. 명시적 해제와 캐릭터 파괴가 공유한다. */
	void ClearEquipment();

	/** 정의에 적힌 액터를 스폰해 메시 소켓에 붙인다. 액터 클래스가 비어 있으면 null 을 돌려주고 성공으로 본다. */
	AActor* SpawnEquippedActor(const UASItemDefinition& Definition, bool& bOutFailed) const;

	/**
	 * 무기의 일반 공격 어빌리티를 ASC 에 부여한다. 스펙에 일반 공격 입력 태그를 붙이므로
	 * 좌클릭이 오면 ASC 가 이 스펙을 고른다. 어빌리티 세트와 같은 방식이다.
	 */
	void GrantPrimaryAttack(UClass* AbilityClass, UASItemInstance* Item);

	/** 이 캐릭터가 쓰는 ASC. 플레이어는 PlayerState 의 것이다. 아직 연결 전이면 null. */
	UASAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;

	/** 리스폰 기록을 적어 둘 곳. 캐릭터보다 오래 사는 PlayerState 다. */
	void SaveEquippedItemRecord(UASItemInstance* Item) const;

	/** 무기의 일반 공격 어빌리티에 붙일 입력 태그. 기본은 InputTag.Attack.Primary 다. */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment", meta = (Categories = "InputTag"))
	FGameplayTag PrimaryAttackInputTag;

	UPROPERTY(Transient, Replicated)
	TObjectPtr<UASItemInstance> EquippedItem;

	UPROPERTY(Transient, Replicated)
	TObjectPtr<AActor> EquippedActor;

	/** 부여한 어빌리티 핸들. 해제할 때 이걸로 회수한다. */
	FASAbilitySet_GrantedHandles GrantedHandles;
};
