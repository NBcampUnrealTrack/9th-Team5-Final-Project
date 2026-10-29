#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Framework/ASPlayerCameraManager.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASDebugZoomLimitsTest,
	"ApocalypseSurvival.Camera.DebugZoomUnlockAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASDebugZoomLimitsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) return false;
	AASPlayerCameraManager* Camera = World->SpawnActor<AASPlayerCameraManager>();
	Camera->MinCameraDistance = 200.0f;
	Camera->MaxCameraDistance = 400.0f;
	Camera->ZoomStep = 50.0f;
	Camera->TargetCameraDistance = Camera->ViewCameraDistance = 300.0f;
	Camera->AddZoomInput(100.0f);
	TestEqual(TEXT("Default zoom-in limit"), Camera->TargetCameraDistance, 200.0f);
	Camera->AddZoomInput(-100.0f);
	TestEqual(TEXT("Default zoom-out limit"), Camera->TargetCameraDistance, 400.0f);

	Camera->SetDebugZoomUnlocked(true);
	Camera->AddZoomInput(-100.0f);
	TestTrue(TEXT("Unlocked zoom exceeds the old maximum"), Camera->TargetCameraDistance > 400.0f);
	Camera->ViewCameraDistance = Camera->TargetCameraDistance;
	Camera->SetDebugZoomUnlocked(false);
	TestEqual(TEXT("Restoring clamps the requested distance"), Camera->TargetCameraDistance, 400.0f);
	TestEqual(TEXT("Restoring clamps the displayed distance"), Camera->ViewCameraDistance, 400.0f);

	Camera->SetDebugZoomUnlocked(true);
	Camera->AddZoomInput(100.0f);
	TestEqual(TEXT("Unlocked zoom cannot cross the focus point"), Camera->TargetCameraDistance, 1.0f);
	Camera->ViewCameraDistance = Camera->TargetCameraDistance;
	Camera->SetDebugZoomUnlocked(false);
	TestEqual(TEXT("Restoring the near limit"), Camera->TargetCameraDistance, 200.0f);
	TestEqual(TEXT("Restoring the displayed near limit"), Camera->ViewCameraDistance, 200.0f);
	TestEqual(TEXT("Configured minimum remains unchanged"), Camera->MinCameraDistance, 200.0f);
	TestEqual(TEXT("Configured maximum remains unchanged"), Camera->MaxCameraDistance, 400.0f);
	World->DestroyWorld(false);
	return true;
}

#endif
