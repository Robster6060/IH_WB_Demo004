// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Blueprint/DataTable enums for Right Build Palette and Town Grid tables.

#pragma once

#include "CoreMinimal.h"
#include "IH_BuildPaletteTypes.generated.h"

/** Right Build Palette fly-out tab (G / W / B / C / D). */
UENUM(BlueprintType)
enum class EIHBuildPaletteTab : uint8
{
	Grid = 0 UMETA(DisplayName = "Grid"),
	World UMETA(DisplayName = "World"),
	Build UMETA(DisplayName = "Build"),
	Convey UMETA(DisplayName = "Convey"),
	Defense UMETA(DisplayName = "Defense"),
};

/** How a palette tile interacts when selected. */
UENUM(BlueprintType)
enum class EIHBuildPaletteInteraction : uint8
{
	DropActor = 0 UMETA(DisplayName = "DropActor"),
	SplineOpen UMETA(DisplayName = "SplineOpen"),
	SplineClosed UMETA(DisplayName = "SplineClosed"),
	PaintBrush UMETA(DisplayName = "PaintBrush"),
	GripTemplate UMETA(DisplayName = "GripTemplate"),
	CompositePackage UMETA(DisplayName = "CompositePackage"),
	SliderPanel UMETA(DisplayName = "SliderPanel"),
	/** W tab — terrain heightfield stamp (B2b). */
	TerrainStamp UMETA(DisplayName = "TerrainStamp"),
};

/** Minimum game mode / layer where the palette item is available. */
UENUM(BlueprintType)
enum class EIHBuildPaletteLevel : uint8
{
	WorldBuilder = 0 UMETA(DisplayName = "WorldBuilder"),
	MainGame UMETA(DisplayName = "MainGame"),
	Combat UMETA(DisplayName = "Combat"),
};

/** Canonical five town grid GIS Blueprint Layer templates (T1–T5). */
UENUM(BlueprintType)
enum class EIHTownGridTemplate : uint8
{
	Squared = 0 UMETA(DisplayName = "Squared"),
	Harmonic UMETA(DisplayName = "Harmonic"),
	Radial UMETA(DisplayName = "Radial"),
	Citadel UMETA(DisplayName = "Citadel"),
	Valley UMETA(DisplayName = "Valley"),
};

/** Parcel zone gate for palette items and default commons zoning. */
UENUM(BlueprintType)
enum class EIHParcelZoneCode : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	RES UMETA(DisplayName = "RES"),
	WWF UMETA(DisplayName = "WWF"),
	LID UMETA(DisplayName = "LID"),
	CHA UMETA(DisplayName = "CHA"),
	CIV UMETA(DisplayName = "CIV"),
	SPD UMETA(DisplayName = "SPD"),
};

/** Harmonic block placement axis (lookup table only). */
UENUM(BlueprintType)
enum class EIHHarmonicCardinalAxis : uint8
{
	Cardo = 0 UMETA(DisplayName = "Cardo"),
	Decumanus UMETA(DisplayName = "Decumanus"),
	Interior UMETA(DisplayName = "Interior"),
};

/** Stub for future structure-category gating; use FName on rows until expanded. */
UENUM(BlueprintType)
enum class EIHStructureCategory : uint8
{
	None = 0 UMETA(DisplayName = "None"),
};
