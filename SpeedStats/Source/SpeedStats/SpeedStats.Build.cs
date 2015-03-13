// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SpeedStats : ModuleRules
	{
		public SpeedStats(TargetInfo Target)
        {
            PrivateIncludePaths.Add("SpeedStats/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "Json",
                    "HTTP",
                    "UnrealTournament",
				}
				);
		}
	}
}