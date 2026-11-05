# 개인 퀘스트 루팅과 보관함

퀘스트 아이템은 ASItemDefinition의 ItemType = Quest로 정의한다. 기존 상자·루팅 창·아이템 드롭을 사용하며 일반 가방에는 넣지 않는다. 서버가 수락 상태와 목표 순서를 검사한 뒤 개인 퀘스트 보관함에 기록한다.

## 퀘스트 데이터 편집

모든 퀘스트의 공통 완료 조건은 **개별 목표 달성 → FinalBossDefeated → NPC 보고**다. 개별 목표가 모두 끝나도 진행 중 상태이며, 그 이후 최종보스를 처치해야 보고할 수 있다. 수락 전이나 목표 달성 전에 잡은 최종보스는 나중에 소급하지 않는다.

Objectives에는 챕터별 추가 행동만 넣는다. 최종보스 목표를 퀘스트마다 중복 추가할 필요가 없다. Objectives를 비우면 수락 후 최종보스 처치만 요구하는 퀘스트가 된다. 퀘스트 탭에는 공통 최종보스 목표가 자동으로 표시된다.

- Project Settings → Game → Quests → Catalog에서 전체 퀘스트 카탈로그를 지정한다. 현재는 `/Game/ApocalypseSurvival/Quests/DA_QuestCatalog`를 사용한다. 새 퀘스트를 코드의 목록에 추가하지 않는다.
- 카탈로그의 Quests 배열에 ASQuestDefinition 데이터 에셋을 등록한다. 이 카탈로그가 유일한 등록 경로이며 PlayerState 컴포넌트에는 퀘스트를 따로 등록하지 않는다. 같은 우선순위에서는 배열 순서대로 대화 후보를 선택한다.
- 퀘스트 에셋에서 QuestId, 제목/설명, Objectives, 선행 퀘스트 ID, 순차 진행, 이번 판 목표 초기화 여부, 경험치, 대사를 설정한다.
- OfferNpcId는 수락할 NPC, TurnInNpcId는 완료할 NPC다. OfferNpcId가 비어 있으면 TurnInNpcId를 사용해 기존 데이터를 유지한다. 두 NPC를 다르게 지정할 수 있다.
- 퀘스트 에셋의 Auto Accept는 선행 퀘스트가 없는 최초 시작용 퀘스트에만 적용한다. 다음 챕터는 이전 챕터 보고 후 NPC에게 직접 수락하며, 재접속해도 자동 수락하지 않는다. 선행 퀘스트가 있는 의뢰는 Auto Accept가 켜져 있어도 수동 수락한다. 과거 StartingQuests/QuestCatalogAssets는 숨겨진 직렬화 호환 데이터이며 진행에 사용하지 않는다.
- Dialogue Priority는 숫자가 클수록 먼저 선택한다. 완료 가능한 퀘스트 → 새 의뢰 → 진행 안내 순서 안에서 적용하며, 같은 NPC에 여러 의뢰가 있어도 진행 안내가 새 의뢰를 막지 않는다.
- 퀘스트 전체의 Completion Items는 개별 목표 달성 후 최종보스를 처치했을 때 보관함에 표시되는 제출용 아이템이다. 기존 BossClearItems 필드와 저장 데이터는 그대로 호환된다. 개별 Objectives 안의 CompletionItems는 해당 행동 목표를 달성한 즉시 받는 아이템이며, 이것을 받아도 최종보스를 처치하기 전에는 퀘스트를 완료할 수 없다.
- 카탈로그는 중복 QuestId, 누락된 선행 퀘스트, 순환 선행 관계와 잘못된 목표를 검사한다. 잘못된 카탈로그나 알 수 없는 저장 퀘스트를 만나면 기존 세이브를 덮어쓰지 않고 오류를 알린다.
- 진행 가능한 목표와 완료 여부는 FASQuestProgress의 공통 판정을 사용하며, 아이템 획득 가능 여부와 실제 이벤트 적용도 동일한 조건을 검사한다. 순차 목표는 이벤트 적용 전의 대상만 진행하므로 한 번의 획득으로 다음 목표까지 완료하지 않는다.
- 수락·보고·복원 등 하나의 상태 변경 안에서 발생한 알림은 마지막에 한 번 전달한다. 프로필 저장과 UI 갱신도 최종 상태를 기준으로 실행된다.

