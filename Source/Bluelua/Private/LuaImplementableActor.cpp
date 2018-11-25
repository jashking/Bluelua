// Fill out your copyright notice in the Description page of Project Settings.

#include "LuaImplementableActor.h"


// Sets default values
ALuaImplementableActor::ALuaImplementableActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void ALuaImplementableActor::PostInitProperties()
{
	Super::PostInitProperties();

	PreRegisterLua(LuaFilePath);
}

// Called when the game starts or when spawned
void ALuaImplementableActor::BeginPlay()
{
	OnInit(LuaFilePath);

	Super::BeginPlay();
}

void ALuaImplementableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	OnRelease();
}

void ALuaImplementableActor::BeginDestroy()
{
	Super::BeginDestroy();

	OnRelease();
}

void ALuaImplementableActor::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (!OnProcessEvent(Function, Parameters))
	{
		Super::ProcessEvent(Function, Parameters);
	}
}

// Called every frame
void ALuaImplementableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
