// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_ASLSlopeBiome.csv (IH-DEC-056: 48-row landform grid,
// superseding IH-DEC-052's 45-row and IH-DEC-054's 20-row ecology-biome designs)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_ASLSlopeBiomeRow.generated.h"

/**
 * One elevation/slope landform band — the canonical PCG-derived classification lookup for a
 * sampled point on an IslandMesh, keyed on (ASL, Slope).
 *
 * IH-DEC-056: full replacement of the prior 20-row ecology-named design (Marsh/Timberland/
 * Tundra/Desert/etc.) with a symmetric 48-row landform-morphology grid, authored by the user in
 * `ASLSlopeBiomeChart007.xlsx` — 7 elevation tiers (WWF -25-0m, Shorelands 1-200m, Lowlands
 * 201-600m, Midlands 601-1100m, Highlands 1101-1600m, Montane 1601-2000m, Alpine 2001-2400m) x 7
 * slope-types (Flat 0-5 deg, Rolling 6-10 deg, Rugged 11-25 deg, Steep 26-45 deg, Craggy 46-80
 * deg, Sheer 81-90 deg, Basin [no degree range]), WWF has no Basin variant = 48 rows. Row names
 * describe landform shape (Coastal Plain, Midland Hills, Highland Crags, Alpine Ridges), not
 * ecology — ecology/resource content is layered on top via the mapping rules below, not baked
 * into the name. Slope-type naming shifted from the prior 20-row scheme: what was called "Sloped"
 * (11-25 deg) is now "Rugged," and what was called "Rugged" (46-80 deg) is now "Craggy" — same
 * degree ranges, different names; don't assume "Rugged" means what it used to.
 *
 * Basin is a real dual-origin terrain feature (heightmap-generated depressions AND
 * Hydrology-Phase-spawned ones), not a placeholder — every Basin row carries both dry content
 * (`quarryPitMinerals` — quarries and pits) and default wet content (Reeds/Papyrus/Rice/
 * Shellfish/Fishing/Game), with a Nordic-zone override to Ice/Glacier content resolved entirely
 * through `DT_BiomeTagZoneExemption` (the wet-content tags are Nordic-exempt there; Ice/Glacier
 * are already Tropical-exempt) — no per-row branching needed. Basin rows use `bSlopeAgnostic=true`
 * with `slopeRangeLabel="Basin"` rather than a real degree range.
 *
 * Content-mapping rules (user-authored, applied programmatically per slope-type/tier, not
 * hand-copied per row): Quarry Pit minerals -> Basin only; Deep Mine -> Steep+Craggy; Derrick ->
 * Flat+Rolling+Rugged; Cavern -> Craggy+Sheer; water flora/Fishing/Shellfish -> WWF Flat +
 * Shorelands Basin; Grassland/Timber/Woodland/Orchards/Row Crops -> Shorelands..Montane,
 * Flat..Rugged; Flowers/Herbs/Apiary/Game -> Shorelands..Alpine, Flat..Craggy; hazards ->
 * Typhoon+Waterspout on WWF+Shorelands, Wildfire wherever Grassland/Timber/Woodland flora is
 * present (Shorelands..Montane), Tornado on Flat..Rugged within Lowlands..Midlands.
 *
 * All 48 rows are zone-agnostic (`bZoneNordic`/`Temperate`/`Tropical` = true) — zone restriction
 * happens entirely through `DT_BiomeTagZoneExemption` (IH-DEC-053 Mechanism A), keyed to
 * individual content tags, not rows. Canonical zone boundaries (IH-DEC-051): Nordic >=70 deg N,
 * Temperate 30-70 deg N, Tropical 0-30 deg N — no runtime consequence here; `RealmLatitudeZone`
 * is resolved once at World Builder time, before this table or the registry is ever queried.
 *
 * `biomeColor` is the user's own hex per row from `ASLSlopeBiomeChart007.xlsx`, not derived.
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

	/** Stable synthetic key, e.g. "ASLB_001".."ASLB_048". Also the DataTable RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FName biomeID;

	/** Preserves the original source chart's Order column, for traceability only — not used at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	int32 sortOrder = 0;

	/** Artist-authored display color for this biome band, from the source chart's own Biome Name
	 * cell fill (Topygraphy Elevation Chart.xlsx, Column I) — not derived or procedural. 6-hex-digit
	 * RRGGBB, no leading '#' (e.g. "009900" for High Timberland). sRGB as authored in Excel; convert
	 * via FLinearColor::FromSRGBColor(FColor::FromHex(Hex)) before feeding a MID's linear color
	 * param — FColor::ReinterpretAsLinear() is the wrong call here (naive byte-divide, no gamma
	 * decode) and would read visibly too dark. Reference/visualization metadata, not the
	 * primary runtime classifier — ASL/Slope/Zone drive lookup, not biomeColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FString biomeColor;

	/** WWF / Shorelands / Lowlands / Midlands / Highlands / Montane / Alpine — the 7 elevation
	 * tiers (IH-DEC-056). Plain FName for now (matches this project's own precedent of starting
	 * categorization fields as FName before formalizing into a UENUM once cross-system usage
	 * proves stable — see FIHBuildPaletteItemRow::structureCategory). */
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

	/** True on the 6 "Basin" rows (one per tier except WWF) — Basin has no real degree range
	 * (`slopeRangeLabel="Basin"` instead), an explicit flag rather than a numeric sentinel (the
	 * source chart's old "156"/"nan" slope-field sentinels were a real, already-fixed defect in
	 * earlier drafts; this field exists specifically so that mistake can't recur). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bSlopeAgnostic = false;

	/** Human-readable slope range, e.g. "11-25 deg". Display/debug convenience only — derive, don't hand-edit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FString slopeRangeLabel;

	/** Player/designer-facing label, e.g. "Highland Crags". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	FText biomeName;

	/** Always true post-IH-DEC-056 — landform rows are zone-agnostic by design; zone restriction
	 * happens per content tag via DT_BiomeTagZoneExemption instead. Kept as a real field (not
	 * removed) since a future landform-level zone restriction is plausible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bZoneNordic = true;

	/** See bZoneNordic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|ASL Slope Biome")
	bool bZoneTemperate = true;

	/** See bZoneNordic. */
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
};
