#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Framework/ASSessionSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASRoomPasswordFormatTest,
	"ApocalypseSurvival.Session.RoomPasswordFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASRoomPasswordFormatTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Four ASCII digits are accepted"), UASSessionSubsystem::IsValidRoomPassword(TEXT("1234")));
	TestTrue(TEXT("Leading zeroes are preserved"), UASSessionSubsystem::IsValidRoomPassword(TEXT("0001")));
	TestFalse(TEXT("Empty password is rejected"), UASSessionSubsystem::IsValidRoomPassword(TEXT("")));
	TestFalse(TEXT("Three digits are rejected"), UASSessionSubsystem::IsValidRoomPassword(TEXT("123")));
	TestFalse(TEXT("Five digits are rejected"), UASSessionSubsystem::IsValidRoomPassword(TEXT("12345")));
	TestFalse(TEXT("Letters are rejected"), UASSessionSubsystem::IsValidRoomPassword(TEXT("12a4")));
	TestFalse(TEXT("Spaces are rejected"), UASSessionSubsystem::IsValidRoomPassword(TEXT("12 4")));
	TestFalse(TEXT("Travel options cannot be injected"), UASSessionSubsystem::IsValidRoomPassword(TEXT("?123")));
	TestFalse(TEXT("Full-width digits are rejected"), UASSessionSubsystem::IsValidRoomPassword(TEXT("１２３４")));
	return true;
}

#endif
