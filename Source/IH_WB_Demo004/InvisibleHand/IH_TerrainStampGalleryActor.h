// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_TerrainStampGalleryActor.generated.h"

class AIH_TerrainStampActor;

/** Stub — stamp gallery deferred with IslandMesh rebuild. */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_TerrainStampGalleryActor : public AActor
{
	GENERATED_BODY()

public:
	AIH_TerrainStampGalleryActor();
	void BuildGallery(bool bIncludeInvertDoubleDutyRow = true);
	FVector GetReviewFocusWorldLocation() const { return GetActorLocation(); }
	void RefreshAllPreviewMeshes() const {}

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;
};
