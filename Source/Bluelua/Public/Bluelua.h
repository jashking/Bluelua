// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBluelua, Log, All);

DECLARE_STATS_GROUP(TEXT("Bluelua"), STATGROUP_Bluelua, STATCAT_Advanced);

class FLuaState;

class BLUELUA_API FBlueluaModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<FLuaState> GetDefaultLuaState();

	void ResetDefaultLuaState();

	static inline FBlueluaModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FBlueluaModule>("Bluelua");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Bluelua");
	}

protected:
	TSharedPtr<FLuaState> DefaultLuaState;
};
