# Sheet Asset Importer 사용 안내

이 플러그인은 Google Sheets 또는 CSV의 행을 Unreal DataTable 행이나 DataAsset로 가져옵니다. 아이템 PDA에서 밸런싱 수치를 시트로 관리하고, 아이콘·메시·GameplayEffect 같은 에셋 참조와 고정 설정은 에디터에서 관리하는 흐름을 기준으로 설명합니다.

## 기본 개념

- **가져오기 프로필**은 시트 주소, 대상 에셋 클래스, 출력 폴더, 열 매핑, 검증 옵션을 저장하는 에디터 DataAsset입니다.
- **시트 가져오기 계획**은 여러 프로필을 가져올 순서대로 묶습니다. 단일 시트만 가져올 때는 계획이 필요 없습니다.
- **검증**은 미리보기입니다. 대상 에셋을 수정하거나 저장하지 않고, 가져올 행과 오류만 확인합니다.
- **가져오기**는 검증을 통과한 데이터를 실제 대상에 반영합니다.

시트 열 안내에 표시된 이름은 사용할 수 있는 열의 예시이며 전부 필요한 것은 아닙니다. 가져올 데이터에 필요한 열만 두세요.

## Google Sheets 준비

첫 행은 열 이름이어야 하며, 그 아래 각 행이 하나의 아이템입니다. DataAsset을 만들 때 `Name` 열은 접두사 뒤에 붙일 에셋 이름으로 사용합니다.

예를 들어 프로필의 에셋 이름 접두사가 `PDA_Item_`이면 `Name` 값 `Pistol`은 `PDA_Item_Pistol` 에셋을 대상으로 합니다. `Name` 값에는 점, 슬래시처럼 에셋 이름에 쓸 수 없는 문자를 넣지 말고, 각 행에서 값이 중복되지 않게 하세요. 아이템의 게임 내 식별자와 화면 표시 이름은 별도 열로 둘 수 있습니다.

## 프로필 만들기

1. 콘텐츠 브라우저에서 `Miscellaneous > Data Asset`을 선택하고 `SheetImportProfile`을 만듭니다.
2. 프로필을 열어 시트 주소와 가져오기 대상을 지정합니다.
3. DataAsset 대상이면 DataAsset 클래스, 출력 폴더, 에셋 이름 접두사를 설정합니다.
4. `KeyColumn`에 `Name`을 입력합니다. 비워두면 플러그인이 `Name`, `ID`, `ItemId`, `RowName`, `AssetName` 순으로 찾으므로, 의도한 열을 확실히 쓰려면 직접 지정하세요.
5. 열 매핑 방식을 선택합니다.
6. **미매핑 값 보존**을 켭니다. 기존 에셋을 갱신할 때 시트에 연결하지 않은 에디터 값이 보존됩니다.
7. 먼저 **검증**을 실행하고, 오류가 없으면 **가져오기**를 실행합니다.

Google Sheets CSV 방식은 시트의 CSV 다운로드가 가능한 공유 상태여야 합니다. 비공개 시트는 Google Sheets API 방식과 OAuth 액세스 토큰 환경 변수를 사용해야 합니다. 토큰 자체를 시트나 프로필에 적지 마세요.

## 아이템 PDA: 수치만 시트에서 관리하기

기존 PDA를 시트와 연결한 뒤 수치만 관리하려면 다음 구성이 적합합니다.

1. 각 PDA를 먼저 에디터에서 만들고, 프로필의 출력 폴더와 접두사에 맞춰 이름을 정합니다.
2. `ItemType`, `WeaponType`, `AttackProfile`, `UseEffects`, 아이콘, 메시 등 시트에서 관리하지 않을 값은 PDA에서 설정합니다.
3. 시트에는 `Name`과 조정할 수치 열만 둡니다. 예: `Name`, `BaseDamage`, `AttackInterval`, `AttackRange`.
4. **Auto Map Columns**를 끄고, `Column Mappings`에 수치 열만 명시적으로 연결합니다.
5. `Name`은 에셋 식별에 사용하므로 열 매핑에 추가할 필요가 없습니다. 게임 내 `ItemId`를 시트에서 관리하지 않는다면 해당 열도 생략할 수 있습니다.
6. 기존 설정 보존을 위해 **Preserve Unmapped Values**를 켭니다.

제작 재료는 `ItemType`을 `CraftingMaterial`로 설정하고 `MaxStackSize`를 원하는 스택 크기로 지정합니다. 재료는 직접 사용하거나 장착하는 아이템이 아니며, 레시피/제작 기능은 별도의 crafting system에서 추가해야 합니다. 무기·탄약·소모품과 같은 시트에 둘 수 있지만 `ItemType` 열 값은 각 행에서 정확히 입력하세요.