### 새 퀘스트 추가 순서

1. 콘텐츠 브라우저에서 Data Asset → ASQuestDefinition으로 생성하거나 기존 챕터를 복제한다. 새 QuestId와 각 ObjectiveId를 지정한다.
2. 각 목표의 Objective Type을 선택하고 표시되는 대상과 RequiredCount를 설정한다. Collect Item은 ItemDefinition, Enter Area와 Interact with NPC는 TargetTag, Defeat Monster는 TargetMonsterDefinition과 KillCredit을 사용한다. EventTag는 종류에서 자동 결정되며 수집 대상 태그는 아이템에서 읽는다. 별도 서버 이벤트가 필요할 때만 Custom Event (Advanced)를 선택한다.
3. NPC ID, 선행 QuestId, 대사와 진행 방식을 설정하고 DA_QuestCatalog의 Quests에 등록한다.
4. 맵의 NPC·트리거·상자에 대상과 아이템을 연결한다. 몬스터 처치 목표는 퀘스트의 TargetMonsterDefinition에서 기존 몬스터 데이터 에셋을 선택하고, 목표의 CompletionItems에 개인 보관함으로 자동 지급할 아이템을 넣는다. 몬스터나 드롭 테이블을 수정하지 않는다.
5. 기존 퀘스트의 QuestId/ObjectiveId는 저장 키이므로 내용이나 순서만 바꿀 때 유지한다. ID를 바꾸거나 등록된 과거 퀘스트를 삭제하려면 별도 저장 마이그레이션이 필요하다.

| 목표 | EventTag | 대상 설정 |
|---|---|---|
| 지역 진입 | Quest.Event.ZoneEntered | ASQuestArea의 TargetTag |
| 퀘스트 아이템 획득 | Quest.Event.ItemCollected | 아이템의 QuestTargetTag 및 목표의 ItemDefinition |
| 기존 몬스터 처치 및 개인 아이템 지급 | Quest.Event.EnemyKilled | 목표의 TargetMonsterDefinition / CompletionItems / KillCredit |
| NPC 상호작용 | Quest.Event.Interacted | ASQuestNpcDefinition의 InteractionTargetTag |
| 최종보스 처치 | Quest.Event.FinalBossDefeated | 모든 퀘스트에 공통 필수 조건으로 자동 적용 |

새 행동 유형은 신뢰할 수 있는 서버 게임플레이 코드/BP에서 ASQuestComponent.HandleQuestEvent를 호출해 연결한다. 새로운 대상이나 기존 행동 조합으로 퀘스트를 만드는 데에는 C++ 수정이 필요 없다.

## 플레이 흐름

- 1챕터: 연구원에게 수락 → 캠프 상자에서 일지 가져오기 → 같은 판의 최종보스 처치 및 ID 카드 자동 회수 → 연구원 보고.
- 2챕터: 1챕터 보고 → 2챕터 수락 → 실험실 가장 안쪽 진입 → 기존 엘리트 처치 및 실험기록 자동 획득 → 같은 판의 최종보스 처치 → 연구원 보고.
- 상자를 여는 것만으로는 진행되지 않는다. 기존 루팅 창에서 아이템을 가져와야 한다.
- 상자 아이템은 각 플레이어가 독립적으로 획득한다. 몬스터 처치 보상은 각 플레이어의 수락 상태와 선행 목표를 확인한 뒤 개인 퀘스트 보관함에 자동 기록한다.
- 자신의 목표가 완료되면 자신의 루팅 창에서 해당 퀘스트 아이템이 사라진다. 일반 아이템의 공유 루팅 규칙은 유지한다.
- 일반 가방 용량을 차지하지 않고, 퀵슬롯·장착·제작·버리기·거래의 대상이 아니다.
- 인벤토리 상단 퀘스트 탭에서 아이템 획득 상태와 목표 진행도를 확인한다. UI 안내는 특정 NPC나 지역에 고정하지 않는다.
- 제작 탭은 기존 제작 화면에 연결한다. 모듈 탭은 향후 화면 연결을 위해 비활성 상태로 제공한다.

