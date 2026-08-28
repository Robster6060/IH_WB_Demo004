// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Character.h"
#include "IH_P1C08_MannequinActor.generated.h"

/** DEV-only scale-reference NPC, placed via the "Mannequin" HUD button (click-to-place on
 * IslandMesh, mirroring "Place Ship"'s click-to-place-on-water). Uses Waterline's bundled legacy
 * UE4 mannequin (SK_Mannequin + ThirdPerson_AnimBP) since no Engine-native Manny/Quinn content
 * ships with this project. Static idle pose for now — selection/group-move (mirroring the
 * signed-off ship/vehicle box-grab convention) is a separate, larger follow-up feature. */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C08_MannequinActor : public ACharacter
{
	GENERATED_BODY()

public:
	AIH_P1C08_MannequinActor();
};
