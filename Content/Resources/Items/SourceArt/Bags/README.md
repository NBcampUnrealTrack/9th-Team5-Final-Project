# 가방 4단계 아트

| 단계 | 형태 | 아이템 정의 | 기존 추가 용량 | LOD0 삼각형 |
| --- | --- | --- | --- | --- |
| 1 | 단추 덮개가 있는 천 파우치 | PDA_Item_Bag01 | 8칸 | 3,596 |
| 2 | 앞 지퍼 주머니가 있는 작은 배낭 | PDA_Item_Bag02 | 16칸 | 5,256 |
| 3 | 덮개·버클·옆주머니가 있는 큰 배낭 | PDA_Item_Bag03 | 24칸 | 7,532 |
| 4 | 앞 주머니 두 개와 몰리 웨빙이 있는 전술 배낭 | PDA_Item_Bag04 | 32칸 | 11,008 |

아이콘은 내장 `image_gen`으로 생성한 투명 PNG이며, 원문 프롬프트는 [icon_prompts.json](icon_prompts.json)에 보관했다. 모델은 Blender로 별도 제작한 Static Mesh다. 아이콘의 형태·재질·단계별 특징을 모델에도 적용했다.

모든 가방 아트와 제작 원본은 `Content/Resources/Items` 아래에 있다. 이 문서와 Blender·FBX·베이크 원본의 위치는 `Content/Resources/Items/SourceArt/Bags`다. 아이콘은 가방을 세운 상태에서 정면을 주로 보여 주고, 오른쪽 위에서 약 12도 내려다보는 구도를 사용한다. 이미지 자체는 기울이지 않는다.

## 언리얼에서 사용하는 에셋

- 아이콘: `/Game/Resources/Items/Icon/T_Icon_Bag01` ~ `T_Icon_Bag04`
- 월드 메시: `/Game/Resources/Items/Mesh/Bags/SM_Bag01` ~ `SM_Bag04`
- 공용 재질: `/Game/Resources/Items/Materials/Bags/M_BagFabric`
- 재질 인스턴스: 같은 폴더의 `MI_Bag01` ~ `MI_Bag04`
- 메시 텍스처: `/Game/Resources/Items/Textures/Bags/T_BagXX_BaseColor`, `Normal`, `ORM`

기존 `PDA_Item_Bag01` ~ `PDA_Item_Bag04`의 `Icon`, `WorldMesh`에 연결했다. ItemId, 등급, 추가 슬롯 수, 최대 스택 수는 유지한다.

각 메시에는 재질 슬롯 1개, LOD 3개(100%·50%·20%), 단순 Box 충돌을 설정했다. 월드 메시는 드롭 시 앞면이 위를 향하도록 눕혀져 있고, 어깨끈도 바닥에 놓인 형태로 접었다. 피벗은 바닥 중심이다. 기존 드롭 액터의 배율과 회전은 그대로 적용된다. 캐릭터 착용용 스킨·리깅은 포함하지 않는다.

아이콘은 원본 해상도를 보존하고 언리얼 UI 텍스처의 최대 크기를 512로 설정했다. 메시 재질 맵은 1024×1024다. ORM은 R=Ambient Occlusion, G=Roughness, B=Metallic이며 sRGB를 끈다. 노멀 원본은 OpenGL 방식으로, 언리얼 텍스처에서 Green Channel Flip을 켠다.

## 원본과 재생성

- `Bags.blend`: 4종의 편집 가능한 모델 컬렉션과 베이크한 월드 메시
- `Meshes/`: 가져오기 원본 FBX 4개
- `Textures/`: 베이크한 PBR 맵 12개
- `Previews/`: 기본 형태와 실제 베이크 재질의 월드 메시 미리보기
- `build_bags.py`: 모델 생성·미리보기 렌더·UV·베이크·FBX 내보내기
- `import_bags_unreal.py`: 언리얼 가져오기·재질·LOD·충돌·아이템 연결
- `mesh_manifest.json`, `unreal_manifest.json`: 메시 정보와 연결 경로

Blender 4.5 LTS에서 다음 순서로 실행한다. 첫 명령은 편집용 모델을 새로 만들므로 수작업으로 수정한 원본을 보관한 뒤 사용한다.

```powershell
blender --background --factory-startup --python build_bags.py
blender --background --factory-startup --python build_bags.py -- --finalize
```

수작업으로 `Bags.blend`를 수정한 경우에는 두 번째 명령만 실행하면 편집한 원본을 베이크하고 내보낸다. 언리얼 Python 콘솔에서는 `import_bags_unreal.py`를 실행해 에셋을 갱신할 수 있다. 이 스크립트는 기존 가방 정의의 아이콘과 월드 메시만 변경한다.
