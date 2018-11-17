#include "BlueluaLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

UObject* UBlueluaLibrary::GetWorldContext()
{
	if (GEngine && GEngine->GameViewport)
	{
		return Cast<UObject>(GEngine->GameViewport->GetWorld());
	}

	return nullptr;
}
