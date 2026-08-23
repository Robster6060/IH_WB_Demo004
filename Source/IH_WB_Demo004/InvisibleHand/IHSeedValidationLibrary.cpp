// Copyright Invisible Hand. All Rights Reserved.

#include "IHSeedValidationLibrary.h"
#include "IH_PRNG_SeedWordRow.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

const FString UIHSeedValidationLibrary::InvalidSeedUserMessage =
	TEXT("Sorry, that seed is invalid. Enter WORD + 1 digit (2-7) for island count (example: ABBEY2).");

FString UIHSeedValidationLibrary::NormalizeSeedString(const FString& RawInput)
{
	FString S = RawInput.TrimStartAndEnd().ToUpper();
	FString Result;
	for (TCHAR C : S)
	{
		if (C != TEXT(' '))
		{
			Result.AppendChar(C);
		}
	}
	return Result;
}

int32 UIHSeedValidationLibrary::SeedStringToMasterInt32(const FString& EightCharSeed)
{
	int32 Hash = 5381;
	for (TCHAR C : EightCharSeed)
	{
		Hash = ((Hash << 5) + Hash) + static_cast<int32>(C);
	}
	return Hash;
}

int32 UIHSeedValidationLibrary::ExtractIslandCountFromEightCharSeed(const FString& EightCharUpperSeed)
{
	if (EightCharUpperSeed.Len() != 6)
	{
		return 0;
	}
	const TCHAR Last = EightCharUpperSeed[5];
	if (Last < TEXT('0') || Last > TEXT('9'))
	{
		return 0;
	}
	const int32 Island = static_cast<int32>(Last - TEXT('0'));
	if (Island < 2 || Island > 7)
	{
		return 0;
	}
	return Island;
}

bool UIHSeedValidationLibrary::BuildWordSetFromDataTable(UDataTable* SeedWordsTable, TSet<FString>& OutUpperWords)
{
	OutUpperWords.Reset();
	if (!SeedWordsTable)
	{
		return false;
	}
	const TArray<FName> RowNames = SeedWordsTable->GetRowNames();
	for (FName RowName : RowNames)
	{
		const FIHPRNGSeedWordRow* Row = SeedWordsTable->FindRow<FIHPRNGSeedWordRow>(RowName, TEXT("IH BuildWordSet"));
		if (Row && Row->Word.Len() == 5)
		{
			OutUpperWords.Add(Row->Word.ToUpper());
		}
	}
	return OutUpperWords.Num() > 0;
}

bool UIHSeedValidationLibrary::LoadSeedWordsFromProjectCsv(const FString& ContentRelativePath, TSet<FString>& OutUpperWords)
{
	OutUpperWords.Reset();
	const FString FullPath = FPaths::ProjectContentDir() / ContentRelativePath;
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FullPath))
	{
		return false;
	}
	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines, false);
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		if (i == 0)
		{
			continue;
		}
		const FString& Line = Lines[i];
		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() >= 2)
		{
			FString Word = Parts[1].TrimStartAndEnd().ToUpper();
			Word.ReplaceInline(TEXT("\""), TEXT(""));
			if (Word.Len() == 5)
			{
				OutUpperWords.Add(Word);
			}
		}
	}
	return OutUpperWords.Num() > 0;
}

bool UIHSeedValidationLibrary::ValidateSeedRulesInternal(const FString& EightUpper, const TSet<FString>& AllowedWords,
	FString& OutErrorMessage, int32& OutIslandCount, int32& OutMasterSeedInt)
{
	OutErrorMessage.Empty();
	OutIslandCount = 0;
	OutMasterSeedInt = 0;

	if (EightUpper.Len() != 6)
	{
		OutErrorMessage = TEXT("Seed must be exactly 6 characters: 5 letters + 1 digit (example: ABBEY2).");
		return false;
	}

	const FString Prefix = EightUpper.Left(5);
	for (int32 i = 0; i < Prefix.Len(); ++i)
	{
		const TCHAR C = Prefix[i];
		if (C < TEXT('A') || C > TEXT('Z'))
		{
			OutErrorMessage = TEXT("The first 5 characters must be letters A-Z.");
			return false;
		}
	}

	if (!AllowedWords.Contains(Prefix))
	{
		OutErrorMessage = TEXT("The first 5 letters must be a valid word from the 2400 Seed Words List.");
		return false;
	}

	const TCHAR Digit = EightUpper[5];
	if (Digit < TEXT('0') || Digit > TEXT('9'))
	{
		OutErrorMessage = TEXT("The last character must be a digit (2-7).");
		return false;
	}

	const int32 Island = ExtractIslandCountFromEightCharSeed(EightUpper);
	if (Island == 0)
	{
		OutErrorMessage = TEXT("Invalid island-count digit: the 6th character must be 2-7.");
		return false;
	}

	OutIslandCount = Island;
	OutMasterSeedInt = SeedStringToMasterInt32(EightUpper);
	return true;
}

