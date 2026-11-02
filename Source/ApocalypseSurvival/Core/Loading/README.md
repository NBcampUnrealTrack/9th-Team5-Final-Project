# 공용 로딩 화면

이 시스템은 에셋을 로딩하지 않습니다. 각 기능이 기존 비동기 로딩 코드를 그대로 사용하고,
공용 화면에는 준비 작업의 시작·완료·실패만 알려줍니다. 에셋 참조와 로딩 핸들 수명도 호출자가 관리합니다.
기본 실행 맵은 MainMenu_Proto이며, 로비의 게임 시작은 실제 Loading_Proto 레벨을 거칩니다.
로딩 프로필이나 RequiredAssets 목록을 등록할 필요는 없습니다.

```text
로비에서 캐릭터 선택 완료
→ Loading_Proto로 서버·클라이언트 모두 이동
→ 로딩 레벨 안에서 OnLoadingStarted 호출
→ 각 팀원의 비동기 작업 + PSO 준비 완료
→ 서버가 모든 참가자의 준비 완료를 확인
→ 로딩 레벨을 떠날 때 화면 종료
→ 목적지 게임 레벨로 모두 이동 및 플레이
```

Loading_Proto는 ASLoadingLevelGameMode를 사용하는 독립적인 작은 맵입니다.
엔진이 내부적으로 사용하는 Seamless Travel의 중간 월드와는 별개입니다.
`Core/Loading/DA_LoadingDefinition`에서 `Loading Map`, `Default Game Map`, `Timeout Seconds`를 지정합니다.
맵 참조는 소프트 참조이므로 DA를 읽어도 맵 자체가 로딩되지는 않습니다.
`Core/Framework/BP_GameInstance`의 Class Defaults → Loading → `Loading Definition`에 사용할 DA를 연결합니다.
게임이 사용하는 GameInstance는 이미 이 Blueprint로 연결되어 있습니다. 맵 전환 중에도 같은 DA를 유지합니다.
로비의 게임 목적지도 DA의 `Default Game Map`을 사용합니다. C++이나 별도 프로젝트 설정에 맵 경로를 중복 지정하지 않습니다.
진입 시 필요한 맵 설정이 없거나 잘못되어 있으면 이동을 중단하고 오류를 알립니다.

## 팀원이 사용하는 API

Blueprint: Get Game Instance Subsystem → ASLoadingFlowSubsystem.

1. `BeginTask(Label, Owner)`로 작업을 등록하고 반환된 TaskId를 저장합니다.
2. 자신의 비동기 로딩을 시작합니다. 기존 StreamableManager, AssetManager, 아이템 전용 로더 등을 그대로 사용합니다.
3. 완료 콜백에서 실제 성공 여부를 확인합니다.
4. 성공하면 `CompleteTask(TaskId)`, 실패하면 `FailTask(TaskId, Error)`를 호출합니다.

BeginTask는 로딩 레벨의 준비 단계에서만 작업을 등록합니다. 다른 레벨에서는 무효 TaskId를 반환하며 화면을 열지 않습니다.
여러 팀원이 등록한 작업을 모두 기다리고, 서버의 전체 준비 확인 후 로딩 레벨을 떠날 때 화면을 종료합니다.
게임 레벨에 진입한 뒤 추가 비동기 요청이나 PSO 때문에 로딩 화면을 다시 표시하거나 유지하지 않습니다.
이미 끝나거나 취소된 TaskId는 새 작업에 영향을 주지 않습니다.

```cpp
#include "Core/Loading/ASLoadingFlowSubsystem.h"

// OnLoadingStarted에 연결한 함수 안에서:
auto* Flow = GetGameInstance()->GetSubsystem<UASLoadingFlowSubsystem>();
const FGuid Task = Flow->BeginTask(FText::FromString(TEXT("Preparing items...")), this);

// 여기서 자신이 관리하는 로더로 비동기 요청을 시작합니다.
// 완료 콜백에서, 실제 로딩 성공 여부를 확인한 다음:
Flow->CompleteTask(Task);
// 실패한 경우에는 위 완료 호출 대신:
// Flow->FailTask(Task, FText::FromString(TEXT("Could not prepare items.")));
```

