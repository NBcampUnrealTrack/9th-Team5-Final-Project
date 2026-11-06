#include "Mass/Monster/Traits/ASMassMonsterTrait.h"

#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterTrait)

void UASMassMonsterTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FASMassMonsterConfigFragment Config = Parameters;
	Config.SourceMonsterDefinition = MonsterDefinition;
	Config.StatDefinition = StatDefinition;
	if (MonsterDefinition)
	{
		Config.MaxHealth = MonsterDefinition->InitialAttributes.MaxHealth;
		Config.MoveSpeed = MonsterDefinition->InitialAttributes.MoveSpeed;
		Config.AttackPower = MonsterDefinition->InitialAttributes.AttackPower;
		// Actor Definition에 지정한 StatDefinition은 Mass에서도 최우선으로 공유한다.
		if (MonsterDefinition->StatDefinition)
		{
			Config.StatDefinition = MonsterDefinition->StatDefinition;
		}
		Config.ExperienceReward = MonsterDefinition->ExperienceReward;
		// Definition에 드랍 그룹이 설정된 경우에만 우선한다. 비어 있으면 Mass 전용 Parameters 값을 보존한다.
		if (MonsterDefinition->DropGroup.DataTable && !MonsterDefinition->DropGroup.RowName.IsNone())
		{
			Config.DropGroup = MonsterDefinition->DropGroup;
		}
		Config.AttackRange = MonsterDefinition->AttackRange;
		Config.StaggerResistance = MonsterDefinition->StaggerResistance;
		Config.DetectionRadius = MonsterDefinition->DetectionRadius;
		Config.LoseTargetRadius = MonsterDefinition->LoseTargetRadius;
		Config.AssistAggroRadius = MonsterDefinition->AssistAggroRadius;
		Config.DirectHitAggro = MonsterDefinition->DirectHitAggro;
		Config.AggroDecayPerSecond = MonsterDefinition->AggroDecayPerSecond;
		Config.ProximityScoreMax = MonsterDefinition->ProximityScoreMax;
		Config.ProximityHalfScoreDistance = MonsterDefinition->ProximityHalfScoreDistance;
		Config.TargetSwitchScoreMargin = MonsterDefinition->TargetSwitchScoreMargin;
		Config.MinTargetLockSeconds = MonsterDefinition->MinTargetLockSeconds;
		Config.BlockingTargetSearchRadius = MonsterDefinition->BlockingTargetSearchRadius;
		Config.BlockingTargetMinForwardDot = MonsterDefinition->BlockingTargetMinForwardDot;
		Config.BlockingTargetStuckSeconds = MonsterDefinition->BlockingTargetStuckSeconds;
		Config.BlockingTargetMinProgressDistance = MonsterDefinition->BlockingTargetMinProgressDistance;
		Config.DeathDespawnDelay = MonsterDefinition->DeathDespawnDelay;
	}
	Config.Validate();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
	BuildContext.AddConstSharedFragment(ConfigFragment);
	BuildContext.AddTag<FASMassMonsterTag>();

	// 위치는 모든 Mass 몬스터의 기본 데이터다. 이동·표현 Trait는 이 Fragment를 요구해서 사용한다.
	BuildContext.AddFragment<FTransformFragment>();

	FASMassMonsterHealthFragment& Health = BuildContext.AddFragment_GetRef<FASMassMonsterHealthFragment>();
	Health.CurrentHealth = Config.MaxHealth;

	BuildContext.AddFragment<FASMassMonsterBehaviorFragment>();
	if (bUseTemporaryStateVisual)
	{
		BuildContext.AddFragment<FASMassMonsterTemporaryVisualFragment>();
	}
	BuildContext.AddFragment<FASMassMonsterTargetFragment>();
	BuildContext.AddFragment<FASMassMonsterThreatFragment>();
	BuildContext.AddFragment<FASMassMonsterCombatFragment>();
	BuildContext.AddFragment<FASMassMonsterHitReactionFragment>();
	BuildContext.AddFragment<FASMassMonsterModifierFragment>();
	BuildContext.AddFragment<FASMassMonsterRuntimeStatsFragment>();
	BuildContext.AddFragment<FASMassMonsterNavigationFragment>();
	// 로컬 전장의 안개 표현 상태이며 Mass Replication 대상에 포함하지 않는다.
	BuildContext.AddFragment<FASMassMonsterFogVisibilityFragment>();

	FASMassMonsterHomeFragment& Home = BuildContext.AddFragment_GetRef<FASMassMonsterHomeFragment>();
	Home.LeashRadius = FMath::Max(0.0f, DefaultLeashRadius);
	Home.ReturnAcceptanceRadius = FMath::Max(1.0f, DefaultReturnAcceptanceRadius);
	Home.DetectionRadius = Config.DetectionRadius;
	Home.LoseTargetRadius = Config.LoseTargetRadius;
}

