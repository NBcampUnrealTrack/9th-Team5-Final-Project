#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "ASCombatImpactTestLibrary.generated.h"

class AASMonsterCharacter;
class APawn;
class UASItemDefinition;
class UGameplayAbility;
class UMassEntityConfigAsset;

#if !UE_BUILD_SHIPPING
/** 실제 전투 경로에 영향을 주지 않는 읽기 전용 테스트 계측. 서버에서 실행된 HitStop Multicast만 기록한다. */
struct APOCALYPSESURVIVAL_API FASCombatImpactTestDiagnostics
{
	static void RecordMeleeHitStopDispatch(const AActor* Character, float Duration);
};
#endif

/**
 * Combat Impact 격리 테스트 맵 전용 진단 함수.
 * 판정을 흉내 내지 않고 실제 전투 경로(ExecuteHit, 몬스터 GA, Mass 피해)를 호출한 뒤 결과 상태만 읽는다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASCombatImpactTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 플레이어 인벤토리에 있는 무기로 실제 UASMeleeAttackExecution::ExecuteHit를 한 번 실행한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static bool RunMeleeExecuteHit(APawn* Player, UASItemDefinition* WeaponDefinition, FVector AimDirection, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static void ResetMeleeHitStopDiagnostics(AActor* Character);

	/** 마지막 Reset 이후 서버에서 실행된 MulticastPlayMeleeHitStop 횟수와 마지막 요청 시간. */
	UFUNCTION(BlueprintPure, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static void GetMeleeHitStopDiagnostics(AActor* Character, int32& OutDispatchCount, float& OutLastDuration);

	/** State.Monster.HitReact를 부여한 활성 GE 중 가장 늦게 끝나는 것의 전체 지속 시간과 남은 시간. */
	UFUNCTION(BlueprintPure, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static bool GetMonsterHitReactState(AASMonsterCharacter* Monster, float& OutTotalDuration, float& OutRemaining);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static void StopMonsterAI(AASMonsterCharacter* Monster);

	/** 타깃을 지정하고 몬스터가 보유한 GA를 클래스로 활성화한다. 피해와 경직은 GA의 실제 경로로 적용된다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static bool ActivateMonsterAbility(AASMonsterCharacter* Monster, TSubclassOf<UGameplayAbility> AbilityClass, AActor* Target);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static void RestoreFullHealth(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static float GetHealth(AActor* Target);

	/** 0이면 실패. 반환값은 Mass Entity Handle 번호다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
	static int64 SpawnMassTestMonster(UObject* WorldContextObject, UMassEntityConfigAsset* EntityConfig, FVector Location,
		EASMassMonsterWorldBehavior WorldBehavior);

	/** UASMassMonsterSubsystem::ApplyDamageToTarget 실제 경로로 피해와 경직 의도를 전달한다. 반환값은 피격 수. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
	static int32 ApplyMassImpact(UObject* WorldContextObject, int64 EntityId, EASImpactPreset Preset, float Damage, AActor* Instigator);

	/** FASMassMonsterHitReactionFragment 상태. 시간 값은 현재 월드 시간 기준으로 남은 초. */
	UFUNCTION(BlueprintCallable, Category = "Combat Impact Test", meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
	static bool GetMassHitReaction(UObject* WorldContextObject, int64 EntityId, EASResolvedHitReaction& OutReaction,
		float& OutRemaining, int32& OutSequence, float& OutRapidCooldownRemaining, float& OutHealth);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
	static void DestroyMassTestMonster(UObject* WorldContextObject, int64 EntityId);

	// ---- 자동 판정 케이스. 모든 케이스는 실제 전투 경로를 실행한 뒤 측정값과 기대값을 [ImpactTest] PASS/FAIL로 기록한다. ----

	/**
	 * 몬스터 → 플레이어 케이스 시작. 몬스터를 플레이어 정면 Distance에 배치하고 AI를 멈춘 뒤,
	 * 플레이어의 경직 태그 기록을 시작하고 몬스터 GA를 실제로 활성화한다. 결과는 FinishActorMonsterAttackCase에서 판정한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static void StartActorMonsterAttackCase(AASMonsterCharacter* Monster, APawn* Player,
		TSubclassOf<UGameplayAbility> AbilityClass, float Distance, bool bPlayerSuperArmor);

	/**
	 * 플레이어가 받은 피해, State.Staggered/KnockedDown/StaggerProtected 부여 횟수와 실제 유지 시간을 판정한다.
	 * bExpectStagger/bExpectKnockdown은 하니스가 정의한 기대 결과이며, GA의 ImpactPreset이 ExpectedPreset과 같은지도 확인한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static bool FinishActorMonsterAttackCase(const FString& CaseName, AASMonsterCharacter* Monster, APawn* Player,
		EASImpactPreset ExpectedPreset, bool bExpectStagger, bool bExpectKnockdown);

	/**
	 * 플레이어 정면에 Monster를 두고 무기의 실제 ExecuteHit를 1회 실행해 HitReact GE 지속 시간을 판정한다.
	 * ExpectedReaction이 VisualOnly/None이면 피해만 들어가고 HitReact가 없어야 한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static bool RunActorMonsterReactionCase(const FString& CaseName, APawn* Player, AASMonsterCharacter* Monster,
		AASMonsterCharacter* OtherMonster, UASItemDefinition* WeaponDefinition, EASResolvedHitReaction ExpectedReaction);

	/**
	 * TargetsInCone를 플레이어 정면 부채꼴에, OutOfConeMonsters를 플레이어 뒤에 배치하고 실제 ExecuteHit를 1회 실행한다.
	 * MulticastPlayMeleeHitStop 서버 발송 횟수(명중 시 정확히 1, 빗나가면 0)와 요청 시간을 판정한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly))
	static bool RunMeleeHitStopCase(const FString& CaseName, APawn* Player, UASItemDefinition* WeaponDefinition,
		const TArray<AASMonsterCharacter*>& TargetsInCone, const TArray<AASMonsterCharacter*>& OutOfConeMonsters);

	/**
	 * Mass Entity에 실제 ApplyDamageToTarget 경로로 Preset 피격을 1회 적용하고 Hit Reaction Fragment를 판정한다.
	 * ExpectedReaction이 Weak/Strong/Knockdown이면 새 반응이 적용되어야 하고, None/VisualOnly면 반응 상태가 바뀌지 않아야 한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat Impact Test", meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
	static bool RunMassImpactCase(UObject* WorldContextObject, const FString& CaseName, int64 EntityId,
		EASImpactPreset Preset, EASResolvedHitReaction ExpectedReaction);
};
