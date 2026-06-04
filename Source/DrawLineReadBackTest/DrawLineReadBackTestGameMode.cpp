// Copyright Epic Games, Inc. All Rights Reserved.

#include "DrawLineReadBackTestGameMode.h"
#include "DrawLineReadBackTestCharacter.h"
#include "UObject/ConstructorHelpers.h"

ADrawLineReadBackTestGameMode::ADrawLineReadBackTestGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
