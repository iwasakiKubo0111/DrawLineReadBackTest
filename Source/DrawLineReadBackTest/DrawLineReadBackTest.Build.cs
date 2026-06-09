// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DrawLineReadBackTest : ModuleRules
{
	public DrawLineReadBackTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "RHI", "RenderCore" , "UMG" , "Slate", "SlateCore" });
	}
}
