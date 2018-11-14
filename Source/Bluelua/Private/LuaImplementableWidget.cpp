// Fill out your copyright notice in the Description page of Project Settings.

#include "LuaImplementableWidget.h"

void ULuaImplementableWidget::PostInitProperties()
{
	Super::PostInitProperties();

	PreRegisterLua(LuaFilePath);
}

void ULuaImplementableWidget::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (!OnProcessEvent(Function, Parameters))
	{
		Super::ProcessEvent(Function, Parameters);
	}
}

void ULuaImplementableWidget::NativeConstruct()
{
	OnInit(LuaFilePath);

	Super::NativeConstruct();
}

void ULuaImplementableWidget::NativeDestruct()
{
	Super::NativeDestruct();

	OnRelease();
}
