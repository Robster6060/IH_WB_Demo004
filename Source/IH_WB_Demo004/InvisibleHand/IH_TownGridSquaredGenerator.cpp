// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_TownGridSquaredGenerator.h"

#include "IHInvisibleHandDesignSpec.h"

namespace IH_TownGridSquaredGeneratorPrivate
{
	static FVector LocalToWorldXY(const FVector& Center, float YawDeg, const FVector2D& LocalXY)
	{
		const float Rad = FMath::DegreesToRadians(YawDeg);
		const float CosA = FMath::Cos(Rad);
		const float SinA = FMath::Sin(Rad);
		const float RotX = LocalXY.X * CosA - LocalXY.Y * SinA;
		const float RotY = LocalXY.X * SinA + LocalXY.Y * CosA;
		return FVector(Center.X + RotX, Center.Y + RotY, Center.Z);
	}

	static void AddSegment(
		const FVector& Center,
		float YawDeg,
		const FVector2D& LocalStart,
		const FVector2D& LocalEnd,
		IHInvisibleHandSpec::ETownGridRoadOverlayClass RoadClass,
		bool bCommonsParcelLine,
		FTownGridOverlayData& OutOverlay)
	{
		FTownGridOverlaySegment Segment;
		Segment.StartWorld = LocalToWorldXY(Center, YawDeg, LocalStart);
		Segment.EndWorld = LocalToWorldXY(Center, YawDeg, LocalEnd);
		Segment.RoadClass = RoadClass;
		Segment.bCommonsParcelLine = bCommonsParcelLine;
		OutOverlay.Segments.Add(Segment);
	}

	static IHInvisibleHandSpec::ETownGridRoadOverlayClass ClassifyLine(
		bool bIsCardoOrDecumanus,
		bool bIsCollectorSpacing,
		bool bIsParcelLine)
	{
		if (bIsCardoOrDecumanus)
		{
			return IHInvisibleHandSpec::ETownGridRoadOverlayClass::MajorArtery;
		}
		if (bIsCollectorSpacing)
		{
			return IHInvisibleHandSpec::ETownGridRoadOverlayClass::Collector;
		}
		if (bIsParcelLine)
		{
			return IHInvisibleHandSpec::ETownGridRoadOverlayClass::LocalStreet;
		}
		return IHInvisibleHandSpec::ETownGridRoadOverlayClass::LocalStreet;
	}
}

FVector2D IH_TownGridSquaredGenerator::SnapHalfExtentToModules(
	FVector2D HalfExtentCm,
	float ModuleSizeCm,
	int32 MinModulesPerAxis)
{
	const float MinHalf = ModuleSizeCm * static_cast<float>(MinModulesPerAxis) * 0.5f;
	auto SnapAxis = [ModuleSizeCm, MinHalf, MinModulesPerAxis](float HalfCm) -> float
	{
		const float Modules = FMath::Max(
			static_cast<float>(MinModulesPerAxis),
			FMath::RoundToFloat((HalfCm * 2.f) / ModuleSizeCm));
		return FMath::Max(MinHalf, Modules * ModuleSizeCm * 0.5f);
	};
	return FVector2D(SnapAxis(HalfExtentCm.X), SnapAxis(HalfExtentCm.Y));
}

