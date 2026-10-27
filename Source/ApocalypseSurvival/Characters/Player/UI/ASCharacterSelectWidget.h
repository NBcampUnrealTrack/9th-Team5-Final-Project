#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Characters/Player/Data/ASCharacterDefinition.h"
#include "ASCharacterSelectWidget.generated.h"

class UASCharacterRoster;
class UTexture2D;
class UTextBlock;
class UButton;
class AASLobbyGameState;

/* 선택 화면에 표시할 캐릭터 정보 (정의 DA 의 표시용 값만 모은 것) */
USTRUCT(BlueprintType)
struct FASCharacterDisplayInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText DisplayName;
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText Title;
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText AbilityName;
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText Role;
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText CombatStyle;
	
	// "낮음 / 보통 / 높음"
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText DifficultyText;
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText RepresentativeWeapons;
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	FText Description;
	
	// 초상화 (왼쪽 그리드)
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	TSoftObjectPtr<UTexture2D> Icon;
	
	// 오른쪽 능력·스킬 목록
	UPROPERTY(BlueprintReadOnly, Category = "CharacterSelect")
	TArray<FASSkillDisplayEntry> SkillDisplays;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FASCharacterSelectEvent);

/*
 * 캐릭터 선택 화면의 로직. 배치(디자인)는 BP 자식 위젯에서 한다.
 * 캐릭터를 누르면 "고르는 중"만 바뀌고, 확정 버튼(RequestConfirm)을 눌러야 저장된다.
 * ESC 로 닫는다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "CharacterSelect")
	int32 GetCharacterCount() const;
	
	// 번호가 잘못되면 false
	UFUNCTION(BlueprintPure, Category = "CharacterSelect")
	bool GetCharacterInfo(int32 Index, FASCharacterDisplayInfo& OutInfo) const;
	
	UFUNCTION(BlueprintPure, Category = "CharacterSelect")
	int32 GetSelectedIndex() const { return SelectedIndex; }
	
	// 캐릭터 버튼 클릭 → 고르는 중으로만 변경 (확정 아님)
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void SelectCharacter(int32 Index);
	
	// 확정 버튼 → 선택 서브시스템에 저장하고 OnConfirmed 를 알린다
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void RequestConfirm();
	
	// 닫기 (ESC 또는 닫기 버튼) → OnBackRequested 를 알린다
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void RequestBack();
	UFUNCTION(BlueprintPure, Category = "CharacterSelect")
	float GetSelectionTimeRemaining() const;
	UFUNCTION(BlueprintPure, Category = "CharacterSelect")
	bool IsCharacterConfirmed() const;
	
	// 확정 완료 → 메인메뉴가 받아서 다음 화면(지금은 게임 맵)으로 이동
	UPROPERTY(BlueprintAssignable, Category = "CharacterSelect")
	FASCharacterSelectEvent OnConfirmed;
	
	// 닫기 → 메인메뉴가 받아서 메인 패널로 복귀
	UPROPERTY(BlueprintAssignable, Category = "CharacterSelect")
	FASCharacterSelectEvent OnBackRequested;
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	
	// 고르는 중인 캐릭터가 바뀌었을 때 (화면이 열릴 때도 한 번 호출). BP 에서 표시 갱신
	UFUNCTION(BlueprintImplementableEvent, Category = "CharacterSelect")
	void OnSelectionChanged(int32 NewIndex, const FASCharacterDisplayInfo& Info);
	
	// 선택 화면에 띄울 캐릭터 목록
	UPROPERTY(EditDefaultsOnly, Category = "CharacterSelect")
	TObjectPtr<const UASCharacterRoster> Roster;
	
	// 닫기 키
	UPROPERTY(EditDefaultsOnly, Category = "CharacterSelect|Input")
	FKey BackKey = EKeys::Escape;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CharacterSelectionTimeText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CharacterSelectionStatusText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CharacterReadyButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CharacterPlayerListText;
	
private:
	// 선택 서브시스템에 저장. 성공하면 true
	bool ConfirmSelection();
	
	const UASCharacterDefinition* GetDefinitionAt(int32 Index) const;
	
	// 이전에 확정한 캐릭터의 목록 번호 (없으면 0)
	int32 FindPreviousSelectionIndex() const;
	
	int32 SelectedIndex = 0;
	void UpdateNetworkSelectionDisplay();
	void CreatePlayerListDisplay();
	bool bReadyRequestPending = false;
	bool bRequestedReady = false;
	FTimerHandle SelectionDisplayTimer;
};
