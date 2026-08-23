// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "IHInvisibleHandDesignSpec.h"
#include "IHMapSeedFrameworkTypes.h"

#include "GameFramework/GameModeBase.h"

#include "IH_WB_Demo004GameInstance.generated.h"



UCLASS()

class IH_WB_DEMO004_API UIH_WB_Demo004GameInstance : public UGameInstance

{

	GENERATED_BODY()



public:

	virtual void Init() override;



	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	FString GetCurrentWorldSeed() const { return CurrentWorldSeed; }



	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	int32 GetProceduralIslandCount() const { return ProceduralIslandCount; }



	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	int32 GetMasterSeedInt() const { return MasterSeedInt; }



	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	int32 GetTankIslandIndex() const { return TankIslandIndex; }



	UFUNCTION(BlueprintCallable, Category = "P1C08|Seed")

	void SetCurrentWorldSeed(const FString& InSeed);



	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	int32 GetRandomRealmIslandCount() const { return RandomRealmIslandCount; }



	UFUNCTION(BlueprintCallable, Category = "P1C08|Seed")

	void SetRandomRealmIslandCount(int32 Count) { RandomRealmIslandCount = FMath::Clamp(Count, 2, 7); }

	/** Step random-realm island count by ±1, clamped to 2–7. */
	void StepRandomRealmIslandCount(int32 Delta) { SetRandomRealmIslandCount(RandomRealmIslandCount + Delta); }



	/** N-S realm half-extent in km (13 → 26 km depth). Drives φ width and land/acre budget. */

	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	float GetRealmHalfExtentNSKm() const { return RealmHalfExtentNSKm; }



	/** E-W half-width in km = φ × halfDepth. */

	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	float GetRealmHalfExtentEWKm() const;



	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	float GetDevLandAreaFraction() const { return DevLandAreaFraction; }

	/** Soft hint for layout solve (default 30% effective dry land). */
	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")
	float GetTargetEffectiveLandFraction() const { return TargetEffectiveLandFraction; }

	UFUNCTION(BlueprintCallable, Category = "P1C08|Seed")
	void SetTargetEffectiveLandFraction(float InFraction);

	/** Achieved effective dry-land fraction from last Phase 1 layout solve. */
	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")
	float GetAchievedEffectiveLandFraction() const;



	UFUNCTION(BlueprintCallable, Category = "P1C08|Seed")

	void SetRealmHalfExtentNSKm(float InHalfDepthKm);



	UFUNCTION(BlueprintCallable, Category = "P1C08|Seed")

	void SetDevLandAreaFraction(float InFraction);

	/** Normalized sun arc: 0 = sunrise, 0.5 = noon, 1 = sunset. */
	UFUNCTION(BlueprintPure, Category = "P1C08|Sun")
	float GetSunTimeOfDay() const { return SunTimeOfDay; }

	UFUNCTION(BlueprintCallable, Category = "P1C08|Sun")
	void SetSunTimeOfDay(float InTime) { SunTimeOfDay = FMath::Clamp(InTime, 0.f, 1.f); }

	// Future: SunLatitudeDeg (hemispheric latitude) will modulate arc height/azimuth in GameMode sun rotation.

	UFUNCTION(BlueprintPure, Category = "P1C08|Seed")

	int32 GetTotalLandAcres() const;

	UFUNCTION(BlueprintPure, Category = "P1C09|Map Seed")
	const FIHMapSeedPhase1Result& GetMapSeedPhase1() const { return MapSeedPhase1; }

	UFUNCTION(BlueprintCallable, Category = "P1C09|Map Seed")
	void RebuildMapSeedPhase1(bool bLogDebugReport = true);

	/** Console: IH_LogTemplateGalleryManifest */
	UFUNCTION(Exec, Category = "P1C09|Template Gallery")
	void IH_LogTemplateGalleryManifest();

protected:

	virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(

		TSubclassOf<AGameModeBase> GameModeClass,

		const FString& MapName,

		const FString& Options,

		const FString& Portal) const override;



	UPROPERTY(EditAnywhere, Category = "P1C08|Seed")

	FString CurrentWorldSeed = TEXT("ABBEY3");



	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P1C08|Seed")

	int32 ProceduralIslandCount = 3;



	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P1C08|Seed")

	int32 MasterSeedInt = 0;



	/** Island slot index (Island01 = 0) — not the layout/aquarium AABB. */

	UPROPERTY(EditAnywhere, Category = "P1C08|Seed")

	int32 TankIslandIndex = 0;



	/** 2–7; only affects Random Realm generation in the dev seed panel. */

	UPROPERTY(EditAnywhere, Category = "P1C08|Seed")

	int32 RandomRealmIslandCount = 4;



	/** Realm half-extent N-S in km. Width auto = φ × N-S half-extent. */

	UPROPERTY(EditAnywhere, Category = "P1C08|Seed")

	float RealmHalfExtentNSKm = IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm;



	/** Legacy budget fraction (mirrors achieved effective after solve). */
	UPROPERTY(EditAnywhere, Category = "P1C08|Seed", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float DevLandAreaFraction = IHInvisibleHandSpec::DefaultDevLandAreaFraction;

	/** Soft target for achieved dry land / tank water surface (layout solve hint). */
	UPROPERTY(EditAnywhere, Category = "P1C08|Seed", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float TargetEffectiveLandFraction = IHInvisibleHandSpec::DefaultTargetEffectiveLandFraction;

	UPROPERTY(EditAnywhere, Category = "P1C08|Sun", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SunTimeOfDay = 0.22f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P1C09|Map Seed")
	FIHMapSeedPhase1Result MapSeedPhase1;

};


