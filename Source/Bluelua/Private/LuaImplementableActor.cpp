// Fill out your copyright notice in the Description page of Project Settings.

#include "LuaImplementableActor.h"

#include "Misc/Paths.h"

// Sets default values
ALuaImplementableActor::ALuaImplementableActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ALuaImplementableActor::BeginPlay()
{
	OnInitLuaBinding();

	Super::BeginPlay();
}

void ALuaImplementableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	OnReleaseLuaBinding();
}

void ALuaImplementableActor::BeginDestroy()
{
	Super::BeginDestroy();

	OnReleaseLuaBinding();
}

void ALuaImplementableActor::ProcessEvent(UFunction* Function, void* Parameters)
{
	LuaProcessEvent<Super>(Function, Parameters);
}

FString ALuaImplementableActor::OnInitBindingLuaPath_Implementation()
{
	return FPaths::ProjectContentDir() / LuaFilePath;
}

bool ALuaImplementableActor::ShouldEnableLuaBinding_Implementation()
{
	return !LuaFilePath.IsEmpty();
}
