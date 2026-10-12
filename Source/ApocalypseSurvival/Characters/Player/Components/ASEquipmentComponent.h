#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/Equipment/ASItemEquipmentInterface.h"
#include "ASEquipmentComponent.generated.h"

class UASItemDefinition;
class UASItemInstance;

/**
 * 캐릭터 몸에 무기를 다는 컴포넌트. 플레이어 캐릭터에 붙는다.
 *
 * 인벤토리(컨트롤러)는 "무엇을 들기로 했는가"만 알고, 실제로 액터를 소켓에 붙이는
 * 일은 여기서 한다. 공격 어빌리티는 캐릭터 기본 세트가 한 번만 부여한다.
 * 인벤토리는 IASItemEquipmentInterface 로 이 컴포넌트를 찾아 부른다.
 *
 * 캐릭터에 두는 이유는 수명이다. 장착 액터는 메시에 붙어 캐릭터와 함께 사라지고,
 * 공격 어빌리티는 PlayerState 의 ASC 에 계속 남는다. 무엇을 들고 있었는지는
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

	/** 서버 전용. 장착 액터를 없애고 리스폰 기록도 지운다. */
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
	/** 액터 파괴와 장착 상태 초기화. 명시적 해제와 캐릭터 파괴가 공유한다. */
	void ClearEquipment();

	/** 정의에 적힌 액터를 스폰해 메시 소켓에 붙인다. 액터 클래스가 비어 있으면 null 을 돌려주고 성공으로 본다. */
	AActor* SpawnEquippedActor(const UASItemDefinition& Definition, bool& bOutFailed) const;

	/** 리스폰 기록을 적어 둘 곳. 캐릭터보다 오래 사는 PlayerState 다. */
	void SaveEquippedItemRecord(UASItemInstance* Item) const;

	UPROPERTY(Transient, Replicated)
	TObjectPtr<UASItemInstance> EquippedItem;

	UPROPERTY(Transient, Replicated)
	TObjectPtr<AActor> EquippedActor;

};
