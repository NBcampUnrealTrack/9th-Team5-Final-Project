# 신규 제작 재료 4종

아이콘은 내장 `image_gen`으로 생성한 투명 PNG이며, 사용한 전체 프롬프트는 [icon_prompts.json](icon_prompts.json)에 보관했다. 월드 메시는 아이콘의 특징에 맞춰 별도로 모델링한 실제 3D 지오메트리다. 편집 가능한 OBJ, 생성 스크립트, 재질 설정을 함께 보관한다.

| 재료 | 아이콘과 메시의 형태 | LOD0 삼각형 | 기본 크기 cm |
| --- | --- | ---: | --- |
| 강철판 / SteelPlate | 구멍을 뚫은 강철판 2장, 파란 결속 밴드와 버클 | 1,276 | 23.1 × 19.1 × 3.2 |
| 강화섬유 / ReinforcedFiber | 심지에 감긴 황갈색 보강 직물, 어두운 뒷면과 고정 끈 | 9,312 | 25.3 × 15.5 × 15.2 |
| 정밀부품 / PrecisionParts | 금속 프레임, 황동·강철 기어, 구리 코일과 연결 핀 | 6,368 | 29.9 × 12.0 × 5.0 |
| 변이 촉매 / MutationCatalyst | 붉은 결정 시료, 유리와 금속 지지대, 노란 경고 띠 | 4,702 | 10.0 × 10.0 × 18.4 |

## 언리얼 에셋

아래 경로에서 `<Name>`은 표의 영문 이름이다.

- 아이콘: `/Game/Resources/Items/Icon/T_Icon_<Name>`
- 월드 메시: `/Game/Resources/Items/Mesh/Materials/SM_Item_<Name>`
- 신규 재질: `/Game/Resources/Items/Materials/CraftingMaterials/`
- 연결된 아이템: `/Game/ApocalypseSurvival/Items/Definitions/PDA_Item_<Name>`

각 아이템의 `Icon`과 `WorldMesh`에 연결했다. 기존 ItemId·표시 이름·설명·등급·중첩 수는 유지했다. 레시피와 드랍 테이블은 변경하지 않는다.

메시는 LOD 3단계(100%·50%·20%), 단순 Box 충돌, 바닥 중심 피벗을 사용한다. 크기는 모델 자체의 cm 단위이며 기존 드랍 액터의 배율과 회전이 추가 적용된다. 표면 재질은 기존 `M_ItemSurface`와 `M_ItemBottleGlass`를 재사용하고 붉은 결정의 재질만 별도로 추가했다. 아이콘 PNG 원본은 1254 × 1254 RGBA이며 언리얼에서는 UI 그룹·최대 512·밉맵 없음으로 설정했다.

## 원본과 재생성

- `Meshes/`: 편집 가능한 OBJ 4개와 재질 슬롯 목록 MTL
- `build_meshes.py`: 외부 패키지 없이 Python 3으로 지오메트리·UV·법선 생성
- `import_materials_unreal.py`: 언리얼 임포트·재질 연결·LOD·충돌·아이템 연결
- `render_previews_unreal.py`: 별도의 언리얼 에디터 프로세스에서 실제 메시 렌더링
- `Previews/`: 실제 언리얼 재질을 적용한 메시 미리보기
- `mesh_manifest.json`, `unreal_manifest.json`: 지오메트리 정보와 저장된 에셋 연결 정보

`build_meshes.py`를 Python 3으로 실행한 뒤 언리얼 Python에서 `import_materials_unreal.py`를 실행하면 다시 가져올 수 있다. 이 과정은 해당 신규 에셋을 덮어쓰므로 수작업으로 수정한 메시가 있으면 먼저 원본을 보관한다.

저장된 에셋의 연결·재질·LOD·충돌을 읽기 전용으로 검사하려면 언리얼 Python commandlet에서 import 스크립트에 `--verify-only`를 전달한다. 미리보기 스크립트는 별도 에디터를 `-RenderOffscreen -ExecutePythonScript=<스크립트 절대 경로>`로 실행하고 렌더링 후 해당 프로세스를 종료한다. 미리보기용 액터만 생성·삭제하며 레벨을 저장하지 않는다.
