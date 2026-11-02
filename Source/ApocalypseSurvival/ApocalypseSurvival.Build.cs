// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ApocalypseSurvival : ModuleRules
{
	public ApocalypseSurvival(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
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
			"MassCore",
			"MassEntity",
			"MassCommon",
			"MassSpawner",
			"MassSimulation",
			"MassLOD",
			"MassMovement",
			"MassRepresentation",
			"MassActors",
			"MassSignals",

			// Mass AI / Navigation
			"MassNavigation",
			"MassNavMeshNavigation",
			"MassAIBehavior",

			// Mass Multiplayer
			"MassReplication",
			"MassAIReplication",
			"NetCore",

			"Niagara",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"MoviePlayer",
			"RenderCore",
			"CoreOnline",
			"EngineSettings",
			"SlateCore",
			"NavCorridor"
		});

		PublicIncludePaths.AddRange(new string[] {
			"ApocalypseSurvival",
			"ApocalypseSurvival/Variant_Strategy",
			"ApocalypseSurvival/Variant_Strategy/UI",
			"ApocalypseSurvival/Variant_TwinStick",
			"ApocalypseSurvival/Variant_TwinStick/AI",
			"ApocalypseSurvival/Variant_TwinStick/Gameplay",
			"ApocalypseSurvival/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

	}
}