void IH_TownGridSquaredGenerator::GenerateSquared(
	const FIHTownGridGeneratorParams& Params,
	FTownGridOverlayData& OutOverlay)
{
	using namespace IH_TownGridSquaredGeneratorPrivate;

	OutOverlay.Segments.Reset();
	OutOverlay.CommonsCells.Reset();

	const FVector2D Half = SnapHalfExtentToModules(Params.BboxHalfExtentCm, Params.ModuleSizeCm);
	const float Module = Params.ModuleSizeCm;
	const int32 ModulesX = FMath::Max(1, FMath::RoundToInt((Half.X * 2.f) / Module));
	const int32 ModulesY = FMath::Max(1, FMath::RoundToInt((Half.Y * 2.f) / Module));
	const int32 CollectorInterval = FMath::Max(1, Params.CollectorIntervalModules);
	const int32 CommonsModules = FMath::Clamp(Params.CommonsModules, 2, FMath::Min(ModulesX, ModulesY));

	const float CommonsHalf = CommonsModules * Module * 0.5f;
	FTownGridOverlayCommonsCell Commons;
	Commons.CenterWorld = LocalToWorldXY(Params.CenterWorldCm, Params.YawDeg, FVector2D::ZeroVector);
	Commons.HalfExtentLocalCm = FVector2D(CommonsHalf, CommonsHalf);
	Commons.ZonePrimary = Params.CommonsZonePrimary;
	Commons.ZoneSecondary = Params.CommonsZoneSecondary;
	OutOverlay.CommonsCells.Add(Commons);

	auto IsInsideCommons = [CommonsHalf](float LocalCoord) -> bool
	{
		return FMath::Abs(LocalCoord) < CommonsHalf - KINDA_SMALL_NUMBER;
	};

	for (int32 LineIndex = 0; LineIndex <= ModulesX; ++LineIndex)
	{
		const float LocalX = -Half.X + static_cast<float>(LineIndex) * Module;
		const bool bCardo = FMath::IsNearlyZero(LocalX, 1.f);
		const bool bCollector = (LineIndex % CollectorInterval) == 0;
		const bool bParcelLine = !bCardo && !bCollector;

		const IHInvisibleHandSpec::ETownGridRoadOverlayClass RoadClass =
			ClassifyLine(bCardo, bCollector && !bCardo, bParcelLine);

		float StartY = -Half.Y;
		float EndY = Half.Y;
		if (bCardo)
		{
			AddSegment(
				Params.CenterWorldCm, Params.YawDeg,
				FVector2D(LocalX, StartY), FVector2D(LocalX, EndY),
				RoadClass, false, OutOverlay);
			continue;
		}

		TArray<TPair<float, float>> Spans;
		float SpanStart = StartY;
		bool bSpanInCommons = IsInsideCommons(SpanStart);
		for (int32 YIndex = 0; YIndex <= ModulesY; ++YIndex)
		{
			const float LocalY = -Half.Y + static_cast<float>(YIndex) * Module;
			const bool bInCommons = IsInsideCommons(LocalY);
			if (YIndex == 0)
			{
				bSpanInCommons = bInCommons;
				SpanStart = LocalY;
				continue;
			}
			if (bInCommons != bSpanInCommons || YIndex == ModulesY)
			{
				const float SpanEnd = (YIndex == ModulesY) ? EndY : LocalY;
				if (!bSpanInCommons && SpanEnd > SpanStart + KINDA_SMALL_NUMBER)
				{
					Spans.Add(TPair<float, float>(SpanStart, SpanEnd));
				}
				SpanStart = LocalY;
				bSpanInCommons = bInCommons;
			}
		}

		for (const TPair<float, float>& Span : Spans)
		{
			AddSegment(
				Params.CenterWorldCm, Params.YawDeg,
				FVector2D(LocalX, Span.Key), FVector2D(LocalX, Span.Value),
				RoadClass, false, OutOverlay);
		}
	}

	for (int32 LineIndex = 0; LineIndex <= ModulesY; ++LineIndex)
	{
		const float LocalY = -Half.Y + static_cast<float>(LineIndex) * Module;
		const bool bDecumanus = FMath::IsNearlyZero(LocalY, 1.f);
		const bool bCollector = (LineIndex % CollectorInterval) == 0;
		const bool bParcelLine = !bDecumanus && !bCollector;

		const IHInvisibleHandSpec::ETownGridRoadOverlayClass RoadClass =
			ClassifyLine(bDecumanus, bCollector && !bDecumanus, bParcelLine);

		float StartX = -Half.X;
		float EndX = Half.X;
		if (bDecumanus)
		{
			AddSegment(
				Params.CenterWorldCm, Params.YawDeg,
				FVector2D(StartX, LocalY), FVector2D(EndX, LocalY),
				RoadClass, false, OutOverlay);
			continue;
		}

		TArray<TPair<float, float>> Spans;
		float SpanStart = StartX;
		bool bSpanInCommons = IsInsideCommons(SpanStart);
		for (int32 XIndex = 0; XIndex <= ModulesX; ++XIndex)
		{
			const float LocalX = -Half.X + static_cast<float>(XIndex) * Module;
			const bool bInCommons = IsInsideCommons(LocalX);
			if (XIndex == 0)
			{
				bSpanInCommons = bInCommons;
				SpanStart = LocalX;
				continue;
			}
			if (bInCommons != bSpanInCommons || XIndex == ModulesX)
			{
				const float SpanEnd = (XIndex == ModulesX) ? EndX : LocalX;
				if (!bSpanInCommons && SpanEnd > SpanStart + KINDA_SMALL_NUMBER)
				{
					Spans.Add(TPair<float, float>(SpanStart, SpanEnd));
				}
				SpanStart = LocalX;
				bSpanInCommons = bInCommons;
			}
		}

		for (const TPair<float, float>& Span : Spans)
		{
			AddSegment(
				Params.CenterWorldCm, Params.YawDeg,
				FVector2D(Span.Key, LocalY), FVector2D(Span.Value, LocalY),
				RoadClass, false, OutOverlay);
		}
	}

	const float CH = CommonsHalf;
	AddSegment(
		Params.CenterWorldCm, Params.YawDeg,
		FVector2D(-CH, -CH), FVector2D(CH, -CH),
		IHInvisibleHandSpec::ETownGridRoadOverlayClass::MinorArtery, true, OutOverlay);
	AddSegment(
		Params.CenterWorldCm, Params.YawDeg,
		FVector2D(CH, -CH), FVector2D(CH, CH),
		IHInvisibleHandSpec::ETownGridRoadOverlayClass::MinorArtery, true, OutOverlay);
	AddSegment(
		Params.CenterWorldCm, Params.YawDeg,
		FVector2D(CH, CH), FVector2D(-CH, CH),
		IHInvisibleHandSpec::ETownGridRoadOverlayClass::MinorArtery, true, OutOverlay);
	AddSegment(
		Params.CenterWorldCm, Params.YawDeg,
		FVector2D(-CH, CH), FVector2D(-CH, -CH),
		IHInvisibleHandSpec::ETownGridRoadOverlayClass::MinorArtery, true, OutOverlay);

	AddSegment(
		Params.CenterWorldCm, Params.YawDeg,
		FVector2D(0.f, -CH), FVector2D(0.f, CH),
		IHInvisibleHandSpec::ETownGridRoadOverlayClass::MinorArtery, true, OutOverlay);
	AddSegment(
		Params.CenterWorldCm, Params.YawDeg,
		FVector2D(-CH, 0.f), FVector2D(CH, 0.f),
		IHInvisibleHandSpec::ETownGridRoadOverlayClass::MinorArtery, true, OutOverlay);
}
