# Steam 리슨 서버 멀티플레이

기존 메인메뉴의 방 만들기, 방 검색, 참가, 로비 준비, 캐릭터 선택을 Steam 로비에 연결했습니다. 호스트가 리슨 서버가 되어 함께 플레이합니다.

## 두 PC에서 실행

1. 두 PC에서 서로 다른 Steam 계정으로 로그인하고 친구 관계를 맺습니다. Steam 설정에서 게임 내 오버레이를 켭니다.
2. 에디터를 재시작하여 새 플러그인과 네트워크 설정을 반영합니다. Windows **Development** 패키지를 만들고 같은 패키지를 두 PC에 복사합니다. 패키지 폴더 전체를 복사해야 합니다.
3. 두 PC에서 게임을 실행합니다. 메인메뉴에 `Steam 온라인 · 친구 초대 사용 가능`이 표시되는지 확인합니다.
4. 호스트는 기존 **방 만들기**를 사용합니다. 로비 오른쪽 위의 **Steam 친구 초대** 버튼을 누르고 Steam 오버레이에서 친구를 선택합니다. 방에 참가한 사람도 빈자리가 있으면 친구를 초대할 수 있습니다.
5. 친구는 Steam 초대를 수락한 뒤 게임의 참가 확인 창에서 **참가**를 누릅니다. 비밀번호 방은 호스트에게 받은 숫자 4자리를 입력합니다. 비밀번호는 Steam 로비 정보에 공개하지 않습니다.
6. 두 사람이 로비에 나타나면 준비를 완료하고 호스트가 시작합니다. 기존 캐릭터 선택을 거쳐 함께 게임 맵으로 이동합니다.

초대 없이 **방 검색**으로 같은 프로젝트의 열린 Steam 방을 찾아 참가할 수도 있습니다. 게임 시작 후에는 검색·초대·추가 참가를 닫습니다. 플레이 도중 받은 초대를 Steam에서 수락하면 현재 방을 나가고 메인메뉴에서 새 방 참가를 확인합니다. 호스트가 다른 초대를 수락하면 기존 방도 종료됩니다. 호스트 이전 기능은 없습니다.

## Steam App ID

`Config/DefaultEngine.ini`의 `SteamDevAppId=480`은 개발용 공용 테스트 ID입니다. 실제 Steam App ID가 제공되지 않아 이 값으로 구성했습니다. 출시 시 자체 App ID로 바꾸고 Steamworks의 실행 파일·런치 옵션·빌드를 등록해야 합니다. 공용 ID의 다른 프로젝트가 검색되지 않도록 `ASGame` 프로토콜 값을 생성/검색/참가에 적용합니다. 엔진의 빌드 호환성 검사도 유지합니다.

Development 빌드는 엔진이 App ID 파일을 관리합니다. 외부 실행에서 Steam 초기화가 안 되면 실제 게임 실행 파일이 있는 `ApocalypseSurvival/Binaries/Win64` 폴더에 `steam_appid.txt`를 만들어 `480`만 한 줄로 넣고 다시 실행합니다. Steam으로 배포할 이미지에는 이 파일을 포함하지 않습니다.

게임이 꺼진 상태의 초대는 Steamworks에 실행 경로와 앱이 등록되어 있어야 Steam이 게임을 실행할 수 있습니다. 공용 ID 480으로 만든 로컬 패키지는 **두 사람 모두 게임을 먼저 실행한 상태**에서 초대 수락을 검증하세요. 코드에서는 시작 시 전달되는 Steam 초대도 플레이어 컨트롤러가 준비될 때까지 보관합니다.

## 에디터 및 LAN 테스트

Steam 온라인 테스트는 별도 게임 프로세스 또는 패키지에서 진행합니다. 에디터 내부 PIE 여러 창은 Steam 두 계정 테스트를 대신하지 못합니다. 에디터/`-nosteam` 실행은 Null 기반 LAN 경로를 사용할 수 있으며 초대 버튼은 비활성화됩니다. 패키지에서 Steam 초기화에 실패하면 온라인 연결 오류를 표시하여 LAN 방을 Steam 방으로 오인하지 않게 합니다.

소스 빌드:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' ApocalypseSurvivalEditor Win64 Development '-Project=C:/Unreal/9th-Team5-Final-Project/ApocalypseSurvival.uproject' -WaitMutex -NoHotReloadFromIDE
```

네트워크 전송은 `SteamSockets.SteamSocketsNetDriver`를 사용하고 Steam을 사용할 수 없는 경우 `IpNetDriver`를 사용합니다. UE 5.8에서는 오래된 문서의 `OnlineSubsystemSteam.SteamNetDriver` 경로를 사용하지 않습니다. Steam 세션은 `bUsesPresence`와 `bUseLobbiesIfAvailable`을 함께 켜고 검색에는 `SEARCH_LOBBIES`를 설정합니다.

## 검증 항목

- 로컬 검증: UE 5.8.3 Development Editor 빌드 성공, `ApocalypseSurvival.Session` 6개 테스트 통과. 보고서: `Saved/Automation/SteamSessions/index.json`, 로그: `Saved/Logs/SteamSessionTests.log`. 실제 Steam 두 계정 간 초대 및 네트워크 플레이는 이 환경에서 검증하지 않았습니다.
- 자동화 테스트 그룹: `ApocalypseSurvival.Session` (비밀번호 테스트 포함).
- Steam/LAN 생성 설정, 로비 검색 필터 일치, 타 프로젝트/닫힌 방 거부, 초대 비밀번호 입력, 초대 취소 및 대기 상태 정리를 자동화 테스트로 확인합니다.
- 실제 두 계정에서는 방 검색 참가, 양쪽의 초대, 잘못된 비밀번호 거부 후 재참가, 방 만원, 시작 직후 지연 초대, 호스트 종료 후 메뉴 복귀, 재생성, 로비→캐릭터 선택→게임 맵 이동을 확인합니다.
- 로그: `[Session] Backend=STEAM`, `LogOnlineSession`, `LogSteamShared`, `LogSockets`, `LogNet`. Steam 연결 오류는 Steam 로그인/클라이언트 실행 상태, App ID, 같은 빌드 사용 여부, 오버레이 활성화를 확인합니다.

참고: [Epic Online Subsystem Steam](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine), [Epic Steam Sockets](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-steam-sockets-in-unreal-engine). 드라이버 경로 및 세션 동작은 설치된 UE 5.8 엔진 소스와 대조했습니다.
