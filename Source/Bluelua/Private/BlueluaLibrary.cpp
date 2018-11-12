#include "BlueluaLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

UObject* UBlueluaLibrary::GetWorldContext()
{
	return Cast<UObject>(GEngine->GameViewport->GetWorld());
}
