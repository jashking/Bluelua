// Fill out your copyright notice in the Description page of Project Settings.

#include "LuaImplementableWidget.h"

#include "Engine/LatentActionManager.h"
#include "Engine/World.h"

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

void ULuaImplementableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ensureMsgf(GetDesiredTickFrequency() != EWidgetTickFrequency::Never, TEXT("SObjectWidget and UUserWidget have mismatching tick states or UUserWidget::NativeTick was called manually (Never do this)")))
	{
		TickActions(InDeltaTime);
	}
}

void ULuaImplementableWidget::TickActions(float InDeltaTime)
{
	if (LuaState.IsValid())
	{
		TSet<UObject*> SubDelegateCallers;
		LuaState->GetObjectsByOwner(this, SubDelegateCallers);

		UWorld* World = GetWorld();
		if (World)
		{
			for (const auto& Object : SubDelegateCallers)
			{
				// Update any latent actions we have for this actor
				World->GetLatentActionManager().ProcessLatentActions(Object, InDeltaTime);
			}
		}
	}
}
