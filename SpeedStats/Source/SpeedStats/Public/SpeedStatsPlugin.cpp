// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SpeedStats.h"

#include "Core.h"
#include "Engine.h"
#include "ModuleManager.h"
#include "ModuleInterface.h"

class FSpeedStatsPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FSpeedStatsPlugin, SpeedStats )

void FSpeedStatsPlugin::StartupModule()
{
	
}


void FSpeedStatsPlugin::ShutdownModule()
{
	
}



