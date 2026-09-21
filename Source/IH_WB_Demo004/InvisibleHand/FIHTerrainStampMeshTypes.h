// Copyright Invisible Hand. All Rights Reserved.
// Static-mesh Terrain Stamp catalog (2026-09-09) - replaces the dead procedural height-grid drop
// path (FIHTerrainStampCatalog/AIH_TerrainStampActor's old ApplyToHeightGrid, both already no-ops -
// see FIHTerrainStampTypes.h and AIH_WB_IslandActor::HasCellHeightGrid). Backed by a real
// DT_TerrainStamp DataTable so adding a future stamp mesh never needs a C++ recompile: drop the
// mesh under the right TS_Vertical/TS_Inverted subfolder, then edit that one row in-editor.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FIHTerrainStampTypes.h"
#include "FIHTerrainStampMeshTypes.generated.h"

// Mirrors IHInvisibleHandSpec::ETerrainStampFamily's two values (Vertical/Inverted) as a UENUM -
// that namespaced type is a plain C++ enum (not UHT-reflected), so a DataTable row's UPROPERTY
// can't use it directly. Convert 1:1 at the few call sites that need the namespaced type.
UENUM(BlueprintType)
enum class EIHStampMeshCategory : uint8
{
	Vertical,
	Inverted,
};

// "Feel"/topology family for a static-mesh stamp - orthogonal to EIHStampMeshCategory above.
UENUM(BlueprintType)
enum class EIHStampMeshFamily : uint8
{
	Dome,
	Ridge,
	Cliff,
	Bowl,
	Trench,
	Scoop,
};

// How a stamp gets placed. Only SingleDrop is implemented today (and in the follow-up bounding-box
// transform round) - the other three reserve room in the data model for later phases so adding them
// doesn't force a redesign:
//  - MultiPieceChainable: Ridge/Trench primitives the player combines at angles into a mountain
//    range / canyon system.
//  - OpenSplineWhiskerChain: a single open spline connecting a Ridge's apexes (or a Trench's
//    nadirs), reshaped via tangent-"whisker" grips. Trench variant needs inflow-higher-than-outflow
//    validation once Hydrology exists.
//  - SkewableTopPlane: Cliffs+Ramp's inclinable ~5-25 degree flat top, sheer ~81-90 degree side
//    walls held fixed.
UENUM(BlueprintType)
enum class EIHStampMeshPlacementMode : uint8
{
	SingleDrop,
	MultiPieceChainable,
	OpenSplineWhiskerChain,
	SkewableTopPlane,
};

// One row of DT_TerrainStamp. StampId reuses EIHTerrainStampId (FIHTerrainStampTypes.h) - the same
// 22-value enum already keying the "W" grid's slot layout, so there is no parallel id space.
USTRUCT(BlueprintType)
struct FIHTerrainStampMeshRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain Stamp")
	EIHTerrainStampId StampId = EIHTerrainStampId::Hill;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain Stamp")
	EIHStampMeshCategory Category = EIHStampMeshCategory::Vertical;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain Stamp")
	EIHStampMeshFamily Family = EIHStampMeshFamily::Dome;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain Stamp")
	EIHStampMeshPlacementMode PlacementMode = EIHStampMeshPlacementMode::SingleDrop;

	// Null = not yet available; the W-grid button reads as disabled/greyed until this is filled in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain Stamp")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain Stamp")
	FString DisplayLabel;
};

struct FIHTerrainStampMeshCatalog
{
	// True iff StampId has a row in DT_TerrainStamp with a Mesh that resolves to a real asset.
	static bool IsAvailable(EIHTerrainStampId StampId);

	// Null if not available. Does not load the mesh - callers load it themselves (SetStaticMesh
	// takes care of that via TSoftObjectPtr::LoadSynchronous()).
	static const FIHTerrainStampMeshRow* Get(EIHTerrainStampId StampId);

private:
	static UDataTable* LoadCatalogTable();
};

// Canonical Terrain Stamp interaction/HUD colors (2026-09-09) - ported verbatim from the
// IH_WB_Stamps001 lab project's own IHTerrainStampColors, where these were the user's own approved
// interaction/color spec. AllStampsToggleColor is reused here for a second purpose: an AVAILABLE
// (real-mesh-backed) W-grid catalog button's label tint - distinct from, but palette-consistent
// with, SelectedStampColor (used once a stamp is actually placed and selected in-world).
// TransformGripColor/ExtensionPullGripColor/StampSplineColor are not consumed by this round (the
// bounding-box move/rotate/scale transform system is a follow-up round) - kept here now so that
// round doesn't need to re-derive or re-port them.
namespace IHTerrainStampColors
{
	inline const FColor AllStampsToggleColor(0x9A, 0xE6, 0x30);   // #9AE630
	inline const FColor SelectedStampColor(0x31, 0xC9, 0x50);     // #31C950
	// 2026-09-18 (user request): matches the canonical Midlands ASL band color (DT_ASLSlopeBiome.csv,
	// biomeColor=00FF00) instead of an unrelated teal - was #31D492.
	inline const FColor BoundingBoxGripColor(0x00, 0xFF, 0x00);   // #00FF00
	inline const FColor TransformGripColor(0x1F, 0x7A, 0x55);     // #1F7A55
	inline const FColor ExtensionPullGripColor(0x07, 0x5F, 0x5A); // #075F5A
	inline const FColor StampSplineColor(0x17, 0x82, 0x36);       // #178236
}
