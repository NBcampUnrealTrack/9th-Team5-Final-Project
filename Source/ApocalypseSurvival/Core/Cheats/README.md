**치트 사용법**

이 폴더의 `UASCheatManager`가 제공하는 테스트용 콘솔 명령이다. 플레이 중 콘솔(`~`)을 열어 입력한다. 명령을 처음 추가하거나 변경한 뒤에는 C++ 빌드 후 에디터를 재시작한다.

명령은 자신이 조종하는 플레이어에게 적용된다. 멀티플레이 클라이언트에서 입력해도 상태 변경과 아이템 지급은 PlayerController의 서버 RPC를 통해 처리한다. Shipping 빌드에서는 동작하지 않는다.

| 명령 | 동작 |
|---|---|
| `DebugGod` | 플레이어 피해 무적 ON/OFF 토글 |
| `DebugGiveWeapon <무기 이름>` | 지정한 무기를 인벤토리에 1개 지급 |
| `DebugGiveWeapon` | 사용법과 지원 무기 이름 표시 |

**플레이어 무적**

```text
DebugGod
```

입력할 때마다 무적이 켜지거나 꺼진다. 결과는 `[DebugGod] Invulnerability ON` 또는 `OFF`로 출력된다.

- 무적 중에도 회복은 적용된다.
- 치트를 꺼도 회피·리스폰 보호 등 다른 출처의 무적은 유지된다.
- 캐릭터가 제거되면 치트 무적은 해제된다. 새 캐릭터에서는 다시 입력한다.
- 체력 회복이나 사망한 캐릭터의 부활은 수행하지 않는다. `DebugKillSelf`처럼 체력을 직접 설정하는 별도 디버그 명령은 피해 무적을 우회한다.

**무기 지급**

```text
DebugGiveWeapon Axe
DebugGiveWeapon SniperRifle
```

| 무기 | 입력 이름 |
|---|---|
| 단검 | `Dagger` |
| 도끼 | `Axe` |
| 장검 | `LongSword` |
| 낫 | `Scythe` |
| 권총 | `Pistol` |
| 산탄총 | `Shotgun` |
| 돌격소총 | `AssaultRifle` |
| 저격소총 | `SniperRifle` |

무기 이름은 대소문자를 구분하지 않는다. 한 번에 1개를 가방에 추가하며 자동 장착하지 않는다. 장착 중인 무기는 가방 슬롯 사용량에 포함하지 않는다.

가방 10칸이 가득 차 있으면 지급하지 않고 기존 아이템과 수량을 유지한다. 잘못된 이름, 준비되지 않은 인벤토리, 자산 로드 실패 시에도 지급하지 않는다.

| 출력 메시지 | 의미 |
|---|---|
| `[DebugGiveWeapon] Granted Axe x1.` | 지급 성공. 무기 이름은 입력한 종류에 따라 달라짐 |
| `[DebugGiveWeapon] Inventory is full. Nothing was granted.` | 가방이 가득 차 지급하지 않음 |
| `[DebugGiveWeapon] Usage: ... Weapons: ...` | 이름 생략 또는 잘못된 이름. 지원 목록 표시 |
| `[DebugGiveWeapon] Inventory is not ready. Nothing was granted.` | 인벤토리를 사용할 수 없어 지급하지 않음 |
| `[DebugGiveWeapon] Could not load weapon '...'. Nothing was granted.` | 무기 자산 로드 또는 종류 검증 실패 |
| `[DebugGiveWeapon] Could not add the weapon. Nothing was granted.` | 인벤토리 추가 실패 |

**새 치트 추가 시**

1. [ASCheatManager.h](ASCheatManager.h)에 `UFUNCTION(Exec)` 명령을 선언하고 [ASCheatManager.cpp](ASCheatManager.cpp)에 진입점을 구현한다.
2. 서버 상태를 바꾸는 명령은 [ASPlayerController](../Framework/ASPlayerController.h)의 서버 RPC를 거친다. 서버에서도 인자와 적용 대상을 검증한다.
3. 실제 게임 상태 변경은 기존 캐릭터·인벤토리·GAS 기능을 사용한다. 무기 지급은 `GiveWeaponToPlayer()`에서 허용 목록과 가방 용량을 검증한 뒤 `AddItem()`을 호출한다.
4. 실행 코드에 `!UE_BUILD_SHIPPING` 가드를 유지하고, 이 문서에 명령·인자·실패 조건을 함께 추가한다.
