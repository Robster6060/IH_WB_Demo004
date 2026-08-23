// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHMapSeedFrameworkLibrary.h"

#include "IHInvisibleHandDesignSpec.h"
#include "IHSeedIslandLibrary.h"
#include "IHSeedValidationLibrary.h"

namespace IHMapSeedFrameworkPrivate
{
	static constexpr uint32 StreamId_Global = 0x1A2B3C4Du;
	static constexpr uint32 StreamId_TemplateAssign = 0x7E8F90A1u;
	static constexpr uint32 StreamId_SpawnOrder = 0xB4C5D6E7u;
	static constexpr uint32 StreamId_DiversityGuard = 0xD1B35E7u;
	static constexpr uint32 StreamId_PlacementBase = 0x50ACE001u;
	static constexpr uint32 StreamId_OrientationBase = 0x0B1E0101u;
	static constexpr uint32 StreamId_DetailBase = 0xDE741100u;
	static constexpr uint32 StreamId_CenterBase = 0xCE473700u;

	static void AddDebugRow(
		TArray<FIHMapSeedPhase1DebugRow>& Report,
		const FString& Category,
		const FString& Key,
		const FString& Value)
	{
		FIHMapSeedPhase1DebugRow Row;
		Row.Category = Category;
		Row.Key = Key;
		Row.Value = Value;
		Report.Add(Row);
	}

	static void BuildUrnFromWeights(const FIHIslandTemplateWeights& Weights, TArray<EIHIslandTemplateType>& OutUrn)
	{
		OutUrn.Reset();
		for (int32 i = 0; i < Weights.LowWeight; ++i)
		{
			OutUrn.Add(EIHIslandTemplateType::Low);
		}
		for (int32 i = 0; i < Weights.HighWeight; ++i)
		{
			OutUrn.Add(EIHIslandTemplateType::High);
		}
		for (int32 i = 0; i < Weights.VolcanicWeight; ++i)
		{
			OutUrn.Add(EIHIslandTemplateType::Volcanic);
		}
	}

	static int32 CountDistinctTemplates(const TArray<EIHIslandTemplateType>& Types)
	{
		bool bLow = false;
		bool bHigh = false;
		bool bVolcanic = false;
		for (EIHIslandTemplateType T : Types)
		{
			switch (T)
			{
			case EIHIslandTemplateType::Low: bLow = true; break;
			case EIHIslandTemplateType::High: bHigh = true; break;
			case EIHIslandTemplateType::Volcanic: bVolcanic = true; break;
			default: break;
			}
		}
		return (bLow ? 1 : 0) + (bHigh ? 1 : 0) + (bVolcanic ? 1 : 0);
	}
}

FIHIslandTemplateWeights UIHMapSeedFrameworkLibrary::GetDefaultTemplateWeights()
{
	return FIHIslandTemplateWeights();
}

FString UIHMapSeedFrameworkLibrary::IslandTemplateTypeToString(EIHIslandTemplateType Type)
{
	switch (Type)
	{
	case EIHIslandTemplateType::Low: return TEXT("Low");
	case EIHIslandTemplateType::High: return TEXT("High");
	case EIHIslandTemplateType::Volcanic: return TEXT("Volcanic");
	default: return TEXT("Unknown");
	}
}

FString UIHMapSeedFrameworkLibrary::IslandTemplateTypeToNavAbbrev(EIHIslandTemplateType Type)
{
	switch (Type)
	{
	case EIHIslandTemplateType::Low: return TEXT("Low");
	case EIHIslandTemplateType::High: return TEXT("High");
	case EIHIslandTemplateType::Volcanic: return TEXT("Volc");
	default: return TEXT("?");
	}
}

int32 UIHMapSeedFrameworkLibrary::DeriveStreamSeed(int32 MasterSeedInt32, uint32 StreamId, int32 Salt)
{
	const uint32 Mixed = static_cast<uint32>(MasterSeedInt32)
		^ (StreamId * 0x9E3779B9u)
		^ static_cast<uint32>(Salt);
	return static_cast<int32>(Mixed != 0 ? Mixed : 1);
}

