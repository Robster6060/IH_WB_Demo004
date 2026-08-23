// Copyright Invisible Hand. All Rights Reserved.

#include "IH_P1C07_WaterTankRig.h"
#include "IHInvisibleHandDesignSpec.h"
#include "Components/SceneComponent.h"

AIH_P1C07_WaterTankRig::AIH_P1C07_WaterTankRig()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AIH_P1C07_WaterTankRig::ApplyRealmHalfExtents(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm)
{
	RealmHalfExtentKm.Y = RealmHalfExtentNSKm;
	RealmHalfExtentKm.X = (RealmHalfExtentEWKm > 0.f)
		? RealmHalfExtentEWKm
		: RealmHalfExtentNSKm * static_cast<float>(IHInvisibleHandSpec::GoldenRatioPhi);
}

void AIH_P1C07_WaterTankRig::ApplyDevOceanVisibility(bool /*bOceanVisible*/)
{
}
