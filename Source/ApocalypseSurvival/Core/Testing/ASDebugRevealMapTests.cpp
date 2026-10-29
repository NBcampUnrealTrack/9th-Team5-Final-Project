#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "MapSystem/Vision/ASFogDisplayComponent.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Vision/ASVisionEvaluatorComponent.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASDebugRevealMapTest,
	"ApocalypseSurvival.Vision.DebugRevealMapRestoresVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASDebugRevealMapTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) return false;
	AASFogOfWarManager* Manager = World->SpawnActor<AASFogOfWarManager>();
	UASFogDisplayComponent* Fog = Manager->FindComponentByClass<UASFogDisplayComponent>();
	Fog->ResolutionX = Fog->ResolutionY = 2;
	Fog->FogWorldMin = FVector2D(-100.0, -100.0);
	Fog->FogWorldMax = FVector2D(100.0, 100.0);
	Fog->ExploredCells.Init(0, 4);
	Fog->VisibleCells.Init(0, 4);
	Fog->MassVisibilityBufferCells.Init(0, 4);
	Fog->ShownAlpha.Init(235, 4);
	Fog->bHasLocalVisionMask = true;
	const TArray<uint8> OriginalAlpha = Fog->ShownAlpha;
	UASVisionWorldSubsystem* Subsystem = World->GetSubsystem<UASVisionWorldSubsystem>();
	AActor* TargetActor = World->SpawnActor<AActor>();
	UASVisibilityComponent* Target = NewObject<UASVisibilityComponent>(TargetActor);
	Subsystem->RegisterVisibilityTarget(Target);
	Manager->GetVisionEvaluator()->Configure(NewObject<UASVisionDefinition>(Manager));
	Manager->GetVisionEvaluator()->ServerEvaluateVisibility();
	TestFalse(TEXT("A target without a vision source starts hidden"), Target->IsVisibleToTeam(0));

	Manager->SetDebugMapRevealed(true);
	Manager->GetVisionEvaluator()->ServerEvaluateVisibility();
	TestTrue(TEXT("Reveal is enabled"), Manager->IsDebugMapRevealed());
	TestTrue(TEXT("Targets are revealed to the first team"), Target->IsVisibleToTeam(0));
	TestTrue(TEXT("Targets are revealed to the last team"), Target->IsVisibleToTeam(7));
	TestTrue(TEXT("Terrain is visible"), Manager->IsLocationVisibleLocally(FVector::ZeroVector));
	TestTrue(TEXT("Mass visual instances remain eligible"), Manager->IsLocationInLocalVisionBuffer(FVector::ZeroVector));
	TestFalse(TEXT("Bush concealment is bypassed"), Manager->IsLocationConcealedByBushLocally(FVector::ZeroVector));
	TestTrue(TEXT("Wall occlusion setting is preserved"), Manager->IsWallOcclusionEnabled());
	TestEqual(TEXT("Exploration is not permanently filled"), Fog->ExploredCells[0], static_cast<uint8>(0));
	TestTrue(TEXT("Fog alpha is preserved"), Fog->ShownAlpha == OriginalAlpha);

	Manager->SetDebugMapRevealed(false);
	Manager->GetVisionEvaluator()->ServerEvaluateVisibility();
	TestFalse(TEXT("Targets return to normal team visibility"), Target->IsVisibleToTeam(0));
	TestFalse(TEXT("Terrain returns to normal visibility"), Manager->IsLocationVisibleLocally(FVector::ZeroVector));
	TestFalse(TEXT("Unexplored terrain remains unexplored"), Manager->IsLocationExplored(FVector::ZeroVector));
	TestFalse(TEXT("Mass buffer returns to normal"), Manager->IsLocationInLocalVisionBuffer(FVector::ZeroVector));
	TestTrue(TEXT("Restoring preserves fog alpha"), Fog->ShownAlpha == OriginalAlpha);
	Subsystem->UnregisterVisibilityTarget(Target);
	World->DestroyWorld(false);
	return true;
}

#endif
