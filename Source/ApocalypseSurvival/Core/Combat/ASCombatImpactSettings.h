#pragma once

#include "CoreMinimal.h"
#include "Core/Combat/ASCombatImpactTypes.h"
#include "Engine/DeveloperSettings.h"
#include "ASCombatImpactSettings.generated.h"

/** 프로젝트 전체의 피격 경직·타격 정지 규칙이다. 공격별 수치 대신 이 설정의 결정표를 사용한다. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Combat Impact"))
class APOCALYPSESURVIVAL_API UASCombatImpactSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 일반 몬스터가 Standard/Rapid 공격에 받는 기본 경직 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Monster|Duration", meta = (ClampMin = "0.0", Units = "s"))
	float WeakStaggerDuration = 0.12f;

	/** 일반 몬스터가 Heavy 공격에 받는 기본 경직 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Monster|Duration", meta = (ClampMin = "0.0", Units = "s"))
	float StrongStaggerDuration = 0.35f;

	/** 일반 몬스터가 Knockdown 공격에 받는 기본 제압 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Monster|Duration", meta = (ClampMin = "0.0", Units = "s"))
	float KnockdownDuration = 0.80f;

	/** Rapid 프리셋이 같은 대상에게 실제 경직을 다시 줄 수 있는 최소 간격. */
	UPROPERTY(Config, EditAnywhere, Category = "Monster|Reapply", meta = (ClampMin = "0.0", Units = "s"))
	float RapidReapplyInterval = 0.25f;

	/** Heavy 공격을 맞은 플레이어의 기본 경직 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Player|Duration", meta = (ClampMin = "0.0", Units = "s"))
	float PlayerStaggerDuration = 0.18f;

	/** Knockdown 공격을 맞은 플레이어의 기본 제압 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Player|Duration", meta = (ClampMin = "0.0", Units = "s"))
	float PlayerKnockdownDuration = 0.45f;

	/** 플레이어가 실제 경직을 받은 직후 재경직을 막는 시간. 피해는 막지 않는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Player|Protection", meta = (ClampMin = "0.0", Units = "s"))
	float PlayerProtectionDuration = 0.60f;

	/** 근접 Standard 공격이 명중했을 때만 재생하는 로컬 타격 정지 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Hit Stop", meta = (ClampMin = "0.0", Units = "s"))
	float MeleeStandardHitStop = 0.035f;

	/** 근접 Heavy 공격이 명중했을 때만 재생하는 로컬 타격 정지 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Hit Stop", meta = (ClampMin = "0.0", Units = "s"))
	float MeleeHeavyHitStop = 0.055f;

	/** 근접 Knockdown 공격이 명중했을 때만 재생하는 로컬 타격 정지 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Hit Stop", meta = (ClampMin = "0.0", Units = "s"))
	float MeleeKnockdownHitStop = 0.070f;

	/** 공격 프리셋과 몬스터 저항 등급으로 최종 반응을 계산한다. */
	EASResolvedHitReaction ResolveMonsterReaction(EASImpactPreset ImpactPreset,
		EASStaggerResistance Resistance, bool bHasSuperArmor = false) const;

	/** 플레이어 전용 피격 규칙. 보호 또는 슈퍼아머는 실제 제어를 피격 표현으로 낮춘다. */
	EASResolvedHitReaction ResolvePlayerReaction(EASImpactPreset ImpactPreset,
		bool bHasSuperArmor = false, bool bHasStaggerProtection = false) const;

	/** 몬스터가 최종 반응 상태를 유지해야 하는 시간. */
	float GetMonsterReactionDuration(EASResolvedHitReaction Reaction) const;

	/** 플레이어가 최종 반응 상태를 유지해야 하는 시간. */
	float GetPlayerReactionDuration(EASResolvedHitReaction Reaction) const;

	/** 근접 공격자의 로컬 타격 정지 연출 시간. 원거리 또는 None/Rapid은 0이다. */
	float GetMeleeHitStopDuration(EASImpactPreset ImpactPreset) const;
};