콜백에서는 Flow를 약한 참조로 보관하거나 UObject-bound delegate를 사용해 수명 종료에 대비하세요.
모든 화면 API는 게임 스레드에서 호출합니다. 워커 스레드의 완료 통지는 게임 스레드로 전달하세요.
Owner가 사라지면 해당 필수 작업은 실패 처리됩니다. 맵 전환을 넘어가는 작업은 이전 맵의
Actor 대신 GameInstanceSubsystem 등 전환을 넘어 살아 있는 객체를 Owner로 사용하세요.
같은 시스템에서 요청이 겹칠 수 있다면 단일 멤버 ID를 덮어쓰기보다 요청별 TaskId를 콜백에 보관하세요.

## 로딩 시작 시점에 팀별 작업 실행

GameInstanceSubsystem 같은 오래 살아 있는 객체, 또는 로딩 레벨 Actor의 BeginPlay에서
`OnLoadingStarted`에 바인딩합니다. 준비 이벤트는 실제 로딩 레벨에 도착한 다음 호출됩니다.
`IsPreparingInLoadingLevel()`로 이 단계인지 확인하고, `GetDestinationMap()`으로 목적지를 확인할 수 있습니다.
서브시스템 Initialize에서 구독한다면 `Collection.InitializeDependency<UASLoadingFlowSubsystem>()`를
먼저 호출합니다. 로딩 레벨의 BeginPlay에서는 이벤트를 구독하고, 이벤트 안에서 작업을 등록하세요.
비동기 결과를 게임 레벨에서도 사용할 경우 핸들과 준비된 결과를 GameInstanceSubsystem 등에 유지하세요.

```text
OnLoadingStarted
  → 내 기능이 이번 진입에서 준비할 것이 있는지 확인
  → BeginTask("내 기능 준비 중", Self)
  → 반환 ID 저장
  → 내 비동기 요청 시작
  → 완료 콜백에서 CompleteTask 또는 FailTask
```

이벤트 안에서는 비동기 요청을 시작하기 전에 작업부터 등록하세요. 이벤트는 하나의 로딩 흐름당
한 번 호출됩니다. 이벤트 안에서 BeginTask를 호출해도 다시 이벤트가 발생하지 않습니다.
일반 플레이 중 아이콘 하나를 로딩하는 등 전체 화면이 필요 없는 요청에는 작업을 등록하지 않습니다.

## 맵 전환

로비의 게임 시작은 설정에서 결정한 목적지로 `TravelThroughLoadingLevel(DestinationMap)`을 호출합니다.
새로운 게임 진입 경로도 서버에서 이 API를 사용하면 로딩 레벨을 거칩니다.
일반 OpenLevel / ServerTravel 및 메뉴·로비 진입에는 로딩 UI를 자동 표시하지 않습니다.
로딩 레벨의 GameState가 목적지와 여행 ID를 복제하고 소유 PlayerController가 로컬 준비 결과를 서버에 알립니다.
서버 준비와 모든 참가자의 응답이 확인되기 전에는 목적지 ServerTravel을 실행하지 않습니다.
실패·접속 종료·제한 시간 초과 시 게임으로 진행하지 않고 실패 화면을 유지합니다.

로딩 화면을 사용하는 준비 작업은 실제 로딩 레벨의 OnLoadingStarted에서 시작합니다.
게임 레벨에서 발생하는 일반 비동기 로딩은 각 기능이 백그라운드로 처리합니다.

`CancelLoading`은 화면의 대기 작업만 정리합니다. 팀원이 시작한 요청이나 엔진 travel을 취소하지는
않습니다. 접속 취소는 기존 SessionSubsystem의 CancelJoin을 사용합니다.
게임플레이 코드에서 ScreenSubsystem의 HideLoadingScreen을 직접 호출하지 마세요.

