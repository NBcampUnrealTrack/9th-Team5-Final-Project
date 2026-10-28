#include "Core/Combat/ASCombatDiagnostics.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

bool ASCombatDiagnostics::IsEnabled()
{
	static const bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ASCombatDiagnostics"));
	return bEnabled;
}

void ASCombatDiagnostics::Record(const UObject* Context, const FString& Message)
{
	if (!IsEnabled() || !IsInGameThread()) return;
	static int32 EntryCount = 0;
	if (EntryCount >= 5000) return;
	static FString Filename;
	if (EntryCount == 0)
	{
		if (!FParse::Value(FCommandLine::Get(), TEXT("ASCombatDiagnosticsFile="), Filename))
		{
			Filename = FPaths::ProjectSavedDir() / TEXT("Diagnostics/CombatDiagnostics.log");
		}
		Filename = FPaths::ConvertRelativePathToFull(Filename);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FFileHelper::SaveStringToFile(TEXT("Combat diagnostics, enabled explicitly by command line\n"), *Filename);
	}
	++EntryCount;
	const UWorld* World = Context ? Context->GetWorld() : nullptr;
	const FString Line = FString::Printf(TEXT("Frame=%llu Time=%.3f NetMode=%d Source=%s %s\n"),
		static_cast<unsigned long long>(GFrameCounter), World ? World->GetTimeSeconds() : 0.0,
		World ? static_cast<int32>(World->GetNetMode()) : -1, *GetNameSafe(Context), *Message);
	FFileHelper::SaveStringToFile(Line, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);
}
