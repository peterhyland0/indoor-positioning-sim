using UnrealBuildTool;
using System.Collections.Generic;

public class VerticalGeofenceSimEditorTarget : TargetRules
{
	public VerticalGeofenceSimEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("VerticalGeofenceSim");
	}
}