void UIHMapSeedFrameworkLibrary::AssignTemplatesDeterministic(
	int32 IslandCount,
	int32 MasterSeedInt32,
	const FIHIslandTemplateWeights& Weights,
	TArray<EIHIslandTemplateType>& OutTemplates)
{
	OutTemplates.Reset();
	if (IslandCount <= 0 || !Weights.IsValidWeights())
	{
		return;
	}

	TArray<EIHIslandTemplateType> BaseUrn;
	IHMapSeedFrameworkPrivate::BuildUrnFromWeights(Weights, BaseUrn);

	TArray<EIHIslandTemplateType> WorkingUrn = BaseUrn;
	FRandomStream Stream(DeriveStreamSeed(MasterSeedInt32, IHMapSeedFrameworkPrivate::StreamId_TemplateAssign, IslandCount));

	OutTemplates.Reserve(IslandCount);
	for (int32 i = 0; i < IslandCount; ++i)
	{
		if (WorkingUrn.Num() == 0)
		{
			WorkingUrn = BaseUrn;
		}
		const int32 Pick = Stream.RandRange(0, WorkingUrn.Num() - 1);
		OutTemplates.Add(WorkingUrn[Pick]);
		WorkingUrn.RemoveAtSwap(Pick);
	}
}

void UIHMapSeedFrameworkLibrary::ApplySmallSampleDiversityGuard(
	TArray<EIHIslandTemplateType>& InOutTemplates,
	int32 MasterSeedInt32,
	const FIHIslandTemplateWeights& Weights)
{
	const int32 N = InOutTemplates.Num();
	if (N < 2 || N > SmallSampleDiversityMaxIslandCount)
	{
		return;
	}
	if (IHMapSeedFrameworkPrivate::CountDistinctTemplates(InOutTemplates) > 1)
	{
		return;
	}

	const EIHIslandTemplateType SameType = InOutTemplates[0];
	TArray<EIHIslandTemplateType> Alternates;
	IHMapSeedFrameworkPrivate::BuildUrnFromWeights(Weights, Alternates);
	Alternates.RemoveAll([SameType](EIHIslandTemplateType T) { return T == SameType; });
	if (Alternates.Num() == 0)
	{
		return;
	}

	FRandomStream Stream(DeriveStreamSeed(MasterSeedInt32, IHMapSeedFrameworkPrivate::StreamId_DiversityGuard, N));
	const int32 ReplaceIdx = (N > 1) ? Stream.RandRange(1, N - 1) : 0;
	InOutTemplates[ReplaceIdx] = Alternates[Stream.RandRange(0, Alternates.Num() - 1)];
}

void UIHMapSeedFrameworkLibrary::BuildSpawnOrderDeterministic(
	int32 IslandCount,
	int32 MasterSeedInt32,
	TArray<int32>& OutSpawnOrderByIslandIndex)
{
	OutSpawnOrderByIslandIndex.Reset();
	if (IslandCount <= 0)
	{
		return;
	}

	TArray<TPair<int32, float>> Keys;
	Keys.Reserve(IslandCount);
	FRandomStream Stream(DeriveStreamSeed(MasterSeedInt32, IHMapSeedFrameworkPrivate::StreamId_SpawnOrder, IslandCount));
	for (int32 IslandIndex = 0; IslandIndex < IslandCount; ++IslandIndex)
	{
		Keys.Emplace(IslandIndex, Stream.FRand());
	}
	Keys.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) { return A.Value < B.Value; });

	OutSpawnOrderByIslandIndex.SetNum(IslandCount);
	for (int32 Order = 0; Order < Keys.Num(); ++Order)
	{
		OutSpawnOrderByIslandIndex[Keys[Order].Key] = Order;
	}
}

float UIHMapSeedFrameworkLibrary::GetTemplateLayoutFootprintFactor(EIHIslandTemplateType TemplateType)
{
	switch (TemplateType)
	{
	case EIHIslandTemplateType::Low:
		return 1.06f;
	case EIHIslandTemplateType::Volcanic:
		return 0.96f;
	case EIHIslandTemplateType::High:
	default:
		return 1.f;
	}
}

float UIHMapSeedFrameworkLibrary::ComputeLayoutCoastExtentKm(
	float AreaBudgetKm2, EIHIslandTemplateType TemplateType)
{
	const float BaseExtentKm = UIHSeedIslandLibrary::CoastLayoutExtentKmFromAreaKm2(
		FMath::Max(AreaBudgetKm2, 0.05f));
	return FMath::Max(
		BaseExtentKm * GetTemplateLayoutFootprintFactor(TemplateType)
			* IHInvisibleHandSpec::SquareGridCoastEnvelopeFactor,
		0.05f);
}

