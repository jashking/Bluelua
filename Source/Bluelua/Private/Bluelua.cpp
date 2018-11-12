// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Bluelua.h"

#include "LuaState.h"
#include "LuaObjectBase.h"

#define LOCTEXT_NAMESPACE "FBlueluaModule"

DEFINE_LOG_CATEGORY(LogBluelua);

void FBlueluaModule::StartupModule()
{
	FLuaObjectBase::Init();
}

void FBlueluaModule::ShutdownModule()
{
	ResetDefaultLuaState();
}

TSharedPtr<FLuaState> FBlueluaModule::GetDefaultLuaState()
{
	if (!DefaultLuaState.IsValid())
	{
		DefaultLuaState = MakeShared<FLuaState>();
	}

	return DefaultLuaState;
}

void FBlueluaModule::ResetDefaultLuaState()
{
	DefaultLuaState.Reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueluaModule, Bluelua)