// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_BiomeTagZoneExemption.csv (IH-DEC-053)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_BiomeTagZoneExemptionRow.generated.h"

/**
 * "Mechanism A" of the two-mechanism Latitude x DT_ASLSlopeBiome x PGC design (IH-DEC-053):
 * a small registry of individual content tags (from DT_ASLSlopeBiome's features/minerals/
 * flora/fauna/crops/hazards columns) that must be fully ABSENT in one or more latitude zones,
 * regardless of which biome row they're attached to or whether that row is itself zone-agnostic.
 *
 * This exists because DT_ASLSlopeBiome's own bZoneNordic/Temperate/Tropical flags only gate at
 * the whole-row level — a zone-agnostic row's content tags are not otherwise filtered per zone.
 * Real example that motivated this table: "Coral" appears on 6 fully zone-agnostic rows (Beach,
 * Coastal Cliffs, Delta, Estuary, Low Riverine, River), which would let a Nordic beach spawn
 * coral under row-level gating alone.
 *
 * Deliberately NOT the mechanism for "same tag, different look per zone" (e.g. a generic Timber
 * tag rendering as Fir in Nordic vs Palm in Tropical) — that's "Mechanism B", handled entirely by
 * Phase 4's FAB asset cross-reference (same abstract tag, different concrete asset set chosen per
 * zone), with no DataTable involved at all. This table is only for tags that must vanish outright.
 *
 * A tag with no row in this table is implicitly zone-agnostic (no exemption) — most of
 * DT_ASLSlopeBiome's ~81 distinct content tags need no entry here at all.
 *
 * Not yet consumed by any runtime code — Phase 3 (Latitude selector) doesn't exist yet, so there
 * is no "current realm latitude" to filter by. This table is pure data, authored ahead of its
 * consumer, same situation DT_ASLSlopeBiome's own zone bools were in before this session.
 *
 * CSV / DataTable (DT_BiomeTagZoneExemption):
 *   Header: ---,tag,bExemptNordic,bExemptTemperate,bExemptTropical,notes (row name = tag value)
 */
USTRUCT(BlueprintType)
struct FIHBiomeTagZoneExemptionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Must exactly match a tag string used somewhere in DT_ASLSlopeBiome's features/mineral/
	 * flora/fauna/crops/hazards columns (e.g. "Coral", "Ice", "Hg"). Also the DataTable RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Tag Zone Exemption")
	FName tag;

	/** True if this tag must never spawn in a Nordic-zone realm, regardless of which biome row
	 * or ASL/slope band it would otherwise match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Tag Zone Exemption")
	bool bExemptNordic = false;

	/** True if this tag must never spawn in a Temperate-zone realm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Tag Zone Exemption")
	bool bExemptTemperate = false;

	/** True if this tag must never spawn in a Tropical-zone realm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Tag Zone Exemption")
	bool bExemptTropical = false;

	/** Human-readable reasoning for the exemption, e.g. "Coral requires warm water; excluded from
	 * Nordic regardless of ASL/slope band." Display/authoring aid only, not read at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Biome Tag Zone Exemption")
	FString notes;
};
