// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ApocalypseSurvial : ModuleRules
{
	public ApocalypseSurvial(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",

			// MVVM
			"UMG",
			"ModelViewViewModel",

			// StateTree
			"StateTreeModule",
			"GameplayStateTreeModule",

			// GAS
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",

			// Mass Framework
			"MassEntity",

			"Niagara",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ApocalypseSurvial",
			"ApocalypseSurvial/Variant_Strategy",
			"ApocalypseSurvial/Variant_Strategy/UI",
			"ApocalypseSurvial/Variant_TwinStick",
			"ApocalypseSurvial/Variant_TwinStick/AI",
			"ApocalypseSurvial/Variant_TwinStick/Gameplay",
			"ApocalypseSurvial/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
