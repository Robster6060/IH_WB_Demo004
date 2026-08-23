// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHIslandTemplateProfileLibrary.h"
#include "IH_P1C08_CoastlineTuningSubsystem.h"
#include "IH_P1C08_IslandCoastlineTuning.h"
#include "Engine/World.h"

FIHIslandTemplateProfileV1 UIHIslandTemplateProfileLibrary::BuildDefaultProfileV1(EIHIslandTemplateType TemplateType)
{
	FIHIslandTemplateProfileV1 Profile;
	Profile.TemplateType = TemplateType;
	Profile.ProfileId = FName(*FString::Printf(TEXT("Default_%d"), static_cast<int32>(TemplateType)));
	return Profile;
}

void UIHIslandTemplateProfileLibrary::GetLockedCanonicalProfilesV1(TArray<FIHIslandTemplateProfileV1>& OutProfiles)
{
	OutProfiles.Reset();
	OutProfiles.Add(BuildDefaultProfileV1(EIHIslandTemplateType::Low));
	OutProfiles.Add(BuildDefaultProfileV1(EIHIslandTemplateType::High));
	OutProfiles.Add(BuildDefaultProfileV1(EIHIslandTemplateType::Volcanic));
}

bool UIHIslandTemplateProfileLibrary::FindProfileV1(
	EIHIslandTemplateType TemplateType,
	const TArray<FIHIslandTemplateProfileV1>& Profiles,
	FIHIslandTemplateProfileV1& OutProfile)
{
	for (const FIHIslandTemplateProfileV1& Profile : Profiles)
	{
		if (Profile.TemplateType == TemplateType)
		{
			OutProfile = Profile;
			return true;
		}
	}
	OutProfile = BuildDefaultProfileV1(TemplateType);
	return false;
}

FIHIslandCoastlineTuning UIHIslandTemplateProfileLibrary::MakeCoastlineTuningFromProfile(const FIHIslandTemplateProfileV1& Profile)
{
	return FIHIslandCoastlineTuning::SeedBaseline();
}

void UIHIslandTemplateProfileLibrary::ApplyPhase1ProfilesToCommittedTuning(const UObject* WorldContextObject)
{
	// P1C10: coast shape comes from cell pipeline — no template tuning apply.
}

bool UIHIslandTemplateProfileLibrary::TryLoadProfilesFromContent(TArray<FIHIslandTemplateProfileV1>& OutProfiles)
{
	GetLockedCanonicalProfilesV1(OutProfiles);
	return true;
}

FString UIHIslandTemplateProfileLibrary::GalleryViewModeToString(EIHIslandGalleryViewMode Mode)
{
	switch (Mode)
	{
	case EIHIslandGalleryViewMode::Relief: return TEXT("Relief");
	case EIHIslandGalleryViewMode::Shelf: return TEXT("Shelf");
	default: return TEXT("Coast");
	}
}

FString UIHIslandTemplateProfileLibrary::GalleryZoomLevelToString(EIHIslandGalleryZoomLevel Zoom)
{
	switch (Zoom)
	{
	case EIHIslandGalleryZoomLevel::Medium: return TEXT("Medium");
	case EIHIslandGalleryZoomLevel::Close: return TEXT("Close");
	default: return TEXT("Wide");
	}
}

void UIHIslandTemplateProfileLibrary::BuildGalleryReviewMatrix(TArray<FIHIslandTemplateGalleryCell>& OutCells)
{
	OutCells.Reset();
}

FString UIHIslandTemplateProfileLibrary::FormatGalleryReviewManifest(const TArray<FIHIslandTemplateGalleryCell>& Cells)
{
	return TEXT("P1C10 Azgaar — template gallery disabled.");
}

void UIHIslandTemplateProfileLibrary::LogGalleryReviewManifest()
{
	UE_LOG(LogTemp, Log, TEXT("%s"), *FormatGalleryReviewManifest({}));
}
