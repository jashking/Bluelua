// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LuaImplementableInterface.h"
#include "LuaImplementableActor.generated.h"

UCLASS()
class BLUELUA_API ALuaImplementableActor : public AActor, public ILuaImplementableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALuaImplementableActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override final;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "LuaImplementable", meta = (AllowPrivateAccess = "true"))
	FString LuaFilePath;
};