void UIHMapSeedFrameworkLibrary::AssignTemplatesToFibonacciRanks(
	int32 IslandCount,
	TArray<EIHIslandTemplateType>& InOutTemplatesByIslandIndex)
{
	if (IslandCount <= 0 || InOutTemplatesByIslandIndex.Num() != IslandCount)
	{
		return;
	}

	struct FTemplateRankEntry
	{
		EIHIslandTemplateType Type = EIHIslandTemplateType::Low;
		float Footprint = 1.f;
		int32 DrawOrder = 0;
	};

	TArray<FTemplateRankEntry> Drawn;
	Drawn.Reserve(IslandCount);
	for (int32 i = 0; i < IslandCount; ++i)
	{
		const EIHIslandTemplateType Type = InOutTemplatesByIslandIndex[i];
		FTemplateRankEntry Entry;
		Entry.Type = Type;
		Entry.Footprint = GetTemplateLayoutFootprintFactor(Type);
		Entry.DrawOrder = i;
		Drawn.Add(Entry);
	}

	Drawn.Sort([](const FTemplateRankEntry& A, const FTemplateRankEntry& B)
	{
		if (!FMath::IsNearlyEqual(A.Footprint, B.Footprint))
		{
			return A.Footprint < B.Footprint;
		}
		return A.DrawOrder < B.DrawOrder;
	});

	for (int32 Rank = 0; Rank < IslandCount; ++Rank)
	{
		InOutTemplatesByIslandIndex[Rank] = Drawn[Rank].Type;
	}
}

void UIHMapSeedFrameworkLibrary::GatherLayoutCoastExtentsKmFromPhase1(
	const FIHMapSeedPhase1Result& Phase1,
	TArray<float>& OutLayoutCoastExtentKm)
{
	OutLayoutCoastExtentKm.Reset();
	if (!Phase1.bSuccess || Phase1.SeedContext.IslandCount < 1)
	{
		return;
	}

	OutLayoutCoastExtentKm.SetNumZeroed(Phase1.SeedContext.IslandCount);
	for (const FIHIslandSpawnPlan& Plan : Phase1.SpawnPlans)
	{
		if (Plan.IslandIndex >= 0 && Plan.IslandIndex < OutLayoutCoastExtentKm.Num())
		{
			OutLayoutCoastExtentKm[Plan.IslandIndex] = Plan.LayoutCoastExtentKm;
		}
	}
}