bool UIHSeedValidationLibrary::ValidateSeedAgainstWordSet(const FString& EightCharUpperSeed, const TSet<FString>& AllowedUpperWords,
	FString& OutErrorMessage, int32& OutIslandCount, int32& OutMasterSeedInt)
{
	return ValidateSeedRulesInternal(EightCharUpperSeed, AllowedUpperWords, OutErrorMessage, OutIslandCount, OutMasterSeedInt);
}

bool UIHSeedValidationLibrary::ValidateSeedWithDataTable(const FString& RawInput, UDataTable* SeedWordsTable,
	FString& OutErrorMessage, FString& OutNormalizedSeed, int32& OutIslandCount, int32& OutMasterSeedInt)
{
	const FString Norm = NormalizeSeedString(RawInput);
	OutNormalizedSeed = Norm;
	TSet<FString> Words;
	if (!BuildWordSetFromDataTable(SeedWordsTable, Words))
	{
		OutMasterSeedInt = 0;
		OutIslandCount = 0;
		OutErrorMessage = TEXT("Seed word list not configured (assign Data Table or add CSV).");
		return false;
	}
	return ValidateSeedRulesInternal(Norm, Words, OutErrorMessage, OutIslandCount, OutMasterSeedInt);
}

bool UIHSeedValidationLibrary::ValidateSeedWithTableOrCsv(const FString& RawInput, UDataTable* SeedWordsTable,
	const FString& CsvContentRelativePath, FString& OutErrorMessage, FString& OutNormalizedSeed,
	int32& OutIslandCount, int32& OutMasterSeedInt)
{
	const FString Norm = NormalizeSeedString(RawInput);
	OutNormalizedSeed = Norm;
	TSet<FString> Words;
	if (!BuildWordSetFromDataTable(SeedWordsTable, Words) || Words.Num() == 0)
	{
		LoadSeedWordsFromProjectCsv(CsvContentRelativePath, Words);
	}
	if (Words.Num() == 0)
	{
		OutMasterSeedInt = 0;
		OutIslandCount = 0;
		OutErrorMessage = TEXT("2400 Seed Words List not found. Assign Data Table or add CSV under Content.");
		return false;
	}
	return ValidateSeedRulesInternal(Norm, Words, OutErrorMessage, OutIslandCount, OutMasterSeedInt);
}

bool UIHSeedValidationLibrary::GenerateRandomValidSeed(UDataTable* SeedWordsTable, FRandomStream& Stream, FString& OutSeed, FString& OutErrorMessage)
{
	OutSeed.Empty();
	OutErrorMessage.Empty();
	if (!SeedWordsTable)
	{
		OutErrorMessage = TEXT("Seed words Data Table is required for random generation.");
		return false;
	}
	TArray<FName> RowNames = SeedWordsTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		OutErrorMessage = TEXT("Seed words Data Table has no rows.");
		return false;
	}
	const int32 Pick = Stream.RandRange(0, RowNames.Num() - 1);
	const FIHPRNGSeedWordRow* Row = SeedWordsTable->FindRow<FIHPRNGSeedWordRow>(RowNames[Pick], TEXT("IH RandomSeed"));
	if (!Row || Row->Word.Len() != 5)
	{
		OutErrorMessage = TEXT("Invalid seed word row.");
		return false;
	}
	const FString Word = Row->Word.ToUpper();
	const int32 LastD = Stream.RandRange(2, 7);
	OutSeed = Word + FString::Printf(TEXT("%d"), LastD);
	return OutSeed.Len() == 6;
}

bool UIHSeedValidationLibrary::GenerateRandomValidSeedWithTableOrCsv(UDataTable* SeedWordsTable, const FString& CsvContentRelativePath,
	FRandomStream& Stream, FString& OutSeed, FString& OutErrorMessage, int32 DesiredLastDigitForIslandCount)
{
	OutSeed.Empty();
	OutErrorMessage.Empty();
	TSet<FString> Words;
	if (!BuildWordSetFromDataTable(SeedWordsTable, Words) || Words.Num() == 0)
	{
		LoadSeedWordsFromProjectCsv(CsvContentRelativePath, Words);
	}
	if (Words.Num() == 0)
	{
		OutErrorMessage = TEXT("2400 Seed Words List not found. Assign Data Table or add CSV under Content.");
		return false;
	}
	TArray<FString> WordArray = Words.Array();
	const int32 Pick = Stream.RandRange(0, WordArray.Num() - 1);
	const FString Word = WordArray[Pick];
	if (Word.Len() != 5)
	{
		OutErrorMessage = TEXT("Invalid vocabulary word length.");
		return false;
	}
	const int32 LastD = (DesiredLastDigitForIslandCount >= 2 && DesiredLastDigitForIslandCount <= 7)
		? DesiredLastDigitForIslandCount
		: Stream.RandRange(2, 7);
	OutSeed = Word + FString::Printf(TEXT("%d"), LastD);
	return OutSeed.Len() == 6;
}
