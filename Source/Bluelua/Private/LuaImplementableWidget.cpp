// Fill out your copyright notice in the Description page of Project Settings.

#include "LuaImplementableWidget.h"

#include "Misc/Paths.h"

#include "Engine/LatentActionManager.h"
#include "Engine/World.h"

void ULuaImplementableWidget::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (!OnProcessLuaOverrideEvent(Function, Parameters))
	{
		Super::ProcessEvent(Function, Parameters);
	}
}

void ULuaImplementableWidget::NativeConstruct()
{
	OnInitLuaBinding();

	Super::NativeConstruct();
}

void ULuaImplementableWidget::NativeDestruct()
{
	Super::NativeDestruct();

	OnReleaseLuaBinding();
}

void ULuaImplementableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ensureMsgf(GetDesiredTickFrequency() != EWidgetTickFrequency::Never, TEXT("SObjectWidget and UUserWidget have mismatching tick states or UUserWidget::NativeTick was called manually (Never do this)")))
	{
		TickActions(InDeltaTime);
	}
}

void ULuaImplementableWidget::BeginDestroy()
{
	Super::BeginDestroy();

	OnReleaseLuaBinding();
}

FString ULuaImplementableWidget::OnInitBindingLuaPath_Implementation()
{
	return FPaths::ProjectContentDir() / LuaFilePath;
}

bool ULuaImplementableWidget::ShouldEnableLuaBinding_Implementation()
{
	return !LuaFilePath.IsEmpty();
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
