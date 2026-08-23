// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "IHSeedValidationLibrary.generated.h"



class UDataTable;



UCLASS()

class IH_WB_DEMO004_API UIHSeedValidationLibrary : public UBlueprintFunctionLibrary

{

	GENERATED_BODY()



public:

	static const FString InvalidSeedUserMessage;



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Seed")

	static FString NormalizeSeedString(const FString& RawInput);



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Seed")

	static int32 SeedStringToMasterInt32(const FString& EightCharSeed);



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Seed")

	static int32 ExtractIslandCountFromEightCharSeed(const FString& EightCharUpperSeed);



	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Seed")

	static bool BuildWordSetFromDataTable(UDataTable* SeedWordsTable, TSet<FString>& OutUpperWords);



	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Seed")

	static bool LoadSeedWordsFromProjectCsv(const FString& ContentRelativePath, TSet<FString>& OutUpperWords);



	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Seed")

	static bool ValidateSeedAgainstWordSet(const FString& EightCharUpperSeed, const TSet<FString>& AllowedUpperWords,

		FString& OutErrorMessage, int32& OutIslandCount, int32& OutMasterSeedInt);



	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Seed")

	static bool ValidateSeedWithDataTable(const FString& RawInput, UDataTable* SeedWordsTable,

		FString& OutErrorMessage, FString& OutNormalizedSeed, int32& OutIslandCount, int32& OutMasterSeedInt);



	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Seed")

	static bool ValidateSeedWithTableOrCsv(const FString& RawInput, UDataTable* SeedWordsTable,

		const FString& CsvContentRelativePath, FString& OutErrorMessage, FString& OutNormalizedSeed,

		int32& OutIslandCount, int32& OutMasterSeedInt);



	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Seed")

	static bool GenerateRandomValidSeed(UDataTable* SeedWordsTable, UPARAM(ref) FRandomStream& Stream,

		FString& OutSeed, FString& OutErrorMessage);



	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Seed",

		meta = (AdvancedDisplay = "DesiredLastDigitForIslandCount", AutoExpandCategories = "Invisible Hand|Seed"))

	static bool GenerateRandomValidSeedWithTableOrCsv(UDataTable* SeedWordsTable, const FString& CsvContentRelativePath,

		UPARAM(ref) FRandomStream& Stream, FString& OutSeed, FString& OutErrorMessage, int32 DesiredLastDigitForIslandCount = 0);



	static bool ValidateSeedRulesInternal(const FString& EightUpper, const TSet<FString>& AllowedWords,

		FString& OutErrorMessage, int32& OutIslandCount, int32& OutMasterSeedInt);

};