bool UIHMapSeedFrameworkLibrary::BuildPhase1FromSeed(
	const FString& RawSeedWord,
	FIHMapSeedPhase1Result& OutResult,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm,
	float TargetEffectiveLandFraction)
{
	OutResult = FIHMapSeedPhase1Result();
	OutResult.DebugReport.Reset();

	const FIHIslandTemplateWeights TemplateWeights = GetDefaultTemplateWeights();

	auto Fail = [&](const FString& Message) -> bool
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = Message;
		IHMapSeedFrameworkPrivate::AddDebugRow(OutResult.DebugReport, TEXT("Error"), TEXT("Message"), Message);
		return false;
	};

	const FString Normalized = UIHSeedValidationLibrary::NormalizeSeedString(RawSeedWord);
	if (Normalized.Len() != 6)
	{
		return Fail(TEXT("Seed must normalize to exactly 6 characters (WORD + island count digit 2-7)."));
	}

	const int32 MasterSeed = UIHSeedValidationLibrary::SeedStringToMasterInt32(Normalized);
	const int32 IslandCount = UIHSeedValidationLibrary::ExtractIslandCountFromEightCharSeed(Normalized);
	if (!UIHSeedIslandLibrary::IsLandformCountInDesignRange(IslandCount))
	{
		return Fail(TEXT("Island count must be 2–7 (last seed digit)."));
	}

	FIHMapSeedContext& Ctx = OutResult.SeedContext;
	Ctx.NormalizedSeedWord = Normalized;
	Ctx.MasterSeedInt32 = MasterSeed;
	Ctx.GeneratorVersion = IHMapSeedFrameworkGeneratorVersion;
	Ctx.IslandCount = IslandCount;

	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Seed"), TEXT("NormalizedWord"), Normalized);
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Seed"), TEXT("MasterSeedInt32"), FString::FromInt(MasterSeed));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Seed"), TEXT("GeneratorVersion"), FString::FromInt(Ctx.GeneratorVersion));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Seed"), TEXT("IslandCount"), FString::FromInt(IslandCount));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Seed"), TEXT("IslandCountSource"), TEXT("LastDigitConvention"));

	const float ResolvedHalfWidthKm = (RealmHalfExtentEWKm > 0.f)
		? RealmHalfExtentEWKm
		: UIHSeedIslandLibrary::ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);
	const float ClampedTargetEffective = FMath::Clamp(
		TargetEffectiveLandFraction, 0.05f, 0.95f);
	const float RealmAreaKm2 = UIHSeedIslandLibrary::ComputeRealmAreaKm2(RealmHalfExtentNSKm, ResolvedHalfWidthKm);

	// Shape: weighted urn draw, then match templates to Fibonacci ranks (compact → largest slice).
	TArray<EIHIslandTemplateType> Templates;
	AssignTemplatesDeterministic(IslandCount, MasterSeed, TemplateWeights, Templates);
	const int32 DistinctBefore = IHMapSeedFrameworkPrivate::CountDistinctTemplates(Templates);
	ApplySmallSampleDiversityGuard(Templates, MasterSeed, TemplateWeights);
	AssignTemplatesToFibonacciRanks(IslandCount, Templates);
	const int32 DistinctAfter = IHMapSeedFrameworkPrivate::CountDistinctTemplates(Templates);

	TArray<float> ShapeAreasKm2;
	UIHSeedIslandLibrary::ComputeIslandAreasKM2(
		IslandCount, RealmAreaKm2 * ClampedTargetEffective, ShapeAreasKm2);

	TArray<float> BaseLayoutExtentsKm;
	BaseLayoutExtentsKm.SetNum(IslandCount);
	for (int32 i = 0; i < IslandCount; ++i)
	{
		const float ShapeAreaKm2 = ShapeAreasKm2.IsValidIndex(i) ? ShapeAreasKm2[i] : 0.05f;
		const EIHIslandTemplateType TemplateType = Templates.IsValidIndex(i) ? Templates[i] : EIHIslandTemplateType::Low;
		BaseLayoutExtentsKm[i] = ComputeLayoutCoastExtentKm(ShapeAreaKm2, TemplateType);
	}

	FIHIslandLayoutSolveResult& LayoutSolve = OutResult.LayoutSolve;
	if (!UIHSeedIslandLibrary::SolveIslandLayoutForSeed(
		IslandCount, MasterSeed, RealmHalfExtentNSKm, ResolvedHalfWidthKm, ClampedTargetEffective,
		ShapeAreasKm2, BaseLayoutExtentsKm, LayoutSolve))
	{
		LayoutSolve.TargetEffectiveLandFraction = ClampedTargetEffective;
		LayoutSolve.BaseEffectiveLandFraction = UIHSeedIslandLibrary::ComputeEffectiveLandFractionFromExtentsKm(
			BaseLayoutExtentsKm, RealmHalfExtentNSKm, ResolvedHalfWidthKm);
		LayoutSolve.UniformAreaScale = 1.f;
		LayoutSolve.AchievedEffectiveLandFraction = ClampedTargetEffective;
		LayoutSolve.MaxEffectiveLandFraction = LayoutSolve.AchievedEffectiveLandFraction;
		LayoutSolve.IslandAreasKm2 = ShapeAreasKm2;
		LayoutSolve.LayoutExtentKm = BaseLayoutExtentsKm;
		LayoutSolve.bSuccess = ShapeAreasKm2.Num() == IslandCount;
	}

	TArray<float> AreasKm2 = LayoutSolve.bSuccess ? LayoutSolve.IslandAreasKm2 : ShapeAreasKm2;

	FIHIslandBudgetPlan& Budget = OutResult.BudgetPlan;
	Budget.RealmHalfExtentNSKm = RealmHalfExtentNSKm;
	Budget.RealmHalfExtentEWKm = ResolvedHalfWidthKm;
	Budget.TargetEffectiveLandFraction = ClampedTargetEffective;
	Budget.AchievedEffectiveLandFraction = LayoutSolve.bSuccess
		? LayoutSolve.AchievedEffectiveLandFraction
		: ClampedTargetEffective;
	Budget.DevLandAreaFraction = Budget.AchievedEffectiveLandFraction;
	Budget.TotalLandAcres = UIHSeedIslandLibrary::ComputeTotalLandAcres(
		RealmHalfExtentNSKm, ResolvedHalfWidthKm, Budget.AchievedEffectiveLandFraction);
	UIHSeedIslandLibrary::ComputeAcresBudgets(IslandCount, Budget.TotalLandAcres, Budget.PerIslandAcres);
	Budget.PerIslandTargetSectors.SetNum(IslandCount);
	for (int32 i = 0; i < IslandCount; ++i)
	{
		const int32 Acres = Budget.PerIslandAcres.IsValidIndex(i) ? Budget.PerIslandAcres[i] : 0;
		Budget.PerIslandTargetSectors[i] = Acres;
		// Production Fibonacci millions stay informational until bake+WP unlock.
		if (IHInvisibleHandSpec::bWBUnlockProductionCanonicalAcres)
		{
			const uint32 CanonicalSectors = IHInvisibleHandSpec::GetCanonicalIslandDrySectors(IslandCount, i);
			if (CanonicalSectors > 0u)
			{
				Budget.PerIslandTargetSectors[i] = static_cast<int32>(CanonicalSectors);
			}
		}
	}

	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("TotalLandAcres"), FString::FromInt(Budget.TotalLandAcres));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("RealmHalfExtentNSKm"), FString::SanitizeFloat(RealmHalfExtentNSKm));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("RealmHalfExtentEWKm"), FString::SanitizeFloat(ResolvedHalfWidthKm));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("TargetEffectiveLandFraction"),
		FString::Printf(TEXT("%.1f%%"), ClampedTargetEffective * 100.f));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("AchievedEffectiveLandFraction"),
		FString::Printf(TEXT("%.1f%%"), Budget.AchievedEffectiveLandFraction * 100.f));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("MaxEffectiveLandFraction"),
		FString::Printf(TEXT("%.1f%%"), LayoutSolve.MaxEffectiveLandFraction * 100.f));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("UniformAreaScale"),
		FString::SanitizeFloat(LayoutSolve.UniformAreaScale));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("LandSplitSource"), TEXT("FibonacciFromEffectiveSolve"));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Budget"), TEXT("LayoutPlacement"),
		LayoutSolve.bUsedCompactPlacement ? TEXT("compact") : TEXT("maxSpreadFallback"));

	TArray<float> FibWeights;
	UIHSeedIslandLibrary::GetFibonacciAreaWeights(IslandCount, FibWeights);
	for (int32 i = 0; i < FibWeights.Num(); ++i)
	{
		IHMapSeedFrameworkPrivate::AddDebugRow(
			OutResult.DebugReport, TEXT("Fibonacci"),
			FString::Printf(TEXT("Island_%d_Pct"), i),
			FString::Printf(TEXT("%.1f%%"), FibWeights[i] * 100.f));
	}

	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Template"), TEXT("Weights"),
		FString::Printf(TEXT("Low:%d High:%d Volcanic:%d"), TemplateWeights.LowWeight, TemplateWeights.HighWeight,
			TemplateWeights.VolcanicWeight));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Template"), TEXT("DistinctBeforeGuard"), FString::FromInt(DistinctBefore));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Template"), TEXT("DistinctAfterGuard"), FString::FromInt(DistinctAfter));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Template"), TEXT("FibonacciRankMatch"), TEXT("true"));
	IHMapSeedFrameworkPrivate::AddDebugRow(
		OutResult.DebugReport, TEXT("Template"), TEXT("SmallSampleGuardApplied"),
		(DistinctBefore < 2 && DistinctAfter >= 2) ? TEXT("true") : TEXT("false"));

	TArray<int32> SpawnOrderByIsland;
	BuildSpawnOrderDeterministic(IslandCount, MasterSeed, SpawnOrderByIsland);

	OutResult.SpawnPlans.SetNum(IslandCount);
	for (int32 IslandIndex = 0; IslandIndex < IslandCount; ++IslandIndex)
	{
		FIHIslandSpawnPlan& Plan = OutResult.SpawnPlans[IslandIndex];
		Plan.IslandIndex = IslandIndex;
		Plan.SpawnOrder = SpawnOrderByIsland.IsValidIndex(IslandIndex) ? SpawnOrderByIsland[IslandIndex] : IslandIndex;
		Plan.TemplateType = Templates.IsValidIndex(IslandIndex) ? Templates[IslandIndex] : EIHIslandTemplateType::Low;
		Plan.FibonacciRank = IslandIndex;
		Plan.TemplateFootprintFactor = GetTemplateLayoutFootprintFactor(Plan.TemplateType);
		Plan.AreaBudgetAcres = Budget.PerIslandAcres.IsValidIndex(IslandIndex) ? Budget.PerIslandAcres[IslandIndex] : 0;
		Plan.AreaBudgetKm2 = AreasKm2.IsValidIndex(IslandIndex) ? AreasKm2[IslandIndex] : 0.f;
		Plan.LayoutCoastExtentKm = LayoutSolve.LayoutExtentKm.IsValidIndex(IslandIndex)
			? LayoutSolve.LayoutExtentKm[IslandIndex]
			: ComputeLayoutCoastExtentKm(Plan.AreaBudgetKm2, Plan.TemplateType);
		Plan.PlacementStreamSeed = DeriveStreamSeed(
			MasterSeed, IHMapSeedFrameworkPrivate::StreamId_PlacementBase, IslandIndex);
		Plan.OrientationStreamSeed = DeriveStreamSeed(
			MasterSeed, IHMapSeedFrameworkPrivate::StreamId_OrientationBase, IslandIndex);
		Plan.DetailStreamSeed = DeriveStreamSeed(
			MasterSeed, IHMapSeedFrameworkPrivate::StreamId_DetailBase, IslandIndex);
		Plan.CenterStreamSeed = DeriveStreamSeed(
			MasterSeed, IHMapSeedFrameworkPrivate::StreamId_CenterBase, IslandIndex);

		IHMapSeedFrameworkPrivate::AddDebugRow(
			OutResult.DebugReport, TEXT("SpawnPlan"),
			FString::Printf(TEXT("Island_%d"), IslandIndex),
			FString::Printf(
				TEXT("template=%s rank=%d footprint=%.2f acres=%d km2=%s layoutExt=%s place=%d orient=%d detail=%d center=%d"),
				*IslandTemplateTypeToString(Plan.TemplateType),
				Plan.FibonacciRank,
				Plan.TemplateFootprintFactor,
				Plan.AreaBudgetAcres,
				*FString::SanitizeFloat(Plan.AreaBudgetKm2),
				*FString::SanitizeFloat(Plan.LayoutCoastExtentKm),
				Plan.PlacementStreamSeed,
				Plan.OrientationStreamSeed,
				Plan.DetailStreamSeed,
				Plan.CenterStreamSeed));
	}

	OutResult.bSuccess = true;
	return true;
}