## 저장과 소유권

PlayerState의 ASQuestComponent가 퀘스트 보관함을 소유한다. 아이템 데이터 에셋과 목표 수량이 보관함의 원장이다. 동일한 획득 사실을 두 곳에 중복 저장하지 않으므로 목표와 보관함이 어긋나지 않는다. 기존 개인 프로필 저장·소유자 전용 복제를 재사용한다.

- 진행 중 증거는 이번 판의 임시 보관이다. 패배하거나 생환 전에 종료하면 증거와 미완료 목표를 초기화하며 수락 상태는 유지한다.
- 조건을 갖추어 최종보스를 처치하면 증거가 저장되고 맵 이동/재실행 후에도 연구원에게 전달할 수 있다.
- 저장된 개별 목표 수치가 전부 채워져 있어도 진행 중 상태를 완료 상태로 승격하지 않는다. 실제 생환하여 보고 대기가 된 기록은 유지한다. 이전 데이터의 명시적 최종보스 목표를 제거해도 나머지 목표 ID와 보고 대기/제출 상태는 그대로 복원한다.
- 보고 완료 시 해당 챕터 아이템을 제출 처리하고 다음 챕터를 해금한다. 중복 제출은 거절한다.
- 이전 저장 파일의 목표 진행도는 같은 QuestId/ObjectiveId로 복원한다. 2챕터의 실험기록 목표 ID도 유지하므로 이미 획득한 기록을 다시 지급하지 않으며 세이브를 삭제할 필요가 없다.

## 배치 및 데이터

1. PlayerState는 AASPlayerState 계열을 사용한다. QuestComponent·QuestDialogueComponent·PlayerProfileComponent는 기본 생성되므로 BP에 중복 추가하지 않는다. 퀘스트 상태와 저장은 QuestComponent, 대화 세션·서버 버튼 검증·개인 위젯은 QuestDialogueComponent가 담당한다. 기존 퀘스트 컴포넌트의 대화 호출은 호환용 전달 함수로 남아 있다.
2. Quests/NPC/BP_QuestNpc_Researcher를 배치한다. 연결된 DA_QuestNpc_Researcher에서 NPC ID·표시 이름·일반 대사·상호작용 목표 태그를 편집한다. 새 NPC는 ASQuestNpcDefinition 에셋을 만들고 ASQuestNpc BP의 Definition에 지정한다.
3. 세 캠프에 Quests/World/BP_QuestChest_ExpeditionCamp를 하나씩 배치한다. 이 BP에 일지와 RandomSelectionGroup = ExpeditionCamps가 설정되어 있다. 서버가 판마다 한 곳만 선택한다. 새 상자는 ASQuestLootChest BP에서 QuestItem, QuestItemQuantity, ChestDisplayName과 선택 그룹을 직접 지정한다. 기본 네이티브 클래스에는 특정 일지가 들어 있지 않다.
4. 실험실 가장 안쪽에 Quests/World/BP_QuestArea_Laboratory 트리거를 배치한다. 새 지역은 ASQuestArea BP나 배치 인스턴스의 TargetTag를 목표와 일치시킨다. 기본 클래스는 실험실로 고정되어 있지 않다.
5. 기존 엘리트와 보스를 그대로 사용한다. 퀘스트 목표의 TargetMonsterDefinition에서 대상 몬스터의 기존 ASMonsterDefinition을 선택한다. 2챕터는 DA_EliteMonster를 대상으로 하고 목표 완료 시 실험기록 1개를 자동 지급한다. 몬스터의 BP·컴포넌트·드롭 테이블에 퀘스트 설정을 넣지 않는다.
6. 일반 상자에도 InitialLoot에 Quest 아이템을 넣을 수 있다. 해당 아이템만 개인 보관함으로 라우팅된다.

