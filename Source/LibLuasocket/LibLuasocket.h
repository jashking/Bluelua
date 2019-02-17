// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class LIBLUASOCKET_API FLibLuasocketModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void SetupLuasocket(struct lua_State* L);

	static inline FLibLuasocketModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FLibLuasocketModule>("LibLuasocket");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("LibLuasocket");
	}
};
