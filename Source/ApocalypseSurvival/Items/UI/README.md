# UMG 디자이너에서 아이템 UI 수정하기

콘텐츠 브라우저의 **ApocalypseSurvival → Items → UI**에 있는 Widget Blueprint를 열고 **Designer** 탭에서 편집한다. 기본 게임 화면도 이 에셋들을 사용한다.

| 에셋 | 편집 대상 |
| --- | --- |
| `WBP_InventoryPanel` | I 키로 여는 전체 인벤토리, 장착 영역, 보관함, 제작·닫기 버튼 |
| `WBP_InventoryBar` | 우측 아래 퀵슬롯 HUD |
| `WBP_InventorySlot` | 인벤토리와 퀵슬롯의 공통 슬롯 크기·아이콘 영역·폰트·테두리 |
| `WBP_Loot` | 전리품 상자 창과 제목·목록·닫기 버튼 |
| `WBP_LootEntry` | 전리품 목록의 공통 아이템 칸 |
| `WBP_Crafting` | 상시 제작법 목록, 선택 무기 미리보기·능력치, 필요 재료, 제작·닫기 버튼 |
| `WBP_CraftingRecipeEntry` | 왼쪽 목록의 무기 선택 버튼·분류·선택 강조 |
| `WBP_CraftingMaterialEntry` | 오른쪽 재료 아이콘·이름·보유/필요 수량 |

## 디자인 변경

- 위치·앵커·크기·패딩·색상·폰트는 Designer에서 수정한다. 슬롯의 기본 크기는 `WBP_InventorySlot` 루트 SizeBox에서 변경한다. 데이터 갱신 시 C++에서 크기나 폰트를 다시 지정하지 않는다.
- 반복되는 슬롯은 아이템 수와 가방 용량에 맞춰 생성한다. 공통 칸의 모양은 슬롯 에셋에서, 칸 사이 간격은 부모 화면의 UniformGridPanel `Slot Padding`에서 수정한다.
- **Class Defaults → Inventory → Layout**에서 `Storage Columns`와 `Quick Slots Per Row`를 조절한다. 전리품 창의 **Loot → Layout → Columns**에서 열 수를 조절한다.
- 전체 인벤토리는 Designer에서도 기본 보관함 16개, 퀵슬롯 10개, 무기·가방 장착 위치 2개를 미리 표시한다. 미리보기 아이템명과 수량은 예시다.
- 슬롯 테두리의 기본 색상은 Designer의 `SlotBorder`에서, 장착 상태 색상은 **Class Defaults → Inventory → Appearance → Equipped Slot Color**에서 설정한다. 아이콘·아이템 수량·장착 표시·등급 색상은 실행 중 데이터에 따라 바뀐다.
- 아이콘과 등급 레이어는 빈 이미지가 흰 사각형으로 표시되지 않도록 Designer에서 기본 숨김 상태다. 해당 요소를 직접 미리 보려면 Hierarchy의 눈 아이콘을 켠다. 실행 중 표시 여부는 아이템 데이터가 결정한다.
- 드래그 중 표시하는 아이템도 해당 슬롯 Widget Blueprint를 재사용한다.

## ViewModel과 바인딩

| 화면 | ViewModel 컨텍스트 | 생성 방식 |
| --- | --- | --- |
| 인벤토리·퀵슬롯·전리품 | `InventoryVM` (`UASInventoryViewModel`) | Manual |
| 인벤토리 슬롯·전리품 칸 | `SlotVM` (`UASItemSlotViewModel`) | Manual |
| 제작 | `CraftingVM` (`UASCraftingViewModel`) | Manual |
| 제작법·재료 행 | `EntryVM` (`UASCraftingEntryViewModel`) | Manual |

컨텍스트는 에셋에 등록되어 있으며 C++ 뷰 어댑터가 실제 인스턴스를 전달한다. 전체 인벤토리와 HUD는 `UASItemInteractionComponent`가 보관하는 같은 인벤토리·슬롯 ViewModel을 공유한다. 창마다 별도 ViewModel을 생성하지 않는다.

**Window → View Bindings**에서 등록된 UMG 바인딩을 확인하고 편집할 수 있다. 슬롯 이름·수량·단축키·장착 문구, 인벤토리 용량·실패 문구, 전리품 제목, 제작 상세·결과·버튼 활성화 상태가 ViewModel에서 연결된다. 아이콘 브러시, 등급 표시, 반복 목록, 선택 강조와 입력 제스처는 C++ 뷰 어댑터가 처리한다. 게임 규칙과 아이콘 비동기 로딩은 ViewModel·모델 계층에서 처리한다.

텍스트에 별도의 Tick 바인딩이나 직접 인벤토리를 조회하는 Blueprint 로직을 추가하지 않는다. `FieldNotify` 알림으로 화면을 갱신한다. 실제 아이템 이동·사용·장착·획득·제작은 ViewModel 명령에서 기존 서버 RPC로 전달한다.

## 이름을 유지할 위젯

