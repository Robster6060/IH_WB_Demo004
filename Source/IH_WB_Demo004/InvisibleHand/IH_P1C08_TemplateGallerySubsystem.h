// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHIslandTemplateProfileTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C08_TemplateGallerySubsystem.generated.h"

UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_TemplateGallerySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Gallery")
	const TArray<FIHIslandTemplateProfileV1>& GetLockedProfiles() const { return LockedProfiles; }

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Gallery")
	const TArray<FIHIslandTemplateGalleryCell>& GetGalleryCells() const { return GalleryCells; }

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Gallery")
	int32 GetActiveGalleryIndex() const { return ActiveGalleryIndex; }

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Gallery")
	bool GetActiveGalleryCell(FIHIslandTemplateGalleryCell& OutCell) const;

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Gallery")
	bool GetProfileForTemplate(EIHIslandTemplateType TemplateType, FIHIslandTemplateProfileV1& OutProfile) const;

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Gallery")
	void ReloadCanonicalProfilesV1();

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Gallery")
	void SetActiveGalleryIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Gallery")
	void StepGalleryIndex(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Gallery")
	void LogGalleryManifest() const;

private:
	UPROPERTY()
	TArray<FIHIslandTemplateProfileV1> LockedProfiles;

	UPROPERTY()
	TArray<FIHIslandTemplateGalleryCell> GalleryCells;

	int32 ActiveGalleryIndex = 0;
};
