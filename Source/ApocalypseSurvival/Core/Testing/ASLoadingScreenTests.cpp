#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/UI/SASLoadingScreen.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASLoadingDotsTest,
	"ApocalypseSurvival.UI.LoadingDotsCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASLoadingDotsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Starts without dots"), SASLoadingScreen::GetVisibleDotCount(0.0), 0);
	TestEqual(TEXT("First dot appears"), SASLoadingScreen::GetVisibleDotCount(0.5), 1);
	TestEqual(TEXT("Second dot appears"), SASLoadingScreen::GetVisibleDotCount(1.0), 2);
	TestEqual(TEXT("Third dot appears"), SASLoadingScreen::GetVisibleDotCount(1.5), 3);
	TestEqual(TEXT("All dots disappear"), SASLoadingScreen::GetVisibleDotCount(2.0), 0);
	TestEqual(TEXT("Next cycle starts"), SASLoadingScreen::GetVisibleDotCount(2.5), 1);
	return true;
}

#endif
