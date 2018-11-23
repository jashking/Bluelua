// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlueluaEditor.h"
#include "Editor.h"

#include "Bluelua.h"
#include "LuaImplementableInterface.h"

#define LOCTEXT_NAMESPACE "FBlueluaEditorModule"

void FBlueluaEditorModule::StartupModule()
{
	FEditorDelegates::EndPIE.AddRaw(this, &FBlueluaEditorModule::OnEndPIE);
}

void FBlueluaEditorModule::ShutdownModule()
{
}

void FBlueluaEditorModule::OnEndPIE(bool bIsSimulating)
{
	ILuaImplementableInterface::CleanAllLuaImplementableObject();

	FBlueluaModule::Get().ResetDefaultLuaState();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueluaEditorModule, BlueluaEditor)