Auto Map Columns가 켜져 있으면 직접 매핑하지 않은 열 중 시트 열 이름과 같은 에셋 속성이 자동으로 연결됩니다. 명시한 매핑의 열과 대상 속성은 우선 적용하며, 같은 대상에 자동 매핑을 중복 적용하지 않습니다. 예를 들어 시트에 `AttackProfile` 열이 남아 있으면 그 열도 자동 매핑 대상이 됩니다. 에디터에서 지정할 값을 유지하려면 해당 열을 시트에서 제거하고, 자동 매핑 대신 수치 열만 명시적으로 매핑하세요. 빈 셀 외에 `None`, `null` 같은 텍스트를 넣으면 빈 값이 아니라 실제 입력으로 처리될 수 있습니다.

새 PDA를 만들 때는 에셋이 먼저 존재해야 하고, `Name`과 접두사로 만들어지는 경로가 정확히 일치해야 합니다. 필수 에디터 설정까지 마친 뒤 저장하고 가져오기를 검증하세요. 기존 PDA가 없으면 새 에셋이 만들어지지만, 클래스 기본값만 가진 새 에셋은 아이템의 필수 데이터 검증에서 실패할 수 있습니다.

## 제작 레시피: 에셋 경로 대신 ItemId로 참조하기

레시피 시트의 결과 아이템과 재료는 아이템 정의에 저장된 `ItemId`로 참조할 수 있습니다. 에셋 파일 이름이나 화면 표시 이름은 ID가 아닙니다. 아래 예시는 `ItemId`가 각각 `Iron`, `Wood`, `Rifle`인 아이템 정의가 이미 존재한다고 가정합니다.

| Name | RecipeId | DisplayName | Result | Ingredients |
| --- | --- | --- | --- | --- |
| Rifle | Craft_Rifle | 소총 제작 | Rifle | `[{"ItemDefinition":"Iron","Quantity":3},{"ItemDefinition":"Wood","Quantity":2}]` |

`Ingredients`의 배열 전체를 하나의 셀에 입력합니다. 배열을 다시 가져오면 기존 재료 목록 전체를 대체합니다. 재료는 `CraftingMaterial`, 결과는 `Weapon`, `Bag`, `CraftingMaterial`, `Ammo`, `Consumable`을 지원합니다. 무기와 가방의 `MaxStackSize`는 1이어야 합니다. 제작 수량은 `Result`에 `{"ItemDefinition":"Ammo","Quantity":20}`처럼 입력하며, 중첩 한도보다 많은 양도 여러 묶음으로 지급할 수 있습니다.

1. `SheetImportProfile`의 생성 방식을 `DataAssets`, 클래스를 `ASCraftingRecipeDefinition`으로 설정합니다.
2. 출력 폴더는 기존 레시피 폴더, 접두사는 `DA_Recipe_`, 식별자 열은 `Name`으로 지정합니다. 예시 행은 `DA_Recipe_Rifle`을 생성·갱신합니다.
3. **2. 만들 에셋 → 에셋 참조 ID 속성**에 `ItemId`를 입력합니다. **참조 ID 검색 폴더**는 아이템 정의가 있는 콘텐츠 폴더로 지정합니다. 기본 `/Game`은 전체 게임 콘텐츠를 검색합니다. 폴더는 프로필에서 한 번만 설정하며 시트에 에셋 경로를 적을 필요가 없습니다.
4. **Auto Map Columns**를 켭니다. 아래 이름을 쓰면 `Column Mappings`를 비워도 자동 연결됩니다. `Name`은 에셋 식별에만 사용합니다. 다른 열 이름이 필요할 때는 그 열만 명시적으로 매핑하고 나머지는 자동 연결할 수 있습니다.

| 시트 열 | 자동 연결 대상 | 입력 형식 |
| --- | --- | --- |
| RecipeId | RecipeId | 레시피 ID |
| DisplayName | DisplayName | 표시 이름 |
| Result | Result의 아이템 참조 | 아이템 ID 하나 |
| Ingredients | Ingredients | 재료 JSON 배열, 자동 인식 |

`Result`에 `Rifle`처럼 ID 하나를 쓰면 아이템 참조만 갱신하며 수량은 기존 값 또는 기본값 1을 유지합니다. 기존의 `{"ItemDefinition":"Rifle","Quantity":1}` 형식으로 구조체 전체를 입력하는 방식도 지원합니다. 열 이름을 `ResultItem`으로 사용하려면 해당 열만 `Result.ItemDefinition`으로 매핑하세요.

