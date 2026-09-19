// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_BiomeRecommendations.csv

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_BiomeRecommendationsRow.generated.h"

/**
 * PGC (procedurally generated content) scatter recommendations for one DT_ASLSlopeBiome row,
 * joined 1:1 by biomeID against that table's 48 rows (ASLB_001..ASLB_048).
 *
 * 2026-09-18: this is data-design groundwork for a future PGC scattering round - nothing in the
 * codebase consumes this table yet (no InstancedFoliageActor/HierarchicalInstancedStaticMesh/PCG
 * usage exists anywhere in this project today). Columns mirror the 5 categories already pre-staged
 * (but left empty) as columns J-N in the user's own ASLSlopeBiomeFinalChart.xlsx: Groundcover,
 * Trees, Rocks, Plants, Props. Content is generated programmatically per (terrainTier, slopeType)
 * pair from DT_ASLSlopeBiome — same "rules per tier/slope, not hand-copied per row" convention that
 * table's own flora/fauna/mineral columns already use — not authored per individual row.
 *
 * Deliberately zone-agnostic (Nordic/Temperate/Tropical): DT_ASLSlopeBiome's own bZoneNordic/
 * Temperate/Tropical flags are all still `true` today (an inert placeholder - no latitude selector
 * exists yet), so these recommendations describe latitude-neutral content that won't be invalidated
 * once that selector is built in a later round; zone-specific variants are a refinement for then,
 * not now.
 *
 * CSV / DataTable (DT_BiomeRecommendations):
 *   Header: ---,biomeID,groundcover,trees,rocks,plants,props (UE row-name column + struct fields;
 *   row name = biomeID value, same convention as DT_ASLSlopeBiome).
 *   Editor import: Row Type = FIHBiomeRecommendationsRow, Import Key Field = (empty)
 *   Array columns use UE's CSV array syntax: a parenthesized, comma-separated list, e.g.
 *   "(SparseAlpineGrass,Lichen)"; a single value still needs parentheses; leave the cell blank
 *   for an empty array (e.g. Sheer-slope rows have no groundcover/trees/plants/props at all).
 */
USTRUCT(BlueprintType)
struct FIHBiomeRecommendationsRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Join key against DT_ASLSlopeBiome's own biomeID, e.g. "ASLB_001".."ASLB_048". Also the
	 * DataTable RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Recommendations")
	FName biomeID;

	/** Low ground-clutter scatter candidates (grasses, moss, lichen, snow/bare-soil patches). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Recommendations")
	TArray<FName> groundcover;

	/** LOD tree scatter candidates. Empty above the treeline (Highlands Steep+ / all Montane &
	 * Alpine except the gentlest Basin/Flat) and on Sheer slopes everywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Recommendations")
	TArray<FName> trees;

	/** LOD rock scatter candidates - boulders, outcrops, scree, exposed bedrock, cliff faces.
	 * Density/prominence rises with slope steepness in the recommendation logic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Recommendations")
	TArray<FName> rocks;

	/** Understory/accent plant scatter candidates (ferns, flowers, shrubs, kelp/seagrass for WWF). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Recommendations")
	TArray<FName> plants;

	/** Small built/found prop scatter candidates (cairns, driftwood, stone walls, log piles). Empty
	 * on Craggy/Sheer slopes and the highest Alpine tiers - too exposed/inhospitable to place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Recommendations")
	TArray<FName> props;
};
