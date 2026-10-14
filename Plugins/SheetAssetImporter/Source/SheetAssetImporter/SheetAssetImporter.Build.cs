using UnrealBuildTool;

public class SheetAssetImporter : ModuleRules
{
    public SheetAssetImporter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "EditorSubsystem", "UnrealEd" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "AssetTools", "AssetRegistry", "ContentBrowser", "ToolMenus", "ApplicationCore", "InputCore",
            "Slate", "SlateCore", "HTTP", "Json", "JsonUtilities", "PropertyEditor", "StructViewer"
        });
    }
}