5. **Preserve Unmapped Values**, **Validate Assets**, **Validate References**를 켜고 **검증 → 가져오기 → 저장** 순서로 적용합니다.
6. 새로 생성한 레시피는 사용할 `DA_CraftingCatalog`의 `Recipes`에 등록합니다. 이 기능은 카탈로그를 자동으로 변경하지 않습니다.

ID는 `FName`처럼 대소문자를 구분하지 않습니다. 검색 폴더 안의 같은 종류 DataAsset에서 요청한 ID가 중복되거나 존재하지 않으면 행·열·ID가 포함된 오류와 함께 해당 프로필의 가져오기를 중단합니다. 모든 행을 검증하기 전에 대상 에셋을 변경하지 않으므로, 뒤쪽 행의 ID가 잘못되어도 앞쪽 행만 반영되지 않습니다. 에셋 위치나 파일 이름이 바뀌어도 검색 폴더 안에 있고 ID가 유지되면 다시 찾습니다.

가져오기 계획의 미리보기에서는 앞선 프로필의 임시 결과를 먼저 참조합니다. 같은 에셋의 ID가 변경되었다면 이전에 저장된 ID로 잘못 연결하지 않습니다. 신규 아이템과 레시피를 함께 가져올 때는 아이템 프로필을 먼저 실행하도록 순서를 지정하세요.

이 기능은 에디터에서 가져올 때만 작동합니다. 게임에는 기존과 동일한 아이템 참조가 저장됩니다. 기존 경로 입력도 유지되며, **에셋 참조 ID 속성**을 비워두면 기존 경로 방식으로만 처리합니다. 플러그인은 게임 아이템 클래스에 의존하지 않으며, 다른 DataAsset에도 `FName` 또는 `FString` ID 속성을 지정해 사용할 수 있습니다.

## 소모품과 회복 아이템

`UseEffects`는 GameplayEffect 클래스 참조 배열이고, `EffectMagnitudes`는 GameplayTag와 float 수치의 맵입니다. `UseEffects`를 에디터에서 지정하고 `EffectMagnitudes`만 시트에서 밸런싱할 수 있습니다.

예를 들어 `EffectMagnitudes` 열의 한 셀에 다음 JSON을 넣으면 `Data.Item.Heal` 태그의 SetByCaller 값이 30이 됩니다.

```json
{"Data.Item.Heal":30}
```

다른 태그도 함께 적용한다면 맵 전체를 한 셀에 적습니다.

```json
{"Data.Item.Heal":30,"Data.Item.Mana":10}
```

`EffectMagnitudes`를 가져오면 맵 전체가 대체되므로, 유지할 태그와 수치를 모두 포함해야 합니다. `RecoveryResources`처럼 바뀌지 않는 설정은 시트에서 열을 제거하고 PDA에서 지정하면 됩니다. 배열을 시트에서 가져와야 한다면 JSON 배열 형식(예: `["Health"]`)을 사용하고, 실제로 검증해 보세요.

## 검증과 가져오기 결과 확인

- **검증 통과**: 대상 경로, 생성/갱신 개수를 확인한 뒤 가져오기를 누릅니다.
- **검증 실패**: Output Log에서 `LogSheetAssetImporter`를 찾아 첫 오류를 확인합니다. 오류가 난 프로필에서 처리가 멈춥니다.
- 예: `Weapons require an AttackProfile`은 대상 PDA의 공격 프로필이 비어 있거나 시트 값으로 지워졌다는 뜻입니다.
- 예: `Cannot parse value as EASItemType`은 enum 이름이 코드에 정의된 값과 다르다는 뜻입니다. `Consumable`처럼 정확한 철자를 사용합니다.
- 예: `RecoveryResources ... Cannot parse value as TArray`는 배열 형식이 아니거나 잘못된 형식으로 입력됐다는 뜻입니다.
- 로그에 `0 rows, 0 new / 0 updated`가 표시되면 검증 또는 입력 파싱 단계에서 멈춘 것이므로 대상 PDA는 갱신되지 않았습니다.

자동 저장을 끄면 가져온 에셋은 에디터에서 검토하고 직접 저장해야 합니다. 가져오기 계획에서 앞선 프로필이 이미 반영된 뒤 다음 프로필이 실패하면, 앞선 변경은 자동으로 되돌아가지 않습니다.

## 가져오기 계획을 쓸 때

한 번에 여러 프로필을 처리하거나 참조 데이터가 먼저 준비되어야 할 때 사용합니다. 예를 들어 공격 프로필 DataAsset을 먼저 가져오고 이를 참조하는 아이템 데이터를 나중에 가져오도록 순서를 정할 수 있습니다. 단일 아이템 시트만 갱신한다면 프로필 하나만 실행하면 됩니다.
