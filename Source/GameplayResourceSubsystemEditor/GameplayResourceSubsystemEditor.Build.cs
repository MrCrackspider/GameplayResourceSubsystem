using UnrealBuildTool;

public class GameplayResourceSubsystemEditor : ModuleRules
{
    public GameplayResourceSubsystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore", 
                "GameplayResources",
                "BlueprintGraph",
                "KismetCompiler",
                "UnrealEd",
                "GraphEditor"
            }
        );
    }
}