#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Loading/ASLoadingFlowSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/UI/SASLoadingScreen.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASLoadingFlowCoordinationTest,
	"ApocalypseSurvival.Loading.TaskAndTravelCoordination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASLoadingFlowCoordinationTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("LoadingFlowTest")));
	UWorld* World = Instance->GetWorld();
	auto* Flow = Instance->GetSubsystem<UASLoadingFlowSubsystem>();
	const FGuid First = Flow->BeginTask(FText::FromString(TEXT("Player")), Instance);
	const FGuid Second = Flow->BeginTask(FText::FromString(TEXT("Network")), Instance);
	Flow->CompleteTask(First);
	Flow->Tick(0);
	TestTrue(TEXT("A second caller's task keeps the loading screen active"), Flow->IsLoading());
	TestEqual(TEXT("Only the completed task is removed"), Flow->GetStatus().PendingTasks, 1);
	Flow->BeginMapLoading(TEXT("/Game/Maps/Destination?listen"));
	Flow->BeginMapLoading(TEXT("/Game/Maps/Transition"));
	TestEqual(TEXT("Intermediate seamless world does not replace the destination"), Flow->ExpectedMap, FString(TEXT("/Game/Maps/Destination")));
	TestEqual(TEXT("Travel preserves registered preparation tasks"), Flow->Tasks.Num(), 1);
	Flow->CompleteTask(Second);
	Flow->Tick(0);
	TestTrue(TEXT("Tasks complete but destination world is not ready"), Flow->IsLoading());
	Flow->CancelLoading();
	const FGuid Current = Flow->BeginTask(FText::FromString(TEXT("New generation")), Instance);
	Flow->CompleteTask(Second);
	TestTrue(TEXT("An old completion cannot remove a new task"), Flow->Tasks.Contains(Current));
	Flow->CompleteTask(Current);
	Flow->ReadySince = FPlatformTime::Seconds() - 1;
	Flow->Tick(0);
	TestFalse(TEXT("The flow finishes once all readiness conditions are met"), Flow->IsLoading());
	TestEqual(TEXT("PIE instance prefix is removed, full package path is preserved"),
		UASLoadingFlowSubsystem::NormalizeMapName(TEXT("/Game/Maps/UEDPIE_12_Destination.Destination")), FString(TEXT("/Game/Maps/Destination")));
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASLoadingFailureTest,
	"ApocalypseSurvival.Loading.ExternalTaskFailureAndTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASLoadingFailureTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("LoadingFailureTest")));
	UWorld* World = Instance->GetWorld();
	auto* Flow = Instance->GetSubsystem<UASLoadingFlowSubsystem>();
	Flow->BeginLoading(FText::FromString(TEXT("Custom loading")), 10, false);
	const FGuid Task = Flow->BeginTask(FText::FromString(TEXT("A teammate's async work")), Instance);
	Flow->BeginLoading(FText::FromString(TEXT("Another caller")));
	TestTrue(TEXT("Another BeginLoading does not discard a caller's pending task"), Flow->Tasks.Contains(Task));
	Flow->FailTask(Task, FText::FromString(TEXT("External load failed")));
	TestEqual(TEXT("External failures remain visible instead of completing"), Flow->GetStatus().Phase, EASLoadingPhase::Failed);
	TestFalse(TEXT("Failure stops the pending barrier"), Flow->IsLoading());
	Flow->CompleteTask(Task);
	TestEqual(TEXT("A late success cannot dismiss the failure"), Flow->GetStatus().Phase, EASLoadingPhase::Failed);
	Flow->BeginLoading(FText::GetEmpty(), 1, false);
	Flow->BeginTask(FText::GetEmpty(), Instance);
	Flow->StartedAt = FPlatformTime::Seconds() - 2;
	Flow->Tick(0);
	TestEqual(TEXT("An unfinished external task times out"), Flow->GetStatus().Phase, EASLoadingPhase::Failed);
	Flow->CancelLoading();
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASLoadingScreenRenderTest,
	"ApocalypseSurvival.Loading.ScreenRendering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASLoadingScreenRenderTest::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender()) { AddInfo(TEXT("Rendering unavailable; run with -RenderOffscreen to verify the screen.")); return true; }
	auto Model = MakeShared<FASLoadingScreenModel, ESPMode::ThreadSafe>();
	FASLoadingStatus Status;
	Status.Title = FText::FromString(TEXT("APOCALYPSE SURVIVAL"));
	Status.Message = FText::FromString(TEXT("Loading resources..."));
	Model->Update(Status);
	auto Save = [&](const FString& Name)
	{
		FWidgetRenderer Renderer(false, true);
		const auto Widget = SNew(SASLoadingScreen).Model(Model)
			.OnReturnToMenu(FSimpleDelegate::CreateLambda([]() {}));
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(Widget, FVector2D(1280, 720));
		if (!TestNotNull(TEXT("Loading screen render target"), Target)) return;
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		if (!TestEqual(TEXT("Loading screen renders every pixel"), Pixels.Num(), 1280 * 720)) return;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(1280, 720, Pixels, Png);
		TestTrue(TEXT("Screen preview saved"), FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / TEXT("Automation") / Name)));
	};
	Save(TEXT("LoadingScreen.png"));
	Status.Phase = EASLoadingPhase::Failed;
	Status.Message = FText::FromString(TEXT("Required resources could not be loaded."));
	Model->Update(Status);
	Save(TEXT("LoadingScreenFailed.png"));
	return true;
}

#endif
