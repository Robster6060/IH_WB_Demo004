// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IHMapSeedFrameworkTypes.h"
#include "IHIslandTemplateProfileTypes.generated.h"

UENUM(BlueprintType)
enum class EIHIslandGalleryViewMode : uint8
{
	Coast = 0,
	Relief,
	Shelf,
};

UENUM(BlueprintType)
enum class EIHIslandGalleryZoomLevel : uint8
{
	Wide = 0,
	Medium,
	Close,
};

USTRUCT(BlueprintType)
struct FIHIslandTemplateProfileV1
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EIHIslandTemplateType TemplateType = EIHIslandTemplateType::Low;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ProfileId = NAME_None;
};

USTRUCT(BlueprintType)
struct FIHIslandTemplateGalleryCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GalleryIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EIHIslandTemplateType TemplateType = EIHIslandTemplateType::Low;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EIHIslandGalleryViewMode ViewMode = EIHIslandGalleryViewMode::Coast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EIHIslandGalleryZoomLevel ZoomLevel = EIHIslandGalleryZoomLevel::Wide;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ReviewSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ReviewSeedWord;
};
