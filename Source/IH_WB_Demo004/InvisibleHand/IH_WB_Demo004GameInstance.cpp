// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_WB_Demo004GameInstance.h"

#include "IH_WB_Demo004GameMode.h"

#include "IHMapSeedFrameworkLibrary.h"
#include "IHIslandTemplateProfileLibrary.h"
#include "IH_P1C08_TemplateGallerySubsystem.h"
#include "IHSeedIslandLibrary.h"
#include "IHSeedValidationLibrary.h"



void UIH_WB_Demo004GameInstance::Init()

{

	Super::Init();

	// Legacy PIE sessions may carry older dev defaults (0.69 blob, 0.58 intermediate, 0.50 lane-fit trial).
	if (FMath::IsNearlyEqual(DevLandAreaFraction, 0.69f)
		|| FMath::IsNearlyEqual(DevLandAreaFraction, 0.58f)
		|| FMath::IsNearlyEqual(DevLandAreaFraction, 0.50f)
		|| FMath::IsNearlyEqual(DevLandAreaFraction, 0.65f)
		|| FMath::IsNearlyEqual(DevLandAreaFraction, 0.70f))
	{
		TargetEffectiveLandFraction = IHInvisibleHandSpec::DefaultTargetEffectiveLandFraction;
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("GameInstance: migrated land budget -> TargetEffectiveLandFraction %.2f (achieved drives acres)."),
			TargetEffectiveLandFraction);
	}

	if (FMath::IsNearlyEqual(RealmHalfExtentNSKm, 12.f) || FMath::IsNearlyEqual(RealmHalfExtentNSKm, 6.f))
	{
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("GameInstance: migrated RealmHalfExtentNSKm %.1f -> %.1f (26×~42 km φ realm)."),
			RealmHalfExtentNSKm, IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm);
		RealmHalfExtentNSKm = IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm;
	}

	// Headless multi-seed verification: -RealmSeed=<seed> lets automated -game runs regenerate
	// a specific realm without going through the DevSeedPanel widget's click path, which isn't
	// reachable from a pure -ExecCmds/-game invocation.
	FString CommandLineSeedOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("RealmSeed="), CommandLineSeedOverride)
		&& !CommandLineSeedOverride.IsEmpty())
	{
		CurrentWorldSeed = CommandLineSeedOverride;
	}

	if (CurrentWorldSeed.IsEmpty())

	{

		CurrentWorldSeed = TEXT("ABBEY3");

	}

	CurrentWorldSeed = UIHSeedValidationLibrary::NormalizeSeedString(CurrentWorldSeed);

	MasterSeedInt = UIHSeedValidationLibrary::SeedStringToMasterInt32(CurrentWorldSeed);



	const int32 FromSeed = UIHSeedValidationLibrary::ExtractIslandCountFromEightCharSeed(CurrentWorldSeed);

	ProceduralIslandCount = (FromSeed >= 2 && FromSeed <= 7) ? FromSeed : 3;

	RebuildMapSeedPhase1(false);
}

void UIH_WB_Demo004GameInstance::SetCurrentWorldSeed(const FString& InSeed)

{

	CurrentWorldSeed = UIHSeedValidationLibrary::NormalizeSeedString(InSeed);

	MasterSeedInt = UIHSeedValidationLibrary::SeedStringToMasterInt32(CurrentWorldSeed);

	const int32 FromSeed = UIHSeedValidationLibrary::ExtractIslandCountFromEightCharSeed(CurrentWorldSeed);

	ProceduralIslandCount = (FromSeed >= 2 && FromSeed <= 7) ? FromSeed : 3;

	RebuildMapSeedPhase1(true);
}

void UIH_WB_Demo004GameInstance::RebuildMapSeedPhase1(bool bLogDebugReport)
{
	UIHMapSeedFrameworkLibrary::BuildPhase1FromSeed(
		CurrentWorldSeed,
		MapSeedPhase1,
		RealmHalfExtentNSKm,
		0.f,
		TargetEffectiveLandFraction);
	if (MapSeedPhase1.bSuccess)
	{
		ProceduralIslandCount = MapSeedPhase1.SeedContext.IslandCount;
		DevLandAreaFraction = MapSeedPhase1.BudgetPlan.AchievedEffectiveLandFraction;
	}
	if (bLogDebugReport)
	{
		UIHMapSeedFrameworkLibrary::LogPhase1DebugReport(MapSeedPhase1);
	}
}

TSubclassOf<AGameModeBase> UIH_WB_Demo004GameInstance::OverrideGameModeClass(

	TSubclassOf<AGameModeBase> GameModeClass,

	const FString& MapName,

	const FString& Options,

	const FString& Portal) const

{

	if (GameModeClass == AGameModeBase::StaticClass())

	{

		return AIH_WB_Demo004GameMode::StaticClass();

	}

	return Super::OverrideGameModeClass(GameModeClass, MapName, Options, Portal);

}



float UIH_WB_Demo004GameInstance::GetRealmHalfExtentEWKm() const

{

	return UIHSeedIslandLibrary::ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);

}



void UIH_WB_Demo004GameInstance::SetRealmHalfExtentNSKm(float InHalfDepthKm)

{

	RealmHalfExtentNSKm = FMath::Clamp(InHalfDepthKm, 2.f, 32.f);

	RebuildMapSeedPhase1(false);

}



void UIH_WB_Demo004GameInstance::SetDevLandAreaFraction(float InFraction)

{

	SetTargetEffectiveLandFraction(InFraction);

}

void UIH_WB_Demo004GameInstance::SetTargetEffectiveLandFraction(float InFraction)
{
	TargetEffectiveLandFraction = FMath::Clamp(InFraction, 0.05f, 0.95f);
	RebuildMapSeedPhase1(false);
}

float UIH_WB_Demo004GameInstance::GetAchievedEffectiveLandFraction() const
{
	if (MapSeedPhase1.bSuccess)
	{
		return MapSeedPhase1.BudgetPlan.AchievedEffectiveLandFraction;
	}
	return TargetEffectiveLandFraction;
}



int32 UIH_WB_Demo004GameInstance::GetTotalLandAcres() const

{

	if (MapSeedPhase1.bSuccess && MapSeedPhase1.BudgetPlan.TotalLandAcres > 0)
	{
		return MapSeedPhase1.BudgetPlan.TotalLandAcres;
	}

	return UIHSeedIslandLibrary::ComputeTotalLandAcres(

		RealmHalfExtentNSKm, GetRealmHalfExtentEWKm(), TargetEffectiveLandFraction);

}

void UIH_WB_Demo004GameInstance::IH_LogTemplateGalleryManifest()
{
	if (UIH_P1C08_TemplateGallerySubsystem* Gallery = GetSubsystem<UIH_P1C08_TemplateGallerySubsystem>())
	{
		Gallery->LogGalleryManifest();
	}
	else
	{
		UIHIslandTemplateProfileLibrary::LogGalleryReviewManifest();
	}
}