아이템 에셋 경로: /Game/ApocalypseSurvival/Quests/Items. 일지는 수집 목표의 ItemDefinition과 아이템의 QuestTargetTag로 연결한다. 실험기록은 처치 목표의 CompletionItems에, ID 카드는 1챕터 전체의 Completion Items(BossClearItems)에 설정한다. 대사는 기존 퀘스트 에셋에서 수정한다.

새 퀘스트 상자는 ASQuestLootChest를 사용한다. 아이템 에셋과 퀘스트 목표 연결은 에디터에서 직접 수정할 수 있다.

## 몬스터 처치와 자동 지급

- 퀘스트 Objectives에 Objective Type = Defeat Monster를 지정한다. TargetMonsterDefinition은 기존 몬스터 데이터 에셋, RequiredCount는 필요한 처치 수다. 수집용 ItemDefinition과 TargetTag는 숨겨지며 판정에 사용되지 않는다. TargetMonsterDefinition을 비우면 모든 몬스터를 센다.
- 같은 목표의 CompletionItems에 지급할 퀘스트 아이템과 수량을 넣는다. 필요한 처치 수를 모두 채운 순간 개인 보관함에 한 번 기록되며, 월드 아이템이나 일반 인벤토리 슬롯을 만들지 않는다.
- KillCredit = SharedRun은 같은 판에서 해당 목표를 진행 중인 플레이어가 각자 처치를 인정받는다. KillerOnly는 막타를 친 플레이어만 인정한다. 2챕터는 SharedRun으로 설정되어 있다.
- 수락하지 않았거나 선행 목표가 남았거나 이미 처치 목표를 완료/제출했다면 지급하지 않는다. 수락 전에 잡은 몬스터는 나중에 소급하지 않는다. 수집 이벤트나 바닥 아이템으로 처치 목표를 우회할 수 없다.
- Actor 몬스터와 기존 MonsterDefinition을 공유하는 Mass 몬스터가 공통 서버 처치 알림을 사용한다. Mass는 처리 중 콜백으로 Entity 상태를 바꾸지 않도록 다음 틱에 알린다.
- 아이템 보관 여부는 목표 완료 기록으로 복원하므로 목표와 아이템 지급이 어긋나지 않는다. 판 실패, 생환 저장, NPC 제출 규칙도 기존과 같다.
- 일반 몬스터의 드롭 테이블과 확률은 별도 시스템으로 유지된다. 퀘스트를 위해 드롭 테이블을 만들거나 몬스터 데이터를 복제하지 않는다.

플레이 흐름: 상자 클릭 → 일지 루팅 → 인벤토리의 퀘스트 탭에서 일지 1/1과 최종보스 목표 확인 → 생환 → 연구원에게 보고. 로비 연결은 보류 상태다.

## 공통 완료 조건 테스트

1. 수락 후 개별 목표를 달성한다. 퀘스트 탭은 최종보스 처치 대기를 표시해야 하고, NPC 보고는 아직 불가능해야 한다.
2. 마지막 보스 날에 기존 스폰 시스템으로 나온 최종보스를 처치한다. 생환/완료 가능 상태로 바뀌고 퀘스트 전체의 완료 아이템이 지급되어야 한다.
3. 맵 재진입 후 NPC에게 보고한다. 다음 챕터는 새로 수락한 이후의 최종보스 처치를 요구하며, 이전 챕터의 처치를 재사용하지 않는다.
4. 빠르게 검사하려면 테스트용 게임모드 BP에서 BossDays를 [1]로 두고 테스트맵 GameMode Override에 지정한다. 같은 공통 완료 처리를 1일차 밤에 확인할 수 있다.
