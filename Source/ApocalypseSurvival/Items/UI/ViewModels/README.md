# 메뉴 UI의 MVVM 연결

게임 데이터와 서버 검증은 기존 컴포넌트가 소유한다. ViewModel은 로컬 표시 데이터와 UI 명령을 제공하고, 위젯은 레이아웃·스타일·입력 전달을 담당한다.

| 화면 | ViewModel | 수동 MVVM 컨텍스트 |
|---|---|---|
| 인벤토리 | ASInventoryViewModel / ASItemSlotViewModel | InventoryVM / SlotVM |
| 제작 | ASCraftingViewModel / ASCraftingEntryViewModel | CraftingVM / EntryVM |
| 퀘스트 | ASQuestInventoryViewModel | QuestInventoryVM |
| 모듈 | ASModuleLoadoutViewModel | ModuleLoadoutVM |
| 공통 탭 | ASPlayerMenuViewModel | MenuVM |

퀘스트·모듈·공통 탭은 `Presentation`을 FieldNotify로 게시한다. 네이티브 위젯은 이 알림을 구독하며, Blueprint에 Manual 컨텍스트가 있으면 같은 ViewModel을 주입한다. 화면을 열 때 초기 값을 읽고, 닫을 때 위젯 구독과 게임 데이터 구독을 해제한다. 표시 데이터가 같으면 변경 알림을 보내지 않는다.

퀘스트 위젯은 진행 판정이나 수량·상태 문구를 계산하지 않는다. 모듈 위젯도 설정 테이블이나 장착 컴포넌트를 직접 조회하지 않는다. 탭의 활성 상태·본문 인덱스와 화면 이동·닫기 명령은 공통 메뉴 ViewModel이 담당한다. 인벤토리와 제작의 기존 아이템·레시피 명령은 기존 ViewModel을 사용한다.

ViewModel은 퀘스트 수락·완료나 모듈 장착을 직접 변경하지 않는다. 메뉴 이동도 기존 PlayerController와 UI 소유 컴포넌트의 진입점을 사용한다.
