// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_PGCMeshCatalog.csv

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_PGCMeshCatalogRow.generated.h"

/**
 * Which PGC scatter pass a catalog row belongs to. Mirrors DT_BiomeRecommendations' 5 columns
 * (groundcover/trees/rocks/plants/props) so the future scatter system can apply per-category
 * placement rules (density, slope bias, HISM vs sparse single-actor placement) without having
 * to infer category from the tag name itself.
 */
UENUM(BlueprintType)
enum class EIHPGCMeshCategory : uint8
{
	Groundcover,
	Trees,
	Rocks,
	Plants,
	Props
};

/**
 * Resolves one DT_BiomeRecommendations tag (e.g. "MeadowGrass", "RockOutcrop") to the real
 * static mesh(es) that should be scattered for it, joined by tag = DataTable RowName.
 *
 * 2026-09-18: first pass, scoped to what actually exists in Content today (confirmed via a full
 * Content search) — NOT the full 48-row tag vocabulary. Groundcover and Rocks have decent real
 * coverage; Trees have zero matching assets anywhere in the project and every Trees-category row
 * is deliberately left with an empty `meshes` array (the scatter system must treat an empty array
 * as "skip this tag", not an error) until a tree/conifer asset pack is imported. Most Props tags
 * (Driftwood, FishingNet, StoneWall, Hayrick, FencePost, LogPile, Woodpile) are empty for the same
 * reason — no matching assets exist. Cairn/RockPile/SummitCairn are the exception: a cairn is
 * literally a small rock pile, so they're mapped to real Rock_Collection_04 meshes, not a
 * placeholder. Plants entries reuse OWD_Plants_Pack's ~40 generic SM_Plant_XX_Y_G meshes, split
 * into a "flower-leaning" and "leafy/shrub-leaning" pool by filename index alone (the pack gives
 * no per-mesh species/shape metadata) — treat this split as a coarse first guess to be corrected
 * after an in-editor visual pass, not a verified mapping.
 *
 * CSV / DataTable (DT_PGCMeshCatalog):
 *   Header: ---,tag,category,meshes,uniformScaleMin,uniformScaleMax,bRandomizeYaw
 *   Row name = tag value (same convention as DT_ASLSlopeBiome/DT_BiomeRecommendations).
 *   `meshes` is a parenthesized comma-separated list of full soft object paths, e.g.
 *   "(/Game/Rock_Collection_04/Meshes/Rock_01/StaticMeshes/SM_Rock_01.SM_Rock_01)"; leave the
 *   cell as empty parentheses "()" for a tag with no available mesh yet (never omit the row —
 *   keeping every tag present, even empty, makes future asset backfilling a one-cell edit).
 */
USTRUCT(BlueprintType)
struct FIHPGCMeshCatalogRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Join key against DT_BiomeRecommendations' groundcover/trees/rocks/plants/props tags. Also
	 * the DataTable RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|PGC Mesh Catalog")
	FName tag;

	/** Which scatter pass this tag belongs to; drives per-category placement rules in the future
	 * scatter system (density, slope bias, HISM vs sparse single-actor placement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|PGC Mesh Catalog")
	EIHPGCMeshCategory category = EIHPGCMeshCategory::Groundcover;

	/** Candidate mesh pool for this tag; the scatter system picks one at random per instance for
	 * visual variety. Empty means no asset exists yet for this tag — must be treated as "skip",
	 * not an error, by any consumer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|PGC Mesh Catalog")
	TArray<TSoftObjectPtr<UStaticMesh>> meshes;

	/** Per-instance uniform scale is randomized within [uniformScaleMin, uniformScaleMax]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|PGC Mesh Catalog")
	float uniformScaleMin = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|PGC Mesh Catalog")
	float uniformScaleMax = 1.15f;

	/** Whether each instance gets a random yaw rotation (true for organic clutter like grass/
	 * rocks/plants; a future oriented prop type would set this false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|PGC Mesh Catalog")
	bool bRandomizeYaw = true;
};
