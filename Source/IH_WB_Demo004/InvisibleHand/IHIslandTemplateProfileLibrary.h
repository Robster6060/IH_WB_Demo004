// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHIslandTemplateProfileTypes.h"
#include "IH_P1C08_IslandCoastlineTuning.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IHIslandTemplateProfileLibrary.generated.h"

UCLASS()
class IH_WB_DEMO004_API UIHIslandTemplateProfileLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 GallerySeedsPerTemplate = 12;
	static constexpr int32 GalleryZoomLevelCount = 3;
	static constexpr int32 GalleryViewModeCount = 3;

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Template Profile")
	static FName GetCanonicalProfileSetId() { return FName(TEXT("P1C09_TemplateProfiles_v1")); }

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Template Profile")
	static FIHIslandTemplateProfileV1 BuildDefaultProfileV1(EIHIslandTemplateType TemplateType);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Template Profile")
	static void GetLockedCanonicalProfilesV1(TArray<FIHIslandTemplateProfileV1>& OutProfiles);

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Template Profile")
	static bool FindProfileV1(
		EIHIslandTemplateType TemplateType,
		const TArray<FIHIslandTemplateProfileV1>& Profiles,
		FIHIslandTemplateProfileV1& OutProfile);

	/** Maps profile knobs into harness coastline tuning for tank preview. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Template Profile")
	static FIHIslandCoastlineTuning MakeCoastlineTuningFromProfile(const FIHIslandTemplateProfileV1& Profile);

	/** Seed Nav committed tuning from Phase 1 template assignments (canonical profiles, not user-edited). */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Template Profile", meta = (WorldContext = "WorldContextObject"))
	static void ApplyPhase1ProfilesToCommittedTuning(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Template Profile")
	/** Loads Content profile assets when present (LOAD_Quiet); missing slots use BuildDefaultProfileV1. */
	static bool TryLoadProfilesFromContent(TArray<FIHIslandTemplateProfileV1>& OutProfiles);

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Gallery")
	static FString GalleryViewModeToString(EIHIslandGalleryViewMode Mode);

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Gallery")
	static FString GalleryZoomLevelToString(EIHIslandGalleryZoomLevel Zoom);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Gallery")
	static void BuildGalleryReviewMatrix(TArray<FIHIslandTemplateGalleryCell>& OutCells);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Gallery")
	static FString FormatGalleryReviewManifest(const TArray<FIHIslandTemplateGalleryCell>& Cells);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Gallery")
	static void LogGalleryReviewManifest();
};