## UI와 이벤트

기본 UI는 실제 로딩 레벨의 viewport에서 제목, 작업 문구, 로딩 애니메이션을 표시합니다.
GameInstance의 월드가 로딩 레벨에서 바뀌는 순간 UI를 제거하므로 목적지 BeginPlay 전에 화면이 종료됩니다.
모든 맵 전환에 적용하던 MoviePlayer 및 자동 overlay 표시는 사용하지 않습니다.
공용 화면은 특정 에셋의 개수나 진행률을 추측하지 않습니다.

* OnLoadingStarted: 각 기능의 준비 작업을 등록할 시점.
* OnStatusChanged / GetStatus: 단계·문구·대기 작업 수·PSO 수 확인.
* OnLoadingCompleted: 준비 완료 후 해당 머신이 로딩 레벨을 떠날 때 호출.
* OnLoadingFailed: 외부 작업 실패, Owner 종료, 제한 시간 초과.

외부 작업은 공용 화면이 재실행할 수 없으므로 실패 화면에는 메뉴 복귀만 제공합니다.
로딩 레벨의 클라이언트 준비 결과는 서버가 수집합니다. 목적지 맵에서의 별도 매치 시작 규칙은 기존 게임 로직을 따릅니다.
PSO 0은 현재 제출된 요청의 완료이며 아직 로딩하지 않은 모든 콘텐츠의 준비 완료가 아닙니다.

## 아이템·수류탄 연결

인벤토리는 BeginPlay에서 OnLoadingStarted를 구독합니다. 로딩 시작, 새 인벤토리 생성,
인벤토리 복제·변경 시 기존 PreloadItemAssets가 필요한 아이콘·메시·사용 어빌리티와 효과를 수집합니다.
아직 준비되지 않은 요청만 현재 로딩 화면의 작업으로 등록합니다. 별도 에셋 목록 등록은 필요 없습니다.
소모품인 수류탄도 같은 경로로 준비되며, 수류탄 액터의 메시 fallback 요청도 진행 중인 화면에만 참여합니다.
무기 공격 프로필·몽타주·탄약·투사체, 수류탄/바리케이드 클래스와 시각·소리 에셋은 소프트 참조입니다.
기존 아이템 로더가 로딩된 데이터/클래스 기본값의 소프트 의존성을 수집하고, 새 프로필이나
어빌리티를 읽은 뒤 발견한 다음 단계의 에셋까지 이어서 준비합니다. 중첩 단계가 끝나야 완료를 알립니다.

이 연결은 로딩 화면을 새로 열지 않습니다. 평소 아이템 획득·사용·수류탄 투척은 기존 방식으로
백그라운드 로딩하고, 사용 권한·수량·피해 검증을 유지합니다. 이전 맵의 인벤토리나 수류탄이
제거되면 자신의 작업만 해제합니다. 필요한 에셋의 로딩 실패는 화면에 실패로 전달합니다.
아직 서버에서 복제되지 않은 인벤토리나 미래에 스폰할 모든 아이템의 준비까지 보장하지는 않습니다.

액터의 기본 메시를 변경할 때는 Grenade/Barricade의 `MeshAsset`을 사용하세요. 런타임 인스턴스의
메시 컴포넌트에 준비된 메시를 적용하며, 에셋 기본값의 컴포넌트에는 메시를 하드 참조로 보관하지 않습니다.
수류탄의 투척 중인 메시에는 기존처럼 아이템 정의의 WorldMesh가 우선합니다.
실행 중인 인벤토리 인스턴스, 컴포넌트, 동적 머티리얼 등의 소유 참조는 유지합니다.

기존 에셋 설정의 소프트 참조 이전은 완료되어 있습니다. 팀원은 변환된 에셋을 그대로 사용하면 됩니다.
