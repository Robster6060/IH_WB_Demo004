// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_ASLSlopeBiome.csv (IH-DEC-052)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_ASLSlopeBiomeRow.generated.h"

/**
 * One elevation/slope biome band — the canonical PCG-derived biome classification lookup for a
 * sampled point on an IslandMesh, keyed on (ASL, Slope, LatitudeZone).
 *
 * Deliberately the simpler of two competing draft designs (ASLSlopeBiomeChart002.xlsx, ~45 rows,
 * per-zone Yes/No eligibility flags) rather than the heavier alternative (ASLSlopeBiome001.md,
 * 324 rows, one explicit row per Latitude x ASL x Slope, RuleOrder/ReviewStatus/typed-struct
 * rigor) — see IH-DEC-052: too few real differences between latitude zones to justify that
 * complexity. Latitude eligibility is 3 independent bools, not mutually exclusive — a biome band
 * can be valid in more than one zone (e.g. Volcanic Rim is Nordic+Temperate+Tropical; Glacier is
 * Nordic+Temperate only). Canonical zone boundaries (IH-DEC-051): Nordic >=70 deg N,
 * Temperate 30-70 deg N, Tropical 0-30 deg N — those boundary values have no runtime consequence
 * here, since RealmLatitudeZone is resolved to one of the 3 zones once, at World Builder time,
 * before this table is ever queried.
 *
 * Features and Resources are merged into a single `features` array (IH-DEC-052 resolution of
 * ASLSlopeBiome002.md's Open Item A — the source data split landform content inconsistently
 * across the two columns with no clear rule for which went where). RGB is dropped (ASLSlopeBiome002.md
 * Open Item B — always equal to grayValue on every row; derive a packed color from grayValue
 * directly if one is ever needed).
 *
 * CSV / DataTable (DT_ASLSlopeBiome):
 *   Header: ---,biomeID,sortOrder,... (UE row-name column + struct fields; row name = biomeID value)
 *   Editor import: Row Type = FIHASLSlopeBiomeRow, Import Key Field = (empty)
 *   Array columns (features, minerals, flora, fauna, crops, hazards) use UE's CSV array syntax:
 *   a parenthesized, comma-separated list, e.g. "(Caverns,Aquifer)"; a single value still needs
 *   parentheses, e.g. "(Timber)"; leave the cell blank for an empty array.
 */
USTRUCT(BlueprintType)
struct FIHASLSlopeBiomeRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable synthetic key, e.g. "ASLB_001".."ASLB_045". Also the DataTable RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FName biomeID;

	/** Preserves the original source chart's Order column, for traceability only — not used at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	int32 sortOrder = 0;

	/** Source/reference heightmap grayscale key (0-255). Reference/visualization metadata, not the
	 * primary runtime classifier — ASL/Slope/Zone drive lookup, not grayValue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	uint8 grayValue = 0;

	/** Mountainous / Highlands / Midlands / Lowlands / Upland Coastal / Wet Coastal / Shallow /
	 * Verdant / Harborage / Deep / Abysmal. Plain FName for now (matches this project's own
	 * precedent of starting categorization fields as FName before formalizing into a UENUM once
	 * cross-system usage proves stable — see FIHBuildPaletteItemRow::structureCategory). Keys the
	 * mineral-eligibility lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FName terrainTier;

	/** Upper (shallower/higher) elevation bound of the band, meters. Inclusive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	float aslUpperM = 0.f;

	/** Lower (deeper/lower) elevation bound of the band, meters. Can be negative (underwater). Inclusive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	float aslLowerM = 0.f;

	/** Lower bound of slope angle, degrees. Meaningless when bSlopeAgnostic is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	float minSlopeDeg = 0.f;

	/** Upper bound of slope angle, degrees. Meaningless when bSlopeAgnostic is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	float maxSlopeDeg = 0.f;

	/** True only for Abysmal (flat sea floor, no meaningful slope band) — an explicit flag rather
	 * than a numeric sentinel (the source chart's old "156"/"nan" slope-field sentinels were a real,
	 * already-fixed defect in the earlier drafts; this field exists specifically so that mistake
	 * can't recur here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bSlopeAgnostic = false;

	/** Human-readable slope range, e.g. "11-25 deg". Display/debug convenience only — derive, don't hand-edit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FString slopeRangeLabel;

	/** Player/designer-facing label, e.g. "High Timberland". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FText biomeName;

	/** Eligible when RealmLatitudeZone == Nordic. Not mutually exclusive with the other two zone flags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bZoneNordic = true;

	/** Eligible when RealmLatitudeZone == Temperate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bZoneTemperate = true;

	/** Eligible when RealmLatitudeZone == Tropical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bZoneTropical = true;

	/** Eligible natural/terrain/gameplay features — merged Features+Resources (IH-DEC-052). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> features;

	/** Eligible surface-extractable minerals. Eligibility only — never a guaranteed spawn; deposit
	 * occurrence/abundance/grade/depth belongs to the mineral-generation system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> quarryPitMinerals;

	/** Eligible shaft-mined minerals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> deepMineMinerals;

	/** Eligible drilled resources (gas/oil/helium). Blank where the source Minerals Chart lists none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> derrickMinerals;

	/** Eligible cave-system minerals. Gated in the source data to rows whose Features text mentions Caverns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> cavernMinerals;

	/** Eligible latitude- and biome-appropriate vegetation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> flora;

	/** Eligible latitude- and biome-appropriate wildlife. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> fauna;

	/** Eligible farmable/harvestable crops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> crops;

	/** Eligible environmental/gameplay hazards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	TArray<FName> hazards;

	/** False only on the SEA LEVEL marker row (a zero-thickness datum divider, not a real biome
	 * band) — PCG lookup must filter on this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bPcgEligible = true;
};
