// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_TemplateGallerySubsystem.h"

#include "IHIslandTemplateProfileLibrary.h"

void UIH_P1C08_TemplateGallerySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadCanonicalProfilesV1();
}

bool UIH_P1C08_TemplateGallerySubsystem::GetActiveGalleryCell(FIHIslandTemplateGalleryCell& OutCell) const
{
	if (!GalleryCells.IsValidIndex(ActiveGalleryIndex))
	{
		return false;
	}
	OutCell = GalleryCells[ActiveGalleryIndex];
	return true;
}

bool UIH_P1C08_TemplateGallerySubsystem::GetProfileForTemplate(
	EIHIslandTemplateType TemplateType,
	FIHIslandTemplateProfileV1& OutProfile) const
{
	return UIHIslandTemplateProfileLibrary::FindProfileV1(TemplateType, LockedProfiles, OutProfile);
}

void UIH_P1C08_TemplateGallerySubsystem::ReloadCanonicalProfilesV1()
{
	LockedProfiles.Reset();
	if (!UIHIslandTemplateProfileLibrary::TryLoadProfilesFromContent(LockedProfiles))
	{
		UIHIslandTemplateProfileLibrary::GetLockedCanonicalProfilesV1(LockedProfiles);
	}
	UIHIslandTemplateProfileLibrary::BuildGalleryReviewMatrix(GalleryCells);
	ActiveGalleryIndex = 0;
}

void UIH_P1C08_TemplateGallerySubsystem::SetActiveGalleryIndex(int32 Index)
{
	if (GalleryCells.Num() == 0)
	{
		ActiveGalleryIndex = 0;
		return;
	}
	ActiveGalleryIndex = FMath::Clamp(Index, 0, GalleryCells.Num() - 1);
}

void UIH_P1C08_TemplateGallerySubsystem::StepGalleryIndex(int32 Delta)
{
	SetActiveGalleryIndex(ActiveGalleryIndex + Delta);
}

void UIH_P1C08_TemplateGallerySubsystem::LogGalleryManifest() const
{
	UE_LOG(LogTemp, Log, TEXT("%s"), *UIHIslandTemplateProfileLibrary::FormatGalleryReviewManifest(GalleryCells));
}
