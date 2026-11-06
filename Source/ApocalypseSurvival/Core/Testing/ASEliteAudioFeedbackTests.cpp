#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/Enemy/ASMonsterBeamTelegraphActor.h"
#include "Characters/Enemy/ASMonsterSlowFieldActor.h"
#include "Characters/Enemy/ASMonsterSlowFieldProjectile.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASEliteAudioFeedbackTest,
	"ApocalypseSurvival.Audio.EliteFeedbackAuthorityAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASEliteAudioFeedbackTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("EliteAudioFeedbackTest")));
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("World"), World)) return false;
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	AASMonsterBeamTelegraphActor* Beam = World->SpawnActor<AASMonsterBeamTelegraphActor>();
	AASMonsterSlowFieldProjectile* Projectile = World->SpawnActor<AASMonsterSlowFieldProjectile>();
	if (!Beam || !Projectile)
	{
		AddError(TEXT("Could not spawn feedback actors"));
		Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}
	Beam->NotifyBeamFired();
	Beam->ReleaseAfterFeedback();
	TestFalse(TEXT("Fired beam survives long enough to send its event"), Beam->IsActorBeingDestroyed());
	TestTrue(TEXT("Fired beam is hidden during cleanup"), Beam->IsHidden());
	TestEqual(TEXT("Beam cleanup delay"), Beam->GetLifeSpan(), 0.2f);

	const FVector Impact(300.0f, 400.0f, 0.0f);
	Projectile->InitializeProjectile(Impact, 0.1f, 0.0f, FGameplayEffectSpecHandle(), nullptr,
		AASMonsterSlowFieldActor::StaticClass(), 250.0f, 4.0f);
	Projectile->Tick(0.2f);
	Projectile->Tick(0.2f);
	int32 Fields = 0;
	for (TActorIterator<AASMonsterSlowFieldActor> It(World); It; ++It)
	{
		++Fields;
		TestEqual(TEXT("Field radius preserved"), It->GetRadius(), 250.0f);
		TestEqual(TEXT("Field duration preserved"), It->GetDuration(), 4.0f);
	}
	TestEqual(TEXT("Repeated ticks produce only one authoritative field"), Fields, 1);
	TestTrue(TEXT("Impacted projectile is hidden during cleanup"), Projectile->IsHidden());
	TestEqual(TEXT("Impact position preserved"), Projectile->GetActorLocation(), Impact);
	TestEqual(TEXT("Projectile cleanup delay"), Projectile->GetLifeSpan(), 0.2f);

	AASMonsterSlowFieldProjectile* Receiver = World->SpawnActor<AASMonsterSlowFieldProjectile>();
	Receiver->SetRole(ROLE_SimulatedProxy);
	UFunction* ImpactEvent = Receiver->FindFunction(TEXT("MulticastProjectileImpact"));
	if (TestNotNull(TEXT("Impact multicast"), ImpactEvent))
	{
		TestTrue(TEXT("Impact is a reliable multicast"), ImpactEvent->HasAllFunctionFlags(FUNC_Net | FUNC_NetMulticast | FUNC_NetReliable));
		struct FImpactParams { FVector_NetQuantize Location; } Args { Impact };
		Receiver->ProcessEvent(ImpactEvent, &Args);
		TestEqual(TEXT("Client receives exact impact location"), Receiver->GetActorLocation(), Impact);
		Args.Location = FVector(900.0f, 0.0f, 0.0f);
		Receiver->ProcessEvent(ImpactEvent, &Args);
		TestEqual(TEXT("Duplicate impact does not replay or relocate"), Receiver->GetActorLocation(), Impact);
	}
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
