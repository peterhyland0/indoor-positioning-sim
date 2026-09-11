using UnrealBuildTool;

public class VerticalGeofenceSim : ModuleRules
{
	public VerticalGeofenceSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"WebSockets", "Json", "JsonUtilities",
			"Slate", "SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
