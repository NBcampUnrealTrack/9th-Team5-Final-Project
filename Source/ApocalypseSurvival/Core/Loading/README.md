# 공용 로딩 화면

이 시스템은 에셋을 로딩하지 않습니다. 각 기능이 기존 비동기 로딩 코드를 그대로 사용하고,
공용 화면에는 준비 작업의 시작·완료·실패만 알려줍니다. 에셋 참조와 로딩 핸들 수명도 호출자가 관리합니다.
Boot 맵, 로딩 프로필, RequiredAssets 등록은 필요 없습니다. 기본 실행 맵은 기존 MainMenu_Proto입니다.

## 팀원이 사용하는 API

Blueprint: Get Game Instance Subsystem → ASLoadingFlowSubsystem.

1. `BeginTask(Label, Owner)`로 작업을 등록하고 반환된 TaskId를 저장합니다.
2. 자신의 비동기 로딩을 시작합니다. 기존 StreamableManager, AssetManager, 아이템 전용 로더 등을 그대로 사용합니다.
3. 완료 콜백에서 실제 성공 여부를 확인합니다.
4. 성공하면 `CompleteTask(TaskId)`, 실패하면 `FailTask(TaskId, Error)`를 호출합니다.

BeginTask는 화면이 없으면 자동으로 새 로딩 화면을 엽니다. 여러 팀원이 동시에 작업을 등록해도
마지막 작업까지 모두 완료되어야 닫힙니다. 이미 끝나거나 취소된 TaskId는 새 작업에 영향을 주지 않습니다.

```cpp
#include "Core/Loading/ASLoadingFlowSubsystem.h"

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

GameInstanceSubsystem 같은 오래 살아 있는 객체에서 `OnLoadingStarted`에 바인딩합니다.
서브시스템 Initialize에서 구독한다면 `Collection.InitializeDependency<UASLoadingFlowSubsystem>()`를
먼저 호출합니다. 맵 Actor의 BeginPlay에서 구독하면 해당 맵의 로딩 시작 이벤트는 이미 지났을 수 있습니다.

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

## 맵 전환과 수동 표시

기존 OpenLevel / ServerTravel / ClientTravel 경로에서 로딩 화면이 자동 표시됩니다.
이때 OnLoadingStarted가 호출되며 목적지 월드와 등록된 작업, 현재 PSO 요청의 완료를 기다립니다.
Seamless Travel의 중간 월드는 목적지 준비 완료로 취급하지 않습니다.
기존 세션 이동 API를 사용하세요. 이 시스템은 맵 이동이나 서버의 매치 시작을 실행하지 않습니다.

맵 이동 없이 준비 구간을 명시적으로 열려면 `BeginLoading(Title, TimeoutSeconds, bWaitForPSO)`를
사용하세요. 기본 제한 시간은 120초이고 PSO 대기는 켜져 있습니다. 진행 중에 다시 호출해도
기존 작업을 초기화하지 않습니다. 작업이 없다면 준비 확인 후 자동으로 닫힙니다.

`CancelLoading`은 화면의 대기 작업만 정리합니다. 팀원이 시작한 요청이나 엔진 travel을 취소하지는
않습니다. 접속 취소는 기존 SessionSubsystem의 CancelJoin을 사용합니다.
게임플레이 코드에서 ScreenSubsystem의 HideLoadingScreen을 직접 호출하지 마세요.

## UI와 이벤트

기본 UI는 제목, 작업 문구, 로딩 애니메이션을 표시합니다. 맵 로딩으로 게임 스레드가 막히는 구간에는
MoviePlayer + Slate가 표시하고, 이후에는 viewport overlay가 남아 작업 완료까지 기다립니다.
공용 화면은 특정 에셋의 개수나 진행률을 추측하지 않습니다.

* OnLoadingStarted: 각 기능의 준비 작업을 등록할 시점.
* OnStatusChanged / GetStatus: 단계·문구·대기 작업 수·PSO 수 확인.
* OnLoadingCompleted: 해당 머신의 정상 로딩 완료.
* OnLoadingFailed: 외부 작업 실패, Owner 종료, 제한 시간 초과.

외부 작업은 공용 화면이 재실행할 수 없으므로 실패 화면에는 메뉴 복귀만 제공합니다.
각 클라이언트의 완료를 모아 게임을 시작하는 정책은 서버에서 별도로 관리하세요.
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

## 검증 항목

자동화 테스트: ApocalypseSurvival.Loading 및 ApocalypseSurvival.UI.LoadingDotsCycle.
ScreenRendering은 렌더링 가능한 실행에서 Saved/Automation에 일반·실패 화면 PNG를 저장합니다.
실제 패키지 첫 실행 PSO와 다중 클라이언트 이동은 별도 플레이 검증 대상입니다.