배치나 스타일을 바꿀 때 아래 이름과 타입을 유지한다. C++의 `BindWidget` 및 에셋의 View Bindings가 이 이름을 사용한다. 임의로 이름·타입을 바꾸면 Blueprint 컴파일 시 누락되거나 바인딩 오류가 표시된다.

| 화면 | 연결되는 위젯 |
| --- | --- |
| 전체 인벤토리 | `WindowFrame`(Border), `SlotsPanel`·`QuickSlotsPanel`·`EquipmentPanel`(UniformGridPanel), `CapacityText`·`FeedbackText`(TextBlock), `CraftingButton`·`CloseButton`(Button) |
| 퀵슬롯 HUD | `SlotsPanel`(UniformGridPanel), `OutsideDropArea`(Border, 드래그 중 바깥 영역 입력) |
| 인벤토리 슬롯 | `SlotBorder`(Border), `ItemIcon`(Image), `ItemNameText`·`QuantityText`·`KeyText`·`EquippedText`(TextBlock), `RarityBackground`(ASItemRarityBackground) |
| 전리품 창 | `LootPanel`(월드 위치 추적 대상), `ItemList`(PanelWidget, 기본 UniformGridPanel), `LootTitleText`·`ItemCountText`·`EmptyText`(TextBlock), `CloseButton`·`HeaderCloseButton`(Button) |
| 전리품 칸 | `ItemButton`(Button), `ItemIcon`(Image), `ItemNameText`·`QuantityText`(TextBlock), `QuantityBadge`(Border), `RarityBackground`(ASItemRarityBackground) |
| 제작 | `RecipeList`·`MaterialList`(PanelWidget), `ResultIcon`(Image), `ResultFallback`·`EmptyRecipesText`·`SelectedNameText`·`SelectedCategoryText`·`WeaponStatsText`·`Details`·`OwnedResultText`·`AvailabilityText`·`Status`(TextBlock), `CraftButton`·`CloseButton`(Button) |
| 제작법·재료 행 | `NameText`·`SecondaryText`·`CategoryText`(TextBlock), `ItemIcon`(Image), `IconFallback`(TextBlock), `SelectionFrame`(Border), 제작법 행의 `SelectButton`(Button) |

기본 에셋을 복제한 화면을 쓰려면 PlayerController의 ItemInteractionComponent에서 HUD·전체 인벤토리·전리품 클래스, CraftingComponent에서 `Crafting Widget Class`를 지정한다. 반복 칸의 클래스는 인벤토리 화면의 `Slot Widget Class`, 전리품 화면의 `Entry Widget Class`에서 지정한다. 기본 에셋의 컨텍스트 이름 `InventoryVM`·`SlotVM`·`CraftingVM`·`EntryVM`도 유지한다.

## 검증

에셋의 Blueprint/MVVM 컴파일과 자동 검증 3개를 통과했다. 실제 UMG 수량 갱신과 공유 모델, 슬롯 크기 유지, 서버 경유 이동, 가방 확장, 전리품 갱신, 제작 버튼·사망·부활·결과·다시 열기, Designer 슬롯 미리보기를 확인했다. 검증용 소스·생성 도구·실행 산출물과 에디터 전용 빌드 의존성을 제거한 뒤 Editor Development 빌드도 통과했다. 기본 UI 폴더는 패키징의 Additional Asset Directories to Cook에 등록되어 있다. 별도 프로세스 멀티플레이 PIE와 수동 마우스 조작은 이 검증에 포함하지 않는다.

## 제작 브라우저

제작창의 왼쪽 목록은 카탈로그에 등록된 레시피를 근접 무기·원거리 무기·기타 무기로 묶어 표시한다. 선택하면 가운데 무기 이름·아이콘·설명·기본 공격력·공격 간격·사거리·보유 개수와 오른쪽 재료 목록이 함께 갱신된다. 능력치는 아이템 정의의 기본 값이다.

재료 행은 같은 재료를 합산하며 보유/필요 수량을 표시한다. 부족한 수량은 붉은색, 충족한 수량은 초록색으로 표시한다. 데이터 갱신 시 목록 행 전체를 다시 만들지 않는다. 제작 대기 중에는 레시피 선택과 제작 버튼을 비활성화한다.

왼쪽 행과 오른쪽 행의 모양은 각각 `WBP_CraftingRecipeEntry`, `WBP_CraftingMaterialEntry`에서 수정한다. 제작창의 Class Defaults에서 `Recipe Entry Class`·`Material Entry Class`로 복제한 행 에셋을 지정할 수 있다. 행의 선택 강조와 수량 상태 색은 Class Defaults의 Crafting → Appearance에서 조절한다.

제작 브라우저 변경 후에는 빈 카탈로그, 목록 선택·재료 합산·수량 바인딩·실제 제작·사망/부활·다시 열기, 프로젝트 카탈로그 렌더링 검증을 통과했다.

아이콘과 설명은 결과·재료의 `ASItemDefinition`에서 가져온다. 아이콘이 없는 경우에는 기본 표시를 사용한다. 제작대나 NPC 조건은 현재 게임 시스템에 추가하지 않았다.
