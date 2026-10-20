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
	if (MonsterDefinition)
	{
		Config.MaxHealth = MonsterDefinition->InitialAttributes.MaxHealth;
		Config.MoveSpeed = MonsterDefinition->InitialAttributes.MoveSpeed;
		Config.AttackPower = MonsterDefinition->InitialAttributes.AttackPower;
		Config.AttackRange = MonsterDefinition->AttackRange;
		Config.DetectionRadius = MonsterDefinition->DetectionRadius;
		Config.LoseTargetRadius = MonsterDefinition->LoseTargetRadius;
		Config.AssistAggroRadius = MonsterDefinition->AssistAggroRadius;
		Config.DirectHitAggro = MonsterDefinition->DirectHitAggro;
		Config.AggroDecayPerSecond = MonsterDefinition->AggroDecayPerSecond;
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
	BuildContext.AddFragment<FASMassMonsterTemporaryVisualFragment>();
	BuildContext.AddFragment<FASMassMonsterTargetFragment>();
	BuildContext.AddFragment<FASMassMonsterCombatFragment>();
	BuildContext.AddFragment<FASMassMonsterNavigationFragment>();

	FASMassMonsterHomeFragment& Home = BuildContext.AddFragment_GetRef<FASMassMonsterHomeFragment>();
	Home.LeashRadius = FMath::Max(0.0f, DefaultLeashRadius);
	Home.ReturnAcceptanceRadius = FMath::Max(1.0f, DefaultReturnAcceptanceRadius);
	Home.DetectionRadius = Config.DetectionRadius;
	Home.LoseTargetRadius = Config.LoseTargetRadius;
}