FString UIHMapSeedFrameworkLibrary::FormatPhase1DebugReport(const FIHMapSeedPhase1Result& Result)
{
	FString Lines;
	for (const FIHMapSeedPhase1DebugRow& Row : Result.DebugReport)
	{
		Lines += FString::Printf(TEXT("[%s] %s = %s\n"), *Row.Category, *Row.Key, *Row.Value);
	}
	if (!Result.bSuccess && !Result.ErrorMessage.IsEmpty())
	{
		Lines += FString::Printf(TEXT("[Error] %s\n"), *Result.ErrorMessage);
	}
	return Lines;
}

void UIHMapSeedFrameworkLibrary::LogPhase1DebugReport(const FIHMapSeedPhase1Result& Result)
{
	if (!Result.bSuccess)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("MapSeed Phase1 FAILED: %s"), *Result.ErrorMessage);
	}
	UE_LOG(LogIH_WB_Demo004, Log, TEXT("MapSeed Phase1 (%s) — %d islands, generator v%d"),
		*Result.SeedContext.NormalizedSeedWord,
		Result.SeedContext.IslandCount,
		Result.SeedContext.GeneratorVersion);
	for (const FIHMapSeedPhase1DebugRow& Row : Result.DebugReport)
	{
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("  Phase1 [%s] %s = %s"), *Row.Category, *Row.Key, *Row.Value);
	}
}
