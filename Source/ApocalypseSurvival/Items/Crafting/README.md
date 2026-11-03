# 무기 제작 설정

현재 범위는 고정 레시피, 즉시 제작, 무기 1개 지급이다. 결과는 가방에 들어가며 기존 장착 기능으로 장착한다.

## 에디터에서 연결하기

1. 기존 `ASItemDefinition` 에셋으로 재료를 만든다. `ItemType = CraftingMaterial`, `MaxStackSize`는 원하는 재료 묶음 크기로 설정한다.
2. 결과 무기는 기존 무기 아이템 정의를 사용한다. `ItemType = Weapon`, `MaxStackSize = 1`이며 공격 프로필 등 기존 무기 설정이 필요하다.
3. 콘텐츠 브라우저의 **Miscellaneous → Data Asset → ASCraftingRecipeDefinition**으로 레시피를 만든다.
4. `RecipeId`에 고유 ID, `DisplayName`에 표시 이름을 지정한다. `Ingredients`에 재료 정의와 수량을 넣고, `Result`에는 무기 정의와 수량 1을 지정한다.
5. **Miscellaneous → Data Asset → ASCraftingCatalog**으로 공용 카탈로그(예: `DA_CraftingCatalog`)를 만든다. 카탈로그의 **Recipes**에 사용할 레시피를 등록한다. 같은 ID를 두 번 등록하면 해당 ID는 사용할 수 없다. 콘텐츠 브라우저의 Validate Assets로 빈 참조, 중복 ID, 잘못된 레시피 설정을 확인할 수 있다.
6. 실제 게임에서 사용하는 PlayerController Blueprint의 **CraftingComponent → Catalog**에 이 카탈로그를 한 번 지정한다. 이후 새 레시피를 추가하거나 제거할 때는 카탈로그의 Recipes만 수정하면 된다. 여러 PlayerController Blueprint가 같은 카탈로그를 공유할 수도 있다.
7. UI 버튼이나 Enhanced Input에서 PlayerController의 `ToggleCrafting`을 호출한다. 별도 입력 에셋 없이 시험하려면 PIE 콘솔에서 `ToggleCrafting`을 입력한다. 창의 `닫기` 버튼이나 Escape로 닫는다.
8. 재료는 기존 전리품/시작 아이템 설정으로 지급한다. 제작 창에서 레시피 선택 → 보유/필요 수량 확인 → 제작을 누른다.

실제 재료 종류와 무기 조합이 정해지지 않아 제작 재료·레시피 콘텐츠 에셋이나 기존 입력 매핑은 변경하지 않았다. 카탈로그를 지정하지 않았거나 레시피 목록이 비어 있으면 제작 창에 안내를 표시한다. 기본 창은 `WBP_Crafting`을 사용하며 UMG 디자이너에서 편집한다. 목록은 창을 열 때 카탈로그에서 읽으며, 서버도 자신에게 설정된 카탈로그로 제작 요청을 검증한다.

이전 방식으로 컴포넌트의 `Available Recipes` 배열에 설정한 값이 있다면, 해당 레시피들을 새 카탈로그의 Recipes로 옮기고 컴포넌트에 Catalog를 지정해야 한다. 기존 배열 값은 자동 이전되지 않는다. 기존 Blueprint에서 배열을 직접 읽었다면 `GetAvailableRecipes` 호출로 변경한다.

## Blueprint API

- `GetCraftingComponent → GetAvailableRecipes()`: 컴포넌트에 지정된 카탈로그에서 UI용 목록을 가져온다. 빈 참조와 중복 ID는 목록에서 제외한다.
- `GetCraftingComponent → CanCraft(RecipeId)`: 버튼 활성화용 사전 확인. 최종 판정은 서버가 다시 한다.
- `RequestCraft(RecipeId)`: 로컬 플레이어가 보내는 제작 요청. 재료와 결과 수량은 클라이언트에서 전달받지 않는다.
- `OnCraftingFinished(RecipeId, Result)`: 요청한 플레이어의 성공/실패 알림. 인벤토리 데이터는 기존 복제 경로로 갱신되므로 클라이언트의 `OnInventoryChanged`에서도 UI를 갱신한다.
- `CanExchangeItems / TryExchangeItems`: 인벤토리의 일괄 교환 API. 가방 슬롯만 사용하며 장착 슬롯을 소비하거나 변경하지 않는다. 실제 변경은 서버에서만 가능하다.

전체 재료 수량과 소비 후 슬롯 배치를 먼저 계산한다. 검증 실패 시 인스턴스, 수량, 슬롯을 변경하지 않는다. 성공하면 살아남은 인스턴스의 ID를 유지하고 결과 인스턴스를 추가한 뒤 기존 인벤토리 알림/복제를 실행한다. 재료 중복 행은 합산하고, 여러 스택에 나뉜 재료도 소비한다.

초기 아이템 지급과 제작 교환은 `ASInventoryMutationPlan`의 공통 배치 계산을 사용한다. 계획 계산은 실제 인벤토리를 변경하지 않으며, 적용과 변경 알림은 `ASInventoryTransactions.cpp`에서 처리한다. 장착·드롭·비동기 사용 요청 구현은 `ASInventoryActions.cpp`에 모았다.

기본 제작 창의 계산과 이벤트 구독은 `UASCraftingViewModel`이 담당한다. 인벤토리 변경, 레시피 선택, 제작 응답, 플레이어의 ASC 준비/사망 상태, 게임 단계 변경 알림으로 갱신한다. 위젯은 FieldNotify를 받아 표시와 입력을 처리하며, 닫을 때 ViewModel의 구독을 해제한다. [UI MVVM 구조와 Blueprint 연결](../UI/README.md)을 참고한다.

## 수동 확인 항목

제작 자동 테스트 소스와 실행 산출물은 프로젝트 정리 과정에서 제거했다. 아래 항목은 에디터에서 동작을 확인할 때 사용한다.

- 가방 포화 실패 시 재료/인스턴스 유지
- 재료 소비로 생긴 빈칸에 결과 지급, 자동 장착 없음
- 사전 확인에서 인벤토리 변경 없음
- 중복 재료 행 합산, 여러 스택 소비, 일부 재료 부족 시 전체 취소
- 잘못된 결과/수량 및 합산 수량 오버플로 거부
- 오래된 재요청 시 현재 재료로 재검증
- 무소유 인벤토리 변경 거부, 미등록/중복 레시피 ID 거부
- 카탈로그 공유 및 목록 변경 반영, 카탈로그 미지정 처리, 다른 카탈로그의 레시피 거부
- 카탈로그의 빈 참조/중복 ID/잘못된 레시피 검증

멀티플레이 PIE에서는 호스트와 클라이언트 각각 제작, 사망 중 요청 거부, 제작 직전 재료 버리기, 결과 무기 장착까지 추가 확인한다.
