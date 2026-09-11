using UnrealBuildTool;
using System.Collections.Generic;

public class VerticalGeofenceSimTarget : TargetRules
{
	public VerticalGeofenceSimTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("VerticalGeofenceSim");
	}
}
