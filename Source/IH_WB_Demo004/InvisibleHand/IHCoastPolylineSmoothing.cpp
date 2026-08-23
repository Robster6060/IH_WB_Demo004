// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHCoastPolylineSmoothing.h"

#include "Algo/Reverse.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IH_WB_Demo004.h"

namespace
{
	static void RemoveClosingDuplicate(TArray<FVector2D>& PolylineKm)
	{
		if (PolylineKm.Num() >= 2 && PolylineKm[0].Equals(PolylineKm.Last(), KINDA_SMALL_NUMBER))
		{
			PolylineKm.RemoveAt(PolylineKm.Num() - 1, 1, EAllowShrinking::No);
		}
	}

	static int32 CoastNoiseSeed(int32 MasterSeed, int32 IslandIndex, int32 PolySalt, int32 OctaveSalt)
	{
		return HashCombine(
			HashCombine(HashCombine(MasterSeed, IslandIndex * 7919), PolySalt * 104729),
			OctaveSalt);
	}

	static float LatticeNoise1D(int32 Seed, int32 LatticeIndex)
	{
		FRandomStream Stream(HashCombine(Seed, LatticeIndex * 92821));
		return Stream.FRandRange(-1.f, 1.f);
	}

	static float SmoothNoise1D(int32 Seed, float T)
	{
		const float F = T * 6.103515625f;
		const int32 I0 = FMath::FloorToInt(F);
		const int32 I1 = I0 + 1;
		const float Frac = F - static_cast<float>(I0);
		const float S = Frac * Frac * (3.f - 2.f * Frac);
		return FMath::Lerp(LatticeNoise1D(Seed, I0), LatticeNoise1D(Seed, I1), S);
	}

	static FVector2D PolylineCentroidKm(const TArray<FVector2D>& PolylineKm)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		if (PolylineKm.Num() == 0)
		{
			return Sum;
		}
		for (const FVector2D& P : PolylineKm)
		{
			Sum += P;
		}
		return Sum / static_cast<float>(PolylineKm.Num());
	}

	static FVector2D OutwardNormalAtVertex(
		const TArray<FVector2D>& PolylineKm,
		int32 Index,
		bool bLandInsidePoly)
	{
		const int32 N = PolylineKm.Num();
		if (N < 3)
		{
			return FVector2D(1.f, 0.f);
		}

		const FVector2D& Prev = PolylineKm[(Index + N - 1) % N];
		const FVector2D& Cur = PolylineKm[Index];
		const FVector2D& Next = PolylineKm[(Index + 1) % N];

		const FVector2D E0 = (Cur - Prev).GetSafeNormal();
		const FVector2D E1 = (Next - Cur).GetSafeNormal();
		const FVector2D Left0(-E0.Y, E0.X);
		const FVector2D Left1(-E1.Y, E1.X);
		FVector2D Out = (Left0 + Left1).GetSafeNormal();
		const bool bUsedRadialFallback = Out.IsNearlyZero();
		if (bUsedRadialFallback)
		{
			// Centroid radial is already outward at convex corners; flipping would push noise inward.
			Out = (Cur - PolylineCentroidKm(PolylineKm)).GetSafeNormal();
		}
		else if (bLandInsidePoly)
		{
			// CCW loops with land inside: averaged left normals point inward; negate for outward.
			Out = -Out;
		}
		return Out;
	}

	static float ArcLengthAtVertex(const TArray<FVector2D>& PolylineKm, int32 Index)
	{
		float Arc = 0.f;
		for (int32 i = 0; i < Index; ++i)
		{
			Arc += FVector2D::Distance(PolylineKm[i], PolylineKm[i + 1]);
		}
		return Arc;
	}

	static float TotalPerimeterKm(const TArray<FVector2D>& PolylineKm)
	{
		float Total = 0.f;
		const int32 N = PolylineKm.Num();
		for (int32 i = 0; i < N; ++i)
		{
			Total += FVector2D::Distance(PolylineKm[i], PolylineKm[(i + 1) % N]);
		}
		return Total;
	}

	static float MacroOnlyEdgeOffsetKm(
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		float ArcKm,
		float AmpScale = 1.f)
	{
		return IHInvisibleHandSpec::CoastNoiseMacroAmpKm * AmpScale
			* SmoothNoise1D(
				CoastNoiseSeed(MasterSeed, IslandIndex, PolySalt, 1),
				ArcKm / FMath::Max(IHInvisibleHandSpec::CoastNoiseMacroWavelengthKm, 0.01f));
	}

	static float MultiOctaveEdgeOffsetKm(
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		float ArcKm,
		float AmpScale = 1.f)
	{
		const float Macro = MacroOnlyEdgeOffsetKm(
			MasterSeed, IslandIndex, PolySalt, ArcKm, AmpScale);
		const float Meso = IHInvisibleHandSpec::CoastNoiseMesoAmpKm * AmpScale
			* SmoothNoise1D(
				CoastNoiseSeed(MasterSeed, IslandIndex, PolySalt, 2),
				ArcKm / FMath::Max(IHInvisibleHandSpec::CoastNoiseMesoWavelengthKm, 0.01f));
		const float Micro = IHInvisibleHandSpec::CoastNoiseMicroAmpKm * AmpScale
			* SmoothNoise1D(
				CoastNoiseSeed(MasterSeed, IslandIndex, PolySalt, 3),
				ArcKm / FMath::Max(IHInvisibleHandSpec::CoastNoiseMicroWavelengthKm, 0.01f));
		return Macro + Meso + Micro;
	}

	static float CoastEdgeOffsetKm(
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		float ArcKm,
		float AmpScale,
		const bool bMacroOnly)
	{
		return bMacroOnly
			? MacroOnlyEdgeOffsetKm(MasterSeed, IslandIndex, PolySalt, ArcKm, AmpScale)
			: MultiOctaveEdgeOffsetKm(MasterSeed, IslandIndex, PolySalt, ArcKm, AmpScale);
	}

	static void ApplyCoastEdgeNoiseKm(
		TArray<FVector2D>& InOutPolylineKm,
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		bool bLandInsidePoly,
		float NoiseAmpScale,
		const bool bMacroOnly = false)
	{
		if (InOutPolylineKm.Num() < 3)
		{
			return;
		}

		RemoveClosingDuplicate(InOutPolylineKm);
		const int32 N = InOutPolylineKm.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const float Arc = ArcLengthAtVertex(InOutPolylineKm, i);
			const float Offset = CoastEdgeOffsetKm(
				MasterSeed, IslandIndex, PolySalt, Arc, NoiseAmpScale, bMacroOnly);
			const FVector2D Normal = OutwardNormalAtVertex(InOutPolylineKm, i, bLandInsidePoly);
			InOutPolylineKm[i] += Normal * Offset;
		}
	}

	static void BreakLongStraightSegmentsKm(
		TArray<FVector2D>& InOutPolylineKm,
		float MaxStraightKm,
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		bool bLandInsidePoly,
		float NoiseAmpScale,
		const bool bMacroOnly = false)
	{
		if (InOutPolylineKm.Num() < 3 || MaxStraightKm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		RemoveClosingDuplicate(InOutPolylineKm);
		const int32 N = InOutPolylineKm.Num();
		TArray<FVector2D> Rebuilt;
		Rebuilt.Reserve(N * 3);

		// Running cumulative arc length in place of ArcLengthAtVertex(): that helper recomputes
		// the sum from vertex 0 on every call, which is O(N) per call and O(N^2) total across
		// this loop — measured to hang generation on large coastlines (Do No Harm revert
		// 6423e83). ArcAtI below is numerically identical to ArcLengthAtVertex(InOutPolylineKm, i).
		float RunningArcKm = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = InOutPolylineKm[i];
			const FVector2D& B = InOutPolylineKm[(i + 1) % N];
			const float SegLen = FVector2D::Distance(A, B);
			Rebuilt.Add(A);
			const float ArcAtI = RunningArcKm;
			RunningArcKm += SegLen;

			if (SegLen <= MaxStraightKm)
			{
				continue;
			}

			const int32 InsertCount = FMath::Clamp(
				FMath::CeilToInt(SegLen / (MaxStraightKm * 0.55f)) - 1, 1, 8);
			for (int32 Step = 1; Step <= InsertCount; ++Step)
			{
				const float Alpha = static_cast<float>(Step) / static_cast<float>(InsertCount + 1);
				const FVector2D Mid = FMath::Lerp(A, B, Alpha);
				const float Arc = ArcAtI + SegLen * Alpha;
				const float KickScale = bMacroOnly ? 0.75f : 1.35f;
				const float Kick = CoastEdgeOffsetKm(
					MasterSeed, IslandIndex, PolySalt + 17, Arc, NoiseAmpScale, bMacroOnly) * KickScale;
				const FVector2D Edge = (B - A).GetSafeNormal();
				const FVector2D Out = bLandInsidePoly
					? FVector2D(Edge.Y, -Edge.X)
					: FVector2D(-Edge.Y, Edge.X);
				Rebuilt.Add(Mid + Out * Kick);
			}
		}

		InOutPolylineKm = MoveTemp(Rebuilt);
	}

	/** +1 E, -1 W, +2 N, -2 S, 0 degenerate, 99 diagonal/oblique. */
	static int32 ClassifyCardinalEdgeDir(const FVector2D& A, const FVector2D& B)
	{
		const FVector2D D = B - A;
		const float Len = D.Size();
		if (Len < 1.e-7f)
		{
			return 0;
		}
		const float Ax = FMath::Abs(D.X) / Len;
		const float Ay = FMath::Abs(D.Y) / Len;
		if (Ay < 0.05f)
		{
			return D.X >= 0.f ? 1 : -1;
		}
		if (Ax < 0.05f)
		{
			return D.Y >= 0.f ? 2 : -2;
		}
		return 99;
	}

	static bool IsCardinal90Corner(const int32 DirIn, const int32 DirOut)
	{
		if (DirIn == 0 || DirOut == 0 || DirIn == 99 || DirOut == 99)
		{
			return false;
		}
		return FMath::Abs(DirIn) != FMath::Abs(DirOut);
	}

	static void BreakCollinearCardinalRunsKm(
		TArray<FVector2D>& InOutPolylineKm,
		const float MaxRunKm,
		const int32 MasterSeed,
		const int32 IslandIndex,
		const int32 PolySalt,
		const bool bLandInsidePoly,
		const float NoiseAmpScale)
	{
		RemoveClosingDuplicate(InOutPolylineKm);
		const int32 N = InOutPolylineKm.Num();
		if (N < 4 || MaxRunKm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		TArray<FVector2D> Rebuilt;
		Rebuilt.Reserve(N * 2);
		Rebuilt.Add(InOutPolylineKm[0]);

		// Running cumulative arc length in place of ArcLengthAtVertex() — see
		// BreakLongStraightSegmentsKm above for why. RunningArcKm always holds the arc length
		// (sum of original-polyline segment lengths) from vertex 0 up to InOutPolylineKm[Cursor],
		// numerically identical to ArcLengthAtVertex(InOutPolylineKm, Cursor).
		//
		// Loop bound: ConsumedSteps counts original-polyline edges walked, NOT "Cursor==0".
		// The original `while (Cursor != 0)` is unsound: a run can span across the index-N-1/0
		// wraparound (e.g. a collinear run from N-5 through N-1,0,1 to 4), landing Cursor on 4
		// without ever passing through the literal value 0. Since the direction classification
		// is a fixed deterministic function of the (unchanging) input array, that produces an
		// infinite cycle among a fixed subset of indices that never touches 0 again — confirmed
		// by a 2026-08-12 headless self-test that hung with unbounded memory growth (Rebuilt.Add
		// looping forever) on a 3,701-vertex polyline. Stopping once every original edge has been
		// consumed exactly once is well-defined regardless of where the traversal lands.
		float RunningArcKm = 0.f;
		int32 Cursor = 0;
		int32 ConsumedSteps = 0;
		while (ConsumedSteps < N)
		{
			const int32 Next = (Cursor + 1) % N;
			const int32 Dir = ClassifyCardinalEdgeDir(InOutPolylineKm[Cursor], InOutPolylineKm[Next]);
			if (Dir == 99 || Dir == 0)
			{
				RunningArcKm += FVector2D::Distance(InOutPolylineKm[Cursor], InOutPolylineKm[Next]);
				Rebuilt.Add(InOutPolylineKm[Next]);
				Cursor = Next;
				++ConsumedSteps;
				continue;
			}

			float RunLen = FVector2D::Distance(InOutPolylineKm[Cursor], InOutPolylineKm[Next]);
			int32 RunEnd = Next;
			int32 RunSteps = 1;
			for (; RunSteps < N; )
			{
				const int32 RunNext = (RunEnd + 1) % N;
				if (RunNext == Cursor)
				{
					break;
				}
				const int32 DirNext = ClassifyCardinalEdgeDir(InOutPolylineKm[RunEnd], InOutPolylineKm[RunNext]);
				if (DirNext != Dir)
				{
					break;
				}
				RunLen += FVector2D::Distance(InOutPolylineKm[RunEnd], InOutPolylineKm[RunNext]);
				RunEnd = RunNext;
				++RunSteps;
			}

			const FVector2D RunStart = InOutPolylineKm[Cursor];
			const FVector2D RunEndPt = InOutPolylineKm[RunEnd];
			const float ArcAtCursor = RunningArcKm;
			if (RunLen > MaxRunKm && RunEnd != Cursor)
			{
				const int32 InsertCount = FMath::Clamp(
					FMath::CeilToInt(RunLen / (MaxRunKm * 0.5f)) - 1, 1, 12);
				for (int32 Step = 1; Step <= InsertCount; ++Step)
				{
					const float Alpha = static_cast<float>(Step) / static_cast<float>(InsertCount + 1);
					const FVector2D Mid = FMath::Lerp(RunStart, RunEndPt, Alpha);
					const float Arc = ArcAtCursor + RunLen * Alpha;
					const float Kick = CoastEdgeOffsetKm(
						MasterSeed, IslandIndex, PolySalt + 31, Arc, NoiseAmpScale, true) * 0.85f;
					const FVector2D Edge = (RunEndPt - RunStart).GetSafeNormal();
					const FVector2D Out = bLandInsidePoly
						? FVector2D(Edge.Y, -Edge.X)
						: FVector2D(-Edge.Y, Edge.X);
					Rebuilt.Add(Mid + Out * Kick);
				}
			}

			if (RunEnd == Cursor)
			{
				break;
			}
			RunningArcKm += RunLen;
			Rebuilt.Add(RunEndPt);
			Cursor = RunEnd;
			ConsumedSteps += RunSteps;
		}

		InOutPolylineKm = MoveTemp(Rebuilt);
		RemoveClosingDuplicate(InOutPolylineKm);
	}

	static void ChamferCardinalRightAngleCornersKm(TArray<FVector2D>& InOutPolylineKm)
	{
		RemoveClosingDuplicate(InOutPolylineKm);
		const int32 N = InOutPolylineKm.Num();
		if (N < 4)
		{
			return;
		}

		TArray<FVector2D> Out;
		Out.Reserve(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			const int32 Prev = (i + N - 1) % N;
			const int32 Next = (i + 1) % N;
			const FVector2D& A = InOutPolylineKm[Prev];
			const FVector2D& B = InOutPolylineKm[i];
			const FVector2D& C = InOutPolylineKm[Next];
			const int32 DirIn = ClassifyCardinalEdgeDir(A, B);
			const int32 DirOut = ClassifyCardinalEdgeDir(B, C);
			if (!IsCardinal90Corner(DirIn, DirOut))
			{
				Out.Add(B);
				continue;
			}

			const float LenIn = FVector2D::Distance(A, B);
			const float LenOut = FVector2D::Distance(B, C);
			const float Chamfer = FMath::Clamp(
				FMath::Min(LenIn, LenOut) * IHInvisibleHandSpec::CoastC1d_ChamferLegFraction,
				IHInvisibleHandSpec::CoastC1d_ChamferMinKm,
				IHInvisibleHandSpec::CoastC1d_ChamferMaxKm);
			if (Chamfer * 2.f >= FMath::Min(LenIn, LenOut) * 0.85f)
			{
				Out.Add(B);
				continue;
			}

			const FVector2D InDir = (B - A).GetSafeNormal();
			const FVector2D OutDir = (C - B).GetSafeNormal();
			Out.Add(B - InDir * Chamfer);
			Out.Add(B + OutDir * Chamfer);
		}

		InOutPolylineKm = MoveTemp(Out);
		RemoveClosingDuplicate(InOutPolylineKm);
	}
}

void FIHCoastPolylineSmoothing::SmoothClosedPolylineKm(
	TArray<FVector2D>& InOutPolylineKm,
	const int32 Iterations,
	const float CutRatio)
{
	if (InOutPolylineKm.Num() < 4 || Iterations <= 0)
	{
		return;
	}

	RemoveClosingDuplicate(InOutPolylineKm);
	const float T = FMath::Clamp(CutRatio, 0.05f, 0.45f);

	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		const int32 N = InOutPolylineKm.Num();
		if (N < 3)
		{
			break;
		}

		TArray<FVector2D> Smoothed;
		Smoothed.Reserve(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = InOutPolylineKm[i];
			const FVector2D& B = InOutPolylineKm[(i + 1) % N];
			Smoothed.Add(FMath::Lerp(A, B, T));
			Smoothed.Add(FMath::Lerp(A, B, 1.f - T));
		}
		InOutPolylineKm = MoveTemp(Smoothed);
	}
}

void FIHCoastPolylineSmoothing::DensifyClosedPolylineKm(
	TArray<FVector2D>& InOutPolylineKm,
	const float MaxSpacingKm)
{
	if (InOutPolylineKm.Num() < 3 || MaxSpacingKm <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	RemoveClosingDuplicate(InOutPolylineKm);
	const int32 N = InOutPolylineKm.Num();
	TArray<FVector2D> Dense;
	Dense.Reserve(N * 4);

	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D& A = InOutPolylineKm[i];
		const FVector2D& B = InOutPolylineKm[(i + 1) % N];
		Dense.Add(A);

		const float SegLen = FVector2D::Distance(A, B);
		const int32 Steps = FMath::Max(FMath::CeilToInt(SegLen / MaxSpacingKm) - 1, 0);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / static_cast<float>(Steps + 1);
			Dense.Add(FMath::Lerp(A, B, Alpha));
		}
	}

	InOutPolylineKm = MoveTemp(Dense);
}

void FIHCoastPolylineSmoothing::ResampleClosedPolylineUniformCount(
	const TArray<FVector2D>& PolylineKm,
	const int32 TargetCount,
	TArray<FVector2D>& OutPolylineKm)
{
	// In-place resample (same array for input and output) must not Reset() before reading input.
	if (&PolylineKm == &OutPolylineKm)
	{
		TArray<FVector2D> Temp;
		ResampleClosedPolylineUniformCount(PolylineKm, TargetCount, Temp);
		OutPolylineKm = MoveTemp(Temp);
		return;
	}

	OutPolylineKm.Reset();
	if (PolylineKm.Num() < 3 || TargetCount < 3)
	{
		OutPolylineKm = PolylineKm;
		return;
	}

	TArray<FVector2D> Work = PolylineKm;
	RemoveClosingDuplicate(Work);
	const int32 N = Work.Num();
	if (N < 3)
	{
		OutPolylineKm = PolylineKm;
		return;
	}

	TArray<float> EdgeLengths;
	EdgeLengths.SetNum(N);
	float TotalLength = 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		EdgeLengths[i] = FVector2D::Distance(Work[i], Work[(i + 1) % N]);
		TotalLength += EdgeLengths[i];
	}
	if (TotalLength <= KINDA_SMALL_NUMBER)
	{
		OutPolylineKm = Work;
		return;
	}

	OutPolylineKm.Reserve(TargetCount);
	for (int32 SampleIdx = 0; SampleIdx < TargetCount; ++SampleIdx)
	{
		const float TargetDist = (static_cast<float>(SampleIdx) / static_cast<float>(TargetCount)) * TotalLength;
		float Accum = 0.f;
		for (int32 EdgeIdx = 0; EdgeIdx < N; ++EdgeIdx)
		{
			const float EdgeLen = EdgeLengths[EdgeIdx];
			if (Accum + EdgeLen >= TargetDist || EdgeIdx == N - 1)
			{
				const float Alpha = EdgeLen > KINDA_SMALL_NUMBER
					? FMath::Clamp((TargetDist - Accum) / EdgeLen, 0.f, 1.f)
					: 0.f;
				OutPolylineKm.Add(FMath::Lerp(Work[EdgeIdx], Work[(EdgeIdx + 1) % N], Alpha));
				break;
			}
			Accum += EdgeLen;
		}
	}
}

static FVector2D SampleClosedPolylineAtArcFractionKm(
	const TArray<FVector2D>& PolylineKm,
	const float Fraction01)
{
	TArray<FVector2D> Work = PolylineKm;
	RemoveClosingDuplicate(Work);
	const int32 N = Work.Num();
	if (N < 3)
	{
		return Work.Num() > 0 ? Work[0] : FVector2D::ZeroVector;
	}

	TArray<float> EdgeLengths;
	EdgeLengths.SetNum(N);
	float TotalLength = 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		EdgeLengths[i] = FVector2D::Distance(Work[i], Work[(i + 1) % N]);
		TotalLength += EdgeLengths[i];
	}
	if (TotalLength <= KINDA_SMALL_NUMBER)
	{
		return Work[0];
	}

	const float ClampedFraction = FMath::Clamp(Fraction01, 0.f, 1.f);
	const float TargetDist = ClampedFraction * TotalLength;
	float Accum = 0.f;
	for (int32 EdgeIdx = 0; EdgeIdx < N; ++EdgeIdx)
	{
		const float EdgeLen = EdgeLengths[EdgeIdx];
		if (Accum + EdgeLen >= TargetDist || EdgeIdx == N - 1)
		{
			const float Alpha = EdgeLen > KINDA_SMALL_NUMBER
				? FMath::Clamp((TargetDist - Accum) / EdgeLen, 0.f, 1.f)
				: 0.f;
			return FMath::Lerp(Work[EdgeIdx], Work[(EdgeIdx + 1) % N], Alpha);
		}
		Accum += EdgeLen;
	}

	return Work[0];
}

void FIHCoastPolylineSmoothing::ResampleClosedPolylineToReferenceArcLength(
	const TArray<FVector2D>& ReferenceKm,
	const TArray<FVector2D>& SourceKm,
	TArray<FVector2D>& OutPolylineKm)
{
	if (&ReferenceKm == &OutPolylineKm || &SourceKm == &OutPolylineKm)
	{
		TArray<FVector2D> Temp;
		ResampleClosedPolylineToReferenceArcLength(ReferenceKm, SourceKm, Temp);
		OutPolylineKm = MoveTemp(Temp);
		return;
	}

	OutPolylineKm.Reset();
	const int32 ReferenceCount = ReferenceKm.Num();
	if (ReferenceCount < 3 || SourceKm.Num() < 3)
	{
		OutPolylineKm = SourceKm;
		return;
	}

	OutPolylineKm.Reserve(ReferenceCount);
	for (int32 SampleIdx = 0; SampleIdx < ReferenceCount; ++SampleIdx)
	{
		const float ArcFraction = static_cast<float>(SampleIdx) / static_cast<float>(ReferenceCount);
		OutPolylineKm.Add(SampleClosedPolylineAtArcFractionKm(SourceKm, ArcFraction));
	}
}

void FIHCoastPolylineSmoothing::ProjectClosedPolylineOntoNearestPoint(
	const TArray<FVector2D>& ReferenceKm,
	const TArray<FVector2D>& OuterKm,
	TArray<FVector2D>& OutPairedKm)
{
	if (&ReferenceKm == &OutPairedKm || &OuterKm == &OutPairedKm)
	{
		TArray<FVector2D> Temp;
		ProjectClosedPolylineOntoNearestPoint(ReferenceKm, OuterKm, Temp);
		OutPairedKm = MoveTemp(Temp);
		return;
	}

	OutPairedKm.Reset();
	const int32 ReferenceCount = ReferenceKm.Num();
	const int32 OuterCount = OuterKm.Num();
	if (ReferenceCount < 3 || OuterCount < 3)
	{
		OutPairedKm = OuterKm;
		return;
	}

	OutPairedKm.Reserve(ReferenceCount);
	for (int32 RefIdx = 0; RefIdx < ReferenceCount; ++RefIdx)
	{
		const FVector2D& P = ReferenceKm[RefIdx];
		FVector2D BestPoint = OuterKm[0];
		float BestDistSqKm = TNumericLimits<float>::Max();
		for (int32 SegIdx = 0; SegIdx < OuterCount; ++SegIdx)
		{
			const FVector2D& A = OuterKm[SegIdx];
			const FVector2D& B = OuterKm[(SegIdx + 1) % OuterCount];
			const FVector2D AB = B - A;
			const float LenSqKm = static_cast<float>(AB.SizeSquared());
			float T = LenSqKm > KINDA_SMALL_NUMBER
				? static_cast<float>(FVector2D::DotProduct(P - A, AB)) / LenSqKm : 0.f;
			T = FMath::Clamp(T, 0.f, 1.f);
			const FVector2D Projected = A + AB * T;
			const float DistSqKm = static_cast<float>(FVector2D::DistSquared(P, Projected));
			if (DistSqKm < BestDistSqKm)
			{
				BestDistSqKm = DistSqKm;
				BestPoint = Projected;
			}
		}
		OutPairedKm.Add(BestPoint);
	}
}

void FIHCoastPolylineSmoothing::ResampleClosedPolylineToArcLengthKeys(
	const TArray<FVector2D>& SourceKm,
	const int32 ReferenceCount,
	TArray<FVector2D>& OutPolylineKm)
{
	if (&SourceKm == &OutPolylineKm)
	{
		TArray<FVector2D> Temp;
		ResampleClosedPolylineToArcLengthKeys(SourceKm, ReferenceCount, Temp);
		OutPolylineKm = MoveTemp(Temp);
		return;
	}

	ResampleClosedPolylineUniformCount(SourceKm, ReferenceCount, OutPolylineKm);
}

float FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(TArray<FVector2D>& InOutPolylineKm)
{
	RemoveClosingDuplicate(InOutPolylineKm);
	const int32 N = InOutPolylineKm.Num();
	if (N < 3)
	{
		return 0.f;
	}

	float SignedArea = 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D& A = InOutPolylineKm[i];
		const FVector2D& B = InOutPolylineKm[(i + 1) % N];
		SignedArea += A.X * B.Y - B.X * A.Y;
	}
	SignedArea *= 0.5f;

	if (SignedArea < 0.f)
	{
		Algo::Reverse(InOutPolylineKm);
		SignedArea = -SignedArea;
	}

	return SignedArea;
}

namespace
{
	static bool IntersectLines2DKm(
		const FVector2D& P0,
		const FVector2D& D0,
		const FVector2D& P1,
		const FVector2D& D1,
		FVector2D& OutPoint)
	{
		const float Denom = D0.X * D1.Y - D0.Y * D1.X;
		if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const FVector2D Delta = P1 - P0;
		const float T = (Delta.X * D1.Y - Delta.Y * D1.X) / Denom;
		OutPoint = P0 + D0 * T;
		return true;
	}

	static FVector2D OutwardNormalForEdgeKm(
		const FVector2D& EdgeDirNormalized,
		const bool bLandInsidePoly)
	{
		return bLandInsidePoly
			? FVector2D(EdgeDirNormalized.Y, -EdgeDirNormalized.X)
			: FVector2D(-EdgeDirNormalized.Y, EdgeDirNormalized.X);
	}
}

void FIHCoastPolylineSmoothing::OffsetClosedPolylineOutwardKm(
	const TArray<FVector2D>& PolylineKm,
	const float OutwardOffsetKm,
	const bool bLandInsidePoly,
	TArray<FVector2D>& OutOffsetKm)
{
	OutOffsetKm.Reset();
	if (PolylineKm.Num() < 3 || FMath::Abs(OutwardOffsetKm) <= KINDA_SMALL_NUMBER)
	{
		OutOffsetKm = PolylineKm;
		return;
	}

	const int32 N = PolylineKm.Num();
	OutOffsetKm.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const int32 iPrev = (i + N - 1) % N;
		const int32 iNext = (i + 1) % N;

		const FVector2D& VPrev = PolylineKm[iPrev];
		const FVector2D& V = PolylineKm[i];
		const FVector2D& VNext = PolylineKm[iNext];

		const FVector2D EdgeIn = (V - VPrev).GetSafeNormal();
		const FVector2D EdgeOut = (VNext - V).GetSafeNormal();
		const FVector2D NIn = OutwardNormalForEdgeKm(EdgeIn, bLandInsidePoly);
		const FVector2D NOut = OutwardNormalForEdgeKm(EdgeOut, bLandInsidePoly);

		const FVector2D PIn = VPrev + NIn * OutwardOffsetKm;
		const FVector2D POut = V + NOut * OutwardOffsetKm;

		FVector2D MiterPt = V + (NIn + NOut) * (0.5f * OutwardOffsetKm);
		if (IntersectLines2DKm(PIn, EdgeIn, POut, EdgeOut, MiterPt))
		{
			const float MiterLen = FVector2D::Distance(MiterPt, V);
			if (MiterLen > OutwardOffsetKm * IHInvisibleHandSpec::ShelfOffsetMiterLimit)
			{
				MiterPt = V + (NIn + NOut).GetSafeNormal() * OutwardOffsetKm;
			}
		}

		OutOffsetKm[i] = MiterPt;
	}
}

void FIHCoastPolylineSmoothing::OffsetClosedPolylineVariableOutwardKm(
	const TArray<FVector2D>& PolylineKm,
	const TArray<float>& VertexOffsetKm,
	const bool bLandInsidePoly,
	TArray<FVector2D>& OutOffsetKm)
{
	OutOffsetKm.Reset();
	if (PolylineKm.Num() < 3)
	{
		OutOffsetKm = PolylineKm;
		return;
	}

	const int32 N = PolylineKm.Num();
	if (VertexOffsetKm.Num() != N)
	{
		float AvgOffsetKm = 0.f;
		if (VertexOffsetKm.Num() > 0)
		{
			for (const float OffsetKm : VertexOffsetKm)
			{
				AvgOffsetKm += OffsetKm;
			}
			AvgOffsetKm /= static_cast<float>(VertexOffsetKm.Num());
		}
		OffsetClosedPolylineOutwardKm(PolylineKm, AvgOffsetKm, bLandInsidePoly, OutOffsetKm);
		return;
	}

	OutOffsetKm.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const int32 iPrev = (i + N - 1) % N;
		const int32 iNext = (i + 1) % N;

		const FVector2D& VPrev = PolylineKm[iPrev];
		const FVector2D& V = PolylineKm[i];
		const FVector2D& VNext = PolylineKm[iNext];

		const FVector2D EdgeIn = (V - VPrev).GetSafeNormal();
		const FVector2D EdgeOut = (VNext - V).GetSafeNormal();
		const FVector2D NIn = OutwardNormalForEdgeKm(EdgeIn, bLandInsidePoly);
		const FVector2D NOut = OutwardNormalForEdgeKm(EdgeOut, bLandInsidePoly);

		const float OffsetInKm = 0.5f * (VertexOffsetKm[iPrev] + VertexOffsetKm[i]);
		const float OffsetOutKm = 0.5f * (VertexOffsetKm[i] + VertexOffsetKm[iNext]);
		const float VertexOffsetKm_i = VertexOffsetKm[i];

		const FVector2D PIn = VPrev + NIn * OffsetInKm;
		const FVector2D POut = V + NOut * OffsetOutKm;

		FVector2D MiterPt = V + (NIn + NOut) * (0.5f * VertexOffsetKm_i);
		if (IntersectLines2DKm(PIn, EdgeIn, POut, EdgeOut, MiterPt))
		{
			const float MiterLen = FVector2D::Distance(MiterPt, V);
			const float MiterRefKm = FMath::Max(
				VertexOffsetKm_i, FMath::Max(OffsetInKm, OffsetOutKm));
			if (MiterLen > MiterRefKm * IHInvisibleHandSpec::ShelfOffsetMiterLimit)
			{
				MiterPt = V + (NIn + NOut).GetSafeNormal() * VertexOffsetKm_i;
			}
		}

		OutOffsetKm[i] = MiterPt;
	}
}

bool FIHCoastPolylineSmoothing::IsShelfAnnulusQuadOutwardValid(
	const FVector2D& InnerA,
	const FVector2D& InnerB,
	const FVector2D& OuterA,
	const FVector2D& OuterB,
	const FVector2D& OutwardNormal,
	const float MinOutwardDistance)
{
	if (!FMath::IsFinite(InnerA.X) || !FMath::IsFinite(InnerA.Y)
		|| !FMath::IsFinite(InnerB.X) || !FMath::IsFinite(InnerB.Y)
		|| !FMath::IsFinite(OuterA.X) || !FMath::IsFinite(OuterA.Y)
		|| !FMath::IsFinite(OuterB.X) || !FMath::IsFinite(OuterB.Y)
		|| !FMath::IsFinite(OutwardNormal.X) || !FMath::IsFinite(OutwardNormal.Y))
	{
		return false;
	}

	// Outer must radiate along OutwardNormal (segment right for CCW land-inside coasts).
	// Do NOT use edge×(Outer-Inner)>0 — that selects the inland (left) side on CCW loops.
	if (FVector2D::DotProduct(OuterA - InnerA, OutwardNormal) < MinOutwardDistance
		|| FVector2D::DotProduct(OuterB - InnerB, OutwardNormal) < MinOutwardDistance)
	{
		return false;
	}

	return true;
}

bool FIHCoastPolylineSmoothing::IsQuadOutwardValid(
	const FVector2D& InnerA,
	const FVector2D& InnerB,
	const FVector2D& OuterA,
	const FVector2D& OuterB,
	const FVector2D& OutwardNormal,
	const float MinOutwardKm)
{
	return IsShelfAnnulusQuadOutwardValid(
		InnerA, InnerB, OuterA, OuterB, OutwardNormal, MinOutwardKm);
}

bool FIHCoastPolylineSmoothing::IsPointInsideClosedPolylineKm(
	const FVector2D& PointKm,
	const TArray<FVector2D>& PolylineKm)
{
	if (PolylineKm.Num() < 3)
	{
		return false;
	}

	int32 Crossings = 0;
	const int32 N = PolylineKm.Num();

	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D& A = PolylineKm[i];
		const FVector2D& B = PolylineKm[(i + 1) % N];
		const bool bStraddle = (A.Y > PointKm.Y) != (B.Y > PointKm.Y);
		if (!bStraddle)
		{
			continue;
		}
		const float SpanY = B.Y - A.Y;
		if (FMath::Abs(SpanY) < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float XIntersect = A.X + (B.X - A.X) * (PointKm.Y - A.Y) / SpanY;
		if (XIntersect > PointKm.X)
		{
			++Crossings;
		}
	}

	return (Crossings & 1) == 1;
}

void FIHCoastPolylineSmoothing::ComputeCoastGridArtifactMetricsKm(
	const TArray<FVector2D>& PolylineKm,
	FIHCoastPolylineSmoothing::FIHCoastGridArtifactMetrics& OutMetrics)
{
	OutMetrics = FIHCoastGridArtifactMetrics();
	TArray<FVector2D> Work = PolylineKm;
	RemoveClosingDuplicate(Work);
	const int32 N = Work.Num();
	if (N < 3)
	{
		return;
	}

	float PerimeterKm = 0.f;
	float CardinalLenKm = 0.f;
	int32 Cursor = 0;
	const int32 StartCursor = 0;
	bool bClosedLoop = false;

	do
	{
		if (Cursor == StartCursor && PerimeterKm > 0.f)
		{
			bClosedLoop = true;
			break;
		}

		const int32 Next = (Cursor + 1) % N;
		const int32 Dir = ClassifyCardinalEdgeDir(Work[Cursor], Work[Next]);
		if (Dir == 99 || Dir == 0)
		{
			const float SegLen = FVector2D::Distance(Work[Cursor], Work[Next]);
			PerimeterKm += SegLen;
			Cursor = Next;
			continue;
		}

		float RunLen = FVector2D::Distance(Work[Cursor], Work[Next]);
		int32 RunEnd = Next;
		for (;;)
		{
			const int32 RunNext = (RunEnd + 1) % N;
			if (RunNext == Cursor)
			{
				break;
			}
			const int32 DirNext = ClassifyCardinalEdgeDir(Work[RunEnd], Work[RunNext]);
			if (DirNext != Dir)
			{
				break;
			}
			RunLen += FVector2D::Distance(Work[RunEnd], Work[RunNext]);
			RunEnd = RunNext;
		}

		OutMetrics.MaxCardinalRunKm = FMath::Max(OutMetrics.MaxCardinalRunKm, RunLen);
		PerimeterKm += RunLen;
		CardinalLenKm += RunLen;
		if (RunEnd == Cursor)
		{
			break;
		}
		Cursor = RunEnd;
	}
	while (!bClosedLoop && Cursor != StartCursor);

	if (PerimeterKm > KINDA_SMALL_NUMBER)
	{
		OutMetrics.CardinalPerimeterFraction = CardinalLenKm / PerimeterKm;
	}

	const float MinLegKm = IHInvisibleHandSpec::CoastC1d_ProminentCornerMinLegKm;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Prev = (i + N - 1) % N;
		const int32 Next = (i + 1) % N;
		const int32 DirIn = ClassifyCardinalEdgeDir(Work[Prev], Work[i]);
		const int32 DirOut = ClassifyCardinalEdgeDir(Work[i], Work[Next]);
		if (!IsCardinal90Corner(DirIn, DirOut))
		{
			continue;
		}
		const float LenIn = FVector2D::Distance(Work[Prev], Work[i]);
		const float LenOut = FVector2D::Distance(Work[i], Work[Next]);
		if (LenIn >= MinLegKm && LenOut >= MinLegKm)
		{
			++OutMetrics.Prominent90CornerCount;
		}
	}
}

void FIHCoastPolylineSmoothing::ApplyCoastC1dGridArtifactRemedyKm(
	TArray<FVector2D>& InOutPolylineKm,
	const int32 MasterSeed,
	const int32 IslandIndex,
	const int32 PolySalt,
	const bool bLandInsidePoly,
	const float NoiseAmpScale)
{
	if (!IHInvisibleHandSpec::IsCoastC1dGridArtifactRemedyEnabled() || InOutPolylineKm.Num() < 3)
	{
		return;
	}

	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
	const float AmpScale = FMath::Max(NoiseAmpScale, 0.1f);

	if (IHInvisibleHandSpec::IsCoastC1dCardinalStairstepBreakerEnabled())
	{
		BreakCollinearCardinalRunsKm(
			InOutPolylineKm,
			IHInvisibleHandSpec::CoastC1d_MaxCollinearCardinalRunKm,
			MasterSeed,
			IslandIndex,
			PolySalt,
			bLandInsidePoly,
			AmpScale);
	}

	if (IHInvisibleHandSpec::bCoastC1d_CardinalCornerChamferEnabled)
	{
		ChamferCardinalRightAngleCornersKm(InOutPolylineKm);
	}

	if (IHInvisibleHandSpec::bCoastC2a_BreakLongStraightSegmentsInC1dRemedy)
	{
		BreakLongStraightSegmentsKm(
			InOutPolylineKm,
			IHInvisibleHandSpec::CoastMaxStraightSegmentKm,
			MasterSeed,
			IslandIndex,
			PolySalt + 53,
			bLandInsidePoly,
			AmpScale,
			false);
	}

	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
}

void FIHCoastPolylineSmoothing::RefineCoastPolylineKm(
	TArray<FVector2D>& InOutPolylineKm,
	const int32 MasterSeed,
	const int32 IslandIndex,
	const int32 PolySalt,
	const bool bLandInsidePoly,
	const float NoiseAmpScale)
{
	if (InOutPolylineKm.Num() < 3)
	{
		return;
	}

	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
	const float AmpScale = FMath::Max(NoiseAmpScale, 0.1f);
	if (IHInvisibleHandSpec::bCoastAuthorityRefineNoiseEnabled)
	{
		FIHCoastGridArtifactMetrics RefineMetrics;
		ComputeCoastGridArtifactMetricsKm(InOutPolylineKm, RefineMetrics);
		if (RefineMetrics.MaxCardinalRunKm <= IHInvisibleHandSpec::CoastC1d_PreAuthorityBailMaxCardinalRunKm)
		{
			DensifyClosedPolylineKm(InOutPolylineKm, IHInvisibleHandSpec::CoastEdgeNoiseDensifySpacingKm);
		}
		BreakLongStraightSegmentsKm(
			InOutPolylineKm,
			IHInvisibleHandSpec::CoastMaxStraightSegmentKm,
			MasterSeed,
			IslandIndex,
			PolySalt,
			bLandInsidePoly,
			AmpScale,
			false);
		ApplyCoastEdgeNoiseKm(
			InOutPolylineKm, MasterSeed, IslandIndex, PolySalt, bLandInsidePoly, AmpScale, false);
		SmoothClosedPolylineKm(
			InOutPolylineKm,
			IHInvisibleHandSpec::CoastPolylineSmoothIterations,
			IHInvisibleHandSpec::CoastPolylineSmoothCutRatio);
	}
	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
}

namespace
{
	static float SignedTriangleArea2D(
		const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	}

	static bool IsConvexEarVertex(
		const TArray<FVector2D>& Poly,
		const TArray<int32>& Indices,
		const int32 EarSlot)
	{
		const int32 Count = Indices.Num();
		if (Count < 3)
		{
			return false;
		}

		const int32 PrevSlot = (EarSlot + Count - 1) % Count;
		const int32 NextSlot = (EarSlot + 1) % Count;
		const FVector2D& A = Poly[Indices[PrevSlot]];
		const FVector2D& B = Poly[Indices[EarSlot]];
		const FVector2D& C = Poly[Indices[NextSlot]];
		if (SignedTriangleArea2D(A, B, C) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			if (Slot == PrevSlot || Slot == EarSlot || Slot == NextSlot)
			{
				continue;
			}
			const FVector2D& P = Poly[Indices[Slot]];
			const float AreaABP = SignedTriangleArea2D(A, B, P);
			const float AreaBCP = SignedTriangleArea2D(B, C, P);
			const float AreaCAP = SignedTriangleArea2D(C, A, P);
			const bool bInside = AreaABP >= -KINDA_SMALL_NUMBER
				&& AreaBCP >= -KINDA_SMALL_NUMBER
				&& AreaCAP >= -KINDA_SMALL_NUMBER;
			if (bInside)
			{
				return false;
			}
		}
		return true;
	}

	static bool FindInteriorPointForFanKm(
		const TArray<FVector2D>& PolyKm,
		FVector2D& OutPointKm)
	{
		if (PolyKm.Num() < 3)
		{
			return false;
		}

		FVector2D Centroid = FVector2D::ZeroVector;
		for (const FVector2D& P : PolyKm)
		{
			Centroid += P;
		}
		Centroid /= static_cast<float>(PolyKm.Num());
		if (FIHCoastPolylineSmoothing::IsPointInsideClosedPolylineKm(Centroid, PolyKm))
		{
			OutPointKm = Centroid;
			return true;
		}

		FVector2D BBoxMin = PolyKm[0];
		FVector2D BBoxMax = PolyKm[0];
		for (const FVector2D& P : PolyKm)
		{
			BBoxMin.X = FMath::Min(BBoxMin.X, P.X);
			BBoxMin.Y = FMath::Min(BBoxMin.Y, P.Y);
			BBoxMax.X = FMath::Max(BBoxMax.X, P.X);
			BBoxMax.Y = FMath::Max(BBoxMax.Y, P.Y);
		}

		constexpr int32 GridSteps = 10;
		for (int32 GridY = 1; GridY < GridSteps; ++GridY)
		{
			for (int32 GridX = 1; GridX < GridSteps; ++GridX)
			{
				const FVector2D Candidate(
					FMath::Lerp(BBoxMin.X, BBoxMax.X, static_cast<float>(GridX) / static_cast<float>(GridSteps)),
					FMath::Lerp(BBoxMin.Y, BBoxMax.Y, static_cast<float>(GridY) / static_cast<float>(GridSteps)));
				if (FIHCoastPolylineSmoothing::IsPointInsideClosedPolylineKm(Candidate, PolyKm))
				{
					OutPointKm = Candidate;
					return true;
				}
			}
		}
		return false;
	}

	static bool FanTriangulateFromInteriorPoint(
		TArray<FVector2D>& InOutPolyKm,
		TArray<int32>& OutTriangleVertexIndices)
	{
		OutTriangleVertexIndices.Reset();
		if (InOutPolyKm.Num() < 3)
		{
			return false;
		}

		FVector2D InteriorPoint;
		if (!FindInteriorPointForFanKm(InOutPolyKm, InteriorPoint))
		{
			return false;
		}

		const int32 BoundaryCount = InOutPolyKm.Num();
		const int32 InteriorIdx = InOutPolyKm.Add(InteriorPoint);
		OutTriangleVertexIndices.Reserve(BoundaryCount * 3);
		for (int32 BoundaryIdx = 0; BoundaryIdx < BoundaryCount; ++BoundaryIdx)
		{
			OutTriangleVertexIndices.Add(InteriorIdx);
			OutTriangleVertexIndices.Add(BoundaryIdx);
			OutTriangleVertexIndices.Add((BoundaryIdx + 1) % BoundaryCount);
		}
		return OutTriangleVertexIndices.Num() >= 3;
	}

	static bool EarClipTriangulate(
		const TArray<FVector2D>& Poly,
		TArray<int32>& OutTriangleVertexIndices)
	{
		OutTriangleVertexIndices.Reset();
		if (Poly.Num() < 3)
		{
			return false;
		}

		TArray<int32> Indices;
		Indices.Reserve(Poly.Num());
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			Indices.Add(i);
		}

		int32 Guard = 0;
		const int32 GuardMax = Poly.Num() * Poly.Num();
		while (Indices.Num() > 3 && Guard++ < GuardMax)
		{
			bool bClipped = false;
			for (int32 Slot = 0; Slot < Indices.Num(); ++Slot)
			{
				if (!IsConvexEarVertex(Poly, Indices, Slot))
				{
					continue;
				}

				const int32 PrevSlot = (Slot + Indices.Num() - 1) % Indices.Num();
				const int32 NextSlot = (Slot + 1) % Indices.Num();
				OutTriangleVertexIndices.Add(Indices[PrevSlot]);
				OutTriangleVertexIndices.Add(Indices[Slot]);
				OutTriangleVertexIndices.Add(Indices[NextSlot]);
				Indices.RemoveAt(Slot, 1, EAllowShrinking::No);
				bClipped = true;
				break;
			}
			if (!bClipped)
			{
				return false;
			}
		}

		if (Indices.Num() == 3)
		{
			OutTriangleVertexIndices.Add(Indices[0]);
			OutTriangleVertexIndices.Add(Indices[1]);
			OutTriangleVertexIndices.Add(Indices[2]);
			return OutTriangleVertexIndices.Num() >= 3;
		}
		return false;
	}

	/** Drop degenerate/collinear verts, then re-run ear-clip on the reduced ring. */
	static bool MapboxEarcutTriangulate(
		const TArray<FVector2D>& Poly,
		TArray<int32>& OutTriangleVertexIndices)
	{
		OutTriangleVertexIndices.Reset();
		if (Poly.Num() < 3)
		{
			return false;
		}

		TArray<int32> ActiveIndices;
		ActiveIndices.Reserve(Poly.Num());
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			ActiveIndices.Add(i);
		}

		const float CollinearEps = 1e-5f;
		bool bRemovedDegenerate = true;
		while (bRemovedDegenerate && ActiveIndices.Num() >= 3)
		{
			bRemovedDegenerate = false;
			for (int32 Slot = 0; Slot < ActiveIndices.Num(); ++Slot)
			{
				const int32 PrevSlot = (Slot + ActiveIndices.Num() - 1) % ActiveIndices.Num();
				const int32 NextSlot = (Slot + 1) % ActiveIndices.Num();
				const float Area = SignedTriangleArea2D(
					Poly[ActiveIndices[PrevSlot]],
					Poly[ActiveIndices[Slot]],
					Poly[ActiveIndices[NextSlot]]);
				if (FMath::Abs(Area) <= CollinearEps)
				{
					ActiveIndices.RemoveAt(Slot, 1, EAllowShrinking::No);
					bRemovedDegenerate = true;
					break;
				}
			}
		}

		if (ActiveIndices.Num() < 3)
		{
			return false;
		}

		TArray<FVector2D> ReducedPoly;
		ReducedPoly.Reserve(ActiveIndices.Num());
		for (const int32 VertexIdx : ActiveIndices)
		{
			ReducedPoly.Add(Poly[VertexIdx]);
		}

		TArray<int32> LocalTris;
		if (!EarClipTriangulate(ReducedPoly, LocalTris))
		{
			return false;
		}

		OutTriangleVertexIndices.Reserve(LocalTris.Num());
		for (const int32 LocalIdx : LocalTris)
		{
			OutTriangleVertexIndices.Add(ActiveIndices[LocalIdx]);
		}
		return OutTriangleVertexIndices.Num() >= 3;
	}

	static bool RobustTriangulatePolygon(
		const TArray<FVector2D>& Poly,
		TArray<int32>& OutTriangleVertexIndices)
	{
		if (EarClipTriangulate(Poly, OutTriangleVertexIndices))
		{
			return true;
		}
		if (MapboxEarcutTriangulate(Poly, OutTriangleVertexIndices))
		{
			return true;
		}

		// A+++ MainCoast rings often self-intersect at 768 pts — retry coarser uniform resamples.
		static const int32 RetryCounts[] = {512, 384, 256, 192, 128};
		for (const int32 TargetCount : RetryCounts)
		{
			if (Poly.Num() <= TargetCount)
			{
				continue;
			}
			TArray<FVector2D> ResampledKm;
			FIHCoastPolylineSmoothing::ResampleClosedPolylineUniformCount(
				Poly, TargetCount, ResampledKm);
			if (ResampledKm.Num() < 3)
			{
				continue;
			}
			if (EarClipTriangulate(ResampledKm, OutTriangleVertexIndices))
			{
				return true;
			}
			if (MapboxEarcutTriangulate(ResampledKm, OutTriangleVertexIndices))
			{
				return true;
			}
		}
		return false;
	}

	static bool SegmentSegmentIntersectProper2D(
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C,
		const FVector2D& D,
		FVector2D& OutIntersection)
	{
		const FVector2D R = B - A;
		const FVector2D S = D - C;
		const float Denom = R.X * S.Y - R.Y * S.X;
		if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector2D CmA = C - A;
		const float T = (CmA.X * S.Y - CmA.Y * S.X) / Denom;
		const float U = (CmA.X * R.Y - CmA.Y * R.X) / Denom;
		constexpr float Eps = 1e-5f;
		if (T <= Eps || T >= 1.f - Eps || U <= Eps || U >= 1.f - Eps)
		{
			return false;
		}

		OutIntersection = A + R * T;
		return true;
	}

	static bool AreClosedPolyEdgesAdjacent(const int32 EdgeA, const int32 EdgeB, const int32 NumPoints)
	{
		if (EdgeA == EdgeB)
		{
			return true;
		}
		const int32 NextA = (EdgeA + 1) % NumPoints;
		const int32 NextB = (EdgeB + 1) % NumPoints;
		return NextA == EdgeB || NextB == EdgeA;
	}

	/** Remove near-180° hairpin vertices (path doubles back on itself). Returns verts removed. */
	static int32 RemoveHairpinBacktrackVerticesKm(
		TArray<FVector2D>& PolyKm,
		const float MaxBacktrackDot)
	{
		RemoveClosingDuplicate(PolyKm);
		if (PolyKm.Num() < 4)
		{
			return 0;
		}

		int32 Removed = 0;
		bool bRemoved = true;
		while (bRemoved && PolyKm.Num() >= 4)
		{
			bRemoved = false;
			const int32 N = PolyKm.Num();
			for (int32 VertexIdx = 0; VertexIdx < N; ++VertexIdx)
			{
				const int32 PrevIdx = (VertexIdx + N - 1) % N;
				const int32 NextIdx = (VertexIdx + 1) % N;
				const FVector2D InEdge = PolyKm[VertexIdx] - PolyKm[PrevIdx];
				const FVector2D OutEdge = PolyKm[NextIdx] - PolyKm[VertexIdx];
				const float InLen = InEdge.Size();
				const float OutLen = OutEdge.Size();
				if (InLen < KINDA_SMALL_NUMBER || OutLen < KINDA_SMALL_NUMBER)
				{
					PolyKm.RemoveAt(VertexIdx, 1, EAllowShrinking::No);
					++Removed;
					bRemoved = true;
					break;
				}
				const float Dot = FVector2D::DotProduct(InEdge / InLen, OutEdge / OutLen);
				if (Dot < MaxBacktrackDot)
				{
					PolyKm.RemoveAt(VertexIdx, 1, EAllowShrinking::No);
					++Removed;
					bRemoved = true;
					break;
				}
			}
		}
		return Removed;
	}

	static void RemoveAcuteBacktrackVerticesKm(TArray<FVector2D>& PolyKm)
	{
		RemoveHairpinBacktrackVerticesKm(PolyKm, -0.55f);
	}

	/**
	 * Collapse short outward bulges from breaker kicks / macro noise on convex coast.
	 * CCW land-inside: exterior is right of chord A->C (Cross2 < 0).
	 */
	static int32 RemoveOutwardBulgeVerticesKm(
		TArray<FVector2D>& PolyKm,
		const float MinBulgeHeightKm,
		const float MaxChordKm)
	{
		RemoveClosingDuplicate(PolyKm);
		if (PolyKm.Num() < 4)
		{
			return 0;
		}

		int32 Removed = 0;
		bool bRemoved = true;
		while (bRemoved && PolyKm.Num() >= 4)
		{
			bRemoved = false;
			const int32 N = PolyKm.Num();
			for (int32 VertexIdx = 0; VertexIdx < N; ++VertexIdx)
			{
				const int32 PrevIdx = (VertexIdx + N - 1) % N;
				const int32 NextIdx = (VertexIdx + 1) % N;
				const FVector2D& A = PolyKm[PrevIdx];
				const FVector2D& B = PolyKm[VertexIdx];
				const FVector2D& C = PolyKm[NextIdx];
				const FVector2D AC = C - A;
				const float ChordLen = AC.Size();
				if (ChordLen < KINDA_SMALL_NUMBER || ChordLen > MaxChordKm)
				{
					continue;
				}

				const float Cross2 = AC.X * (B.Y - A.Y) - AC.Y * (B.X - A.X);
				const float Height = FMath::Abs(Cross2) / ChordLen * 0.5f;
				if (Height < MinBulgeHeightKm || Cross2 > 0.f)
				{
					continue;
				}

				const float PathLen = FVector2D::Distance(A, B) + FVector2D::Distance(B, C);
				const float Excess = PathLen - ChordLen;
				if (Excess <= Height * 2.5f + 0.02f)
				{
					PolyKm.RemoveAt(VertexIdx, 1, EAllowShrinking::No);
					++Removed;
					bRemoved = true;
					break;
				}
			}
		}
		return Removed;
	}

	static bool RemoveOneSelfIntersectionLoopKm(TArray<FVector2D>& PolyKm, const int32 MaxLoopVerts = 48)
	{
		RemoveClosingDuplicate(PolyKm);
		const int32 N = PolyKm.Num();
		if (N < 4)
		{
			return false;
		}

		for (int32 EdgeI = 0; EdgeI < N; ++EdgeI)
		{
			const FVector2D& A = PolyKm[EdgeI];
			const FVector2D& B = PolyKm[(EdgeI + 1) % N];
			for (int32 EdgeJ = EdgeI + 2; EdgeJ < N; ++EdgeJ)
			{
				if (EdgeI == 0 && EdgeJ == N - 1)
				{
					continue;
				}
				if (AreClosedPolyEdgesAdjacent(EdgeI, EdgeJ, N))
				{
					continue;
				}

				const FVector2D& C = PolyKm[EdgeJ];
				const FVector2D& D = PolyKm[(EdgeJ + 1) % N];
				FVector2D Intersection;
				if (!SegmentSegmentIntersectProper2D(A, B, C, D, Intersection))
				{
					continue;
				}

				const int32 Loop1Count = EdgeJ - EdgeI;
				const int32 Loop2Count = N - Loop1Count;
				const int32 SmallLoopVerts = FMath::Min(Loop1Count, Loop2Count);
				if (SmallLoopVerts > MaxLoopVerts)
				{
					continue;
				}

				TArray<FVector2D> Clean;
				Clean.Reserve(N);

				if (Loop1Count <= Loop2Count)
				{
					for (int32 k = 0; k <= EdgeI; ++k)
					{
						Clean.Add(PolyKm[k]);
					}
					Clean.Add(Intersection);
					for (int32 k = EdgeJ + 1; k < N; ++k)
					{
						Clean.Add(PolyKm[k]);
					}
				}
				else
				{
					for (int32 k = EdgeI + 1; k <= EdgeJ; ++k)
					{
						Clean.Add(PolyKm[k]);
					}
					Clean.Add(Intersection);
				}

				if (Clean.Num() < N / 2)
				{
					continue;
				}

				PolyKm = MoveTemp(Clean);
				return true;
			}
		}
		return false;
	}

	static void RemoveSelfIntersectionLoopsKm(
		TArray<FVector2D>& PolyKm,
		const int32 MinVertCount = 3,
		const int32 MaxLoopVerts = 48)
	{
		constexpr int32 MaxPasses = 48;
		constexpr int32 MaxVertCap = 900;
		for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
		{
			const int32 StartCount = PolyKm.Num();
			TArray<FVector2D> Backup = PolyKm;
			if (!RemoveOneSelfIntersectionLoopKm(PolyKm, MaxLoopVerts))
			{
				break;
			}
			if (PolyKm.Num() > StartCount || PolyKm.Num() > MaxVertCap || PolyKm.Num() < MinVertCount)
			{
				PolyKm = MoveTemp(Backup);
				return;
			}
		}
	}

	static void ReverseClosedPolylineKm(TArray<FVector2D>& PolylineKm)
	{
		RemoveClosingDuplicate(PolylineKm);
		Algo::Reverse(PolylineKm);
	}

	static void SanitizeCoastPolylineHairpinsKm(
		TArray<FVector2D>& PolyKm,
		const int32 MinVertFloor)
	{
		RemoveOutwardBulgeVerticesKm(
			PolyKm,
			IHInvisibleHandSpec::CoastSpurMinBulgeHeightKm,
			IHInvisibleHandSpec::CoastSpurMaxChordKm);
		RemoveHairpinBacktrackVerticesKm(PolyKm, IHInvisibleHandSpec::CoastHairpinBacktrackDot);
		RemoveSelfIntersectionLoopsKm(
			PolyKm,
			MinVertFloor,
			IHInvisibleHandSpec::CoastSanitizeMaxLoopVerts);
		RemoveHairpinBacktrackVerticesKm(PolyKm, IHInvisibleHandSpec::CoastHairpinBacktrackDot);
		FIHCoastPolylineSmoothing::SanitizeClosedPolylineForTriangulationKm(PolyKm);
	}

	/** C1a Exp3: lighter hairpin sanitize for MainCoast authority bake only. */
	static void SanitizeMainCoastAuthorityPolylineHairpinsKm(
		TArray<FVector2D>& PolyKm,
		const int32 MinVertFloor)
	{
		if (!IHInvisibleHandSpec::bCoastAuthoritySkipOutwardBulgeStrip)
		{
			RemoveOutwardBulgeVerticesKm(
				PolyKm,
				IHInvisibleHandSpec::CoastSpurMinBulgeHeightKm,
				IHInvisibleHandSpec::CoastSpurMaxChordKm);
		}

		const float HairpinDot = IHInvisibleHandSpec::CoastAuthorityHairpinBacktrackDot;
		RemoveHairpinBacktrackVerticesKm(PolyKm, HairpinDot);
		RemoveSelfIntersectionLoopsKm(
			PolyKm,
			MinVertFloor,
			IHInvisibleHandSpec::CoastSanitizeMaxLoopVerts);
		RemoveHairpinBacktrackVerticesKm(PolyKm, HairpinDot);
		FIHCoastPolylineSmoothing::SanitizeClosedPolylineForTriangulationKm(PolyKm);
	}

	static void ResampleAuthorityPolylineToCapKm(TArray<FVector2D>& InOutPolylineKm, const int32 MaxVerts)
	{
		if (MaxVerts <= 0 || InOutPolylineKm.Num() < 3)
		{
			return;
		}

		const bool bBelowCap = InOutPolylineKm.Num() < MaxVerts;
		const bool bAboveCap = InOutPolylineKm.Num() > MaxVerts;
		if (!bAboveCap && !(bBelowCap && IHInvisibleHandSpec::bCoastAuthorityResampleFillToCap))
		{
			return;
		}

		if (InOutPolylineKm.Num() == MaxVerts)
		{
			return;
		}

		TArray<FVector2D> ResampledKm;
		FIHCoastPolylineSmoothing::ResampleClosedPolylineUniformCount(
			InOutPolylineKm, MaxVerts, ResampledKm);
		InOutPolylineKm = MoveTemp(ResampledKm);
	}

	static bool MergeHoleIntoOuterPolygonKm(
		TArray<FVector2D>& OuterKm,
		const TArray<FVector2D>& HoleKm)
	{
		if (OuterKm.Num() < 3 || HoleKm.Num() < 3)
		{
			return false;
		}

		TArray<FVector2D> HoleCw = HoleKm;
		ReverseClosedPolylineKm(HoleCw);

		int32 BestOuterIdx = INDEX_NONE;
		int32 BestHoleIdx = INDEX_NONE;
		float BestDistSq = TNumericLimits<float>::Max();
		for (int32 OuterIdx = 0; OuterIdx < OuterKm.Num(); ++OuterIdx)
		{
			const FVector2D& OuterPt = OuterKm[OuterIdx];
			for (int32 HoleIdx = 0; HoleIdx < HoleCw.Num(); ++HoleIdx)
			{
				const float DistSq = FVector2D::DistSquared(OuterPt, HoleCw[HoleIdx]);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestOuterIdx = OuterIdx;
					BestHoleIdx = HoleIdx;
				}
			}
		}
		if (BestOuterIdx == INDEX_NONE || BestHoleIdx == INDEX_NONE)
		{
			return false;
		}

		TArray<FVector2D> Merged;
		Merged.Reserve(OuterKm.Num() + HoleCw.Num() + 4);

		for (int32 i = 0; i <= BestOuterIdx; ++i)
		{
			Merged.Add(OuterKm[i]);
		}
		for (int32 i = 0; i < HoleCw.Num(); ++i)
		{
			const int32 HoleVertexIdx = (BestHoleIdx + i) % HoleCw.Num();
			Merged.Add(HoleCw[HoleVertexIdx]);
		}
		Merged.Add(HoleCw[BestHoleIdx]);
		Merged.Add(OuterKm[BestOuterIdx]);
		for (int32 i = BestOuterIdx + 1; i < OuterKm.Num(); ++i)
		{
			Merged.Add(OuterKm[i]);
		}

		OuterKm = MoveTemp(Merged);
		return OuterKm.Num() >= 3;
	}
}

void FIHCoastPolylineSmoothing::SanitizeClosedPolylineForTriangulationKm(TArray<FVector2D>& PolyKm)
{
	if (PolyKm.Num() >= 2 && PolyKm[0].Equals(PolyKm.Last(), KINDA_SMALL_NUMBER))
	{
		PolyKm.RemoveAt(PolyKm.Num() - 1, 1, EAllowShrinking::No);
	}
	if (PolyKm.Num() < 3)
	{
		return;
	}

	const float MinEdgeKm = 1e-7f;
	const float CollinearEps = 1e-4f;

	TArray<FVector2D> Clean;
	Clean.Reserve(PolyKm.Num());
	for (const FVector2D& Point : PolyKm)
	{
		if (Clean.Num() > 0 && FVector2D::Distance(Clean.Last(), Point) < MinEdgeKm)
		{
			continue;
		}
		Clean.Add(Point);
	}
	if (Clean.Num() >= 2 && FVector2D::Distance(Clean[0], Clean.Last()) < MinEdgeKm)
	{
		Clean.RemoveAt(Clean.Num() - 1, 1, EAllowShrinking::No);
	}
	PolyKm = MoveTemp(Clean);
	if (PolyKm.Num() < 3)
	{
		return;
	}

	bool bRemovedCollinear = true;
	while (bRemovedCollinear && PolyKm.Num() >= 4)
	{
		bRemovedCollinear = false;
		const int32 VertexCount = PolyKm.Num();
		for (int32 VertexIdx = 0; VertexIdx < VertexCount; ++VertexIdx)
		{
			const int32 PrevIdx = (VertexIdx + VertexCount - 1) % VertexCount;
			const int32 NextIdx = (VertexIdx + 1) % VertexCount;
			const FVector2D& A = PolyKm[PrevIdx];
			const FVector2D& B = PolyKm[VertexIdx];
			const FVector2D& C = PolyKm[NextIdx];
			const float Cross = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
			if (FMath::Abs(Cross) <= CollinearEps)
			{
				PolyKm.RemoveAt(VertexIdx, 1, EAllowShrinking::No);
				bRemovedCollinear = true;
				break;
			}
		}
	}
}

bool FIHCoastPolylineSmoothing::TriangulateSimplePolygonCCW(
	const TArray<FVector2D>& PolygonKm,
	TArray<int32>& OutTriangleVertexIndices)
{
	if (PolygonKm.Num() < 3)
	{
		return false;
	}
	TArray<FVector2D> Work = PolygonKm;
	SanitizeClosedPolylineForTriangulationKm(Work);
	if (EnsureCounterClockwiseClosedPolylineKm(Work) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	if (RobustTriangulatePolygon(Work, OutTriangleVertexIndices))
	{
		return true;
	}

	return FanTriangulateFromInteriorPoint(Work, OutTriangleVertexIndices);
}

bool FIHCoastPolylineSmoothing::TriangulatePolygonWithHoles(
	const TArray<FVector2D>& OuterKm,
	const TArray<TArray<FVector2D>>& HolesKm,
	TArray<int32>& OutTriangleVertexIndices,
	TArray<FVector2D>& OutVerticesKm)
{
	OutTriangleVertexIndices.Reset();
	OutVerticesKm = OuterKm;
	SanitizeClosedPolylineForTriangulationKm(OutVerticesKm);
	if (OutVerticesKm.Num() < 3)
	{
		return false;
	}
	EnsureCounterClockwiseClosedPolylineKm(OutVerticesKm);

	TArray<TArray<FVector2D>> RemainingHoles;
	for (TArray<FVector2D> Hole : HolesKm)
	{
		SanitizeClosedPolylineForTriangulationKm(Hole);
		if (Hole.Num() >= 3)
		{
			RemainingHoles.Add(MoveTemp(Hole));
		}
	}

	while (RemainingHoles.Num() > 0)
	{
		const TArray<FVector2D> NextHole = RemainingHoles.Pop(EAllowShrinking::No);
		if (!MergeHoleIntoOuterPolygonKm(OutVerticesKm, NextHole))
		{
			return false;
		}
	}

	if (RobustTriangulatePolygon(OutVerticesKm, OutTriangleVertexIndices))
	{
		return true;
	}

	// B2c: fan triangulation draws visible radial spokes on land — fail closed; caller retries decimated / fine-grid.
	if (IHInvisibleHandSpec::IsCoastPhaseB2SlopeTierShelfActive())
	{
		return false;
	}

	return FanTriangulateFromInteriorPoint(OutVerticesKm, OutTriangleVertexIndices);
}

void FIHCoastPolylineSmoothing::LightRefineEnclosedLakePolylineKm(
	TArray<FVector2D>& InOutPolylineKm,
	const int32 MasterSeed,
	const int32 IslandIndex,
	const int32 PolySalt)
{
	if (InOutPolylineKm.Num() < 3)
	{
		return;
	}

	const float AmpScale = 0.35f;
	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
	DensifyClosedPolylineKm(InOutPolylineKm, IHInvisibleHandSpec::CoastLakeSmoothDensifySpacingKm);
	BreakLongStraightSegmentsKm(
		InOutPolylineKm,
		IHInvisibleHandSpec::CoastMaxStraightSegmentKm * 0.65f,
		MasterSeed,
		IslandIndex,
		PolySalt,
		false,
		AmpScale);
	SmoothClosedPolylineKm(
		InOutPolylineKm,
		IHInvisibleHandSpec::CoastLakeSmoothIterations,
		IHInvisibleHandSpec::CoastPolylineSmoothCutRatio);
	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
}

namespace
{
	static float ComputeClosedPolylinePerimeterKm(const TArray<FVector2D>& PolyKm)
	{
		TArray<FVector2D> Work = PolyKm;
		RemoveClosingDuplicate(Work);
		const int32 N = Work.Num();
		if (N < 2)
		{
			return 0.f;
		}
		float Total = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			Total += FVector2D::Distance(Work[i], Work[(i + 1) % N]);
		}
		return Total;
	}

	static float ComputeClosedPolylineMaxEdgeKm(const TArray<FVector2D>& PolyKm)
	{
		const int32 N = PolyKm.Num();
		if (N < 2)
		{
			return 0.f;
		}
		float MaxEdge = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			MaxEdge = FMath::Max(
				MaxEdge, FVector2D::Distance(PolyKm[i], PolyKm[(i + 1) % N]));
		}
		return MaxEdge;
	}

	static void SubdivideLongEdgesClosedPolylineKm(
		TArray<FVector2D>& PolyKm,
		const float MaxEdgeLenKm)
	{
		RemoveClosingDuplicate(PolyKm);
		if (PolyKm.Num() < 3 || MaxEdgeLenKm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		constexpr int32 MaxIterations = 12;
		for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
		{
			bool bSplit = false;
			const int32 N = PolyKm.Num();
			TArray<FVector2D> Expanded;
			Expanded.Reserve(N * 2);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& A = PolyKm[i];
				const FVector2D& B = PolyKm[(i + 1) % N];
				Expanded.Add(A);
				const float Len = FVector2D::Distance(A, B);
				if (Len > MaxEdgeLenKm)
				{
					const int32 NumSegments = FMath::Max(2, FMath::CeilToInt(Len / MaxEdgeLenKm));
					for (int32 SegmentIdx = 1; SegmentIdx < NumSegments; ++SegmentIdx)
					{
						const float T = static_cast<float>(SegmentIdx) / static_cast<float>(NumSegments);
						Expanded.Add(FMath::Lerp(A, B, T));
					}
					bSplit = true;
				}
			}
			PolyKm = MoveTemp(Expanded);
			if (!bSplit || PolyKm.Num() > 8192)
			{
				break;
			}
		}
	}
}

void FIHCoastPolylineSmoothing::PrepareMainCoastAuthorityPolylineKm(
	const TArray<FVector2D>& SourceKm,
	const int32 MaxVerts,
	const int32 MasterSeed,
	const int32 IslandIndex,
	const int32 PolySalt,
	const bool bLandInsidePoly,
	const float NoiseAmpScale,
	TArray<FVector2D>& OutAuthorityKm)
{
	OutAuthorityKm = SourceKm;
	if (OutAuthorityKm.Num() < 3)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const int32 InCount = OutAuthorityKm.Num();
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT(
			"PrepareMainCoastAuthority config island=%d smoothIter=%d secondSanitize=%d hairpinDot=%.2f skipBulgeStrip=%d resampleFill=%d"),
		IslandIndex,
		IHInvisibleHandSpec::CoastAuthoritySmoothIterations,
		IHInvisibleHandSpec::IsCoastAuthoritySecondHairpinSanitizeEnabled() ? 1 : 0,
		IHInvisibleHandSpec::CoastAuthorityHairpinBacktrackDot,
		IHInvisibleHandSpec::bCoastAuthoritySkipOutwardBulgeStrip ? 1 : 0,
		IHInvisibleHandSpec::bCoastAuthorityResampleFillToCap ? 1 : 0);
	int32 PostPreResampleCount = InCount;
	int32 PostBreakerCount = InCount;
	int32 PostMacroCount = InCount;
	int32 PostSmoothCount = InCount;
	int32 PostSanitize1Count = InCount;
	int32 Sanitize1Removed = 0;
	int32 PostResample1Count = InCount;
	int32 PostSubdivideCount = InCount;
	int32 PostResample2Count = InCount;
	int32 Sanitize2Removed = 0;
#endif

	EnsureCounterClockwiseClosedPolylineKm(OutAuthorityKm);

	if (MaxVerts > 0 && OutAuthorityKm.Num() > MaxVerts)
	{
		TArray<FVector2D> ResampledKm;
		ResampleClosedPolylineUniformCount(OutAuthorityKm, MaxVerts, ResampledKm);
		OutAuthorityKm = MoveTemp(ResampledKm);
	}

#if !UE_BUILD_SHIPPING
	PostPreResampleCount = OutAuthorityKm.Num();
#endif

	if (IHInvisibleHandSpec::bCoastC1f_AuthorityPostResampleC1dEnabled
		&& IHInvisibleHandSpec::IsCoastC1dGridArtifactRemedyEnabled())
	{
		const float C1dAmpScale = FMath::Max(NoiseAmpScale, 0.1f)
			* IHInvisibleHandSpec::CoastAuthorityMacroNoiseAmpScale;
		ApplyCoastC1dGridArtifactRemedyKm(
			OutAuthorityKm, MasterSeed, IslandIndex, PolySalt, bLandInsidePoly, C1dAmpScale);
	}

	const float AuthorityMacroScale = FMath::Max(NoiseAmpScale, 0.1f)
		* IHInvisibleHandSpec::CoastAuthorityMacroNoiseAmpScale;
	if (IHInvisibleHandSpec::bCoastAuthorityStraightBreakerEnabled)
	{
		BreakLongStraightSegmentsKm(
			OutAuthorityKm,
			IHInvisibleHandSpec::CoastAuthorityMaxStraightSegmentKm,
			MasterSeed,
			IslandIndex,
			PolySalt,
			bLandInsidePoly,
			AuthorityMacroScale,
			true);
	}

#if !UE_BUILD_SHIPPING
	PostBreakerCount = OutAuthorityKm.Num();
#endif

	if (IHInvisibleHandSpec::bCoastAuthorityMacroNoiseEnabled)
	{
		ApplyCoastEdgeNoiseKm(
			OutAuthorityKm,
			MasterSeed,
			IslandIndex,
			PolySalt,
			bLandInsidePoly,
			AuthorityMacroScale,
			true);
	}

#if !UE_BUILD_SHIPPING
	PostMacroCount = OutAuthorityKm.Num();
#endif

	if (IHInvisibleHandSpec::CoastAuthoritySmoothIterations > 0)
	{
		SmoothClosedPolylineKm(
			OutAuthorityKm,
			IHInvisibleHandSpec::CoastAuthoritySmoothIterations,
			IHInvisibleHandSpec::CoastPolylineSmoothCutRatio);
	}

#if !UE_BUILD_SHIPPING
	PostSmoothCount = OutAuthorityKm.Num();
#endif

	const int32 MinFloor = FMath::Max(64, MaxVerts > 0 ? MaxVerts / 4 : 64);
	const int32 BeforeSanitize = OutAuthorityKm.Num();
	SanitizeMainCoastAuthorityPolylineHairpinsKm(OutAuthorityKm, MinFloor);

#if !UE_BUILD_SHIPPING
	Sanitize1Removed = BeforeSanitize - OutAuthorityKm.Num();
	PostSanitize1Count = OutAuthorityKm.Num();
#endif

	ResampleAuthorityPolylineToCapKm(OutAuthorityKm, MaxVerts);

#if !UE_BUILD_SHIPPING
	PostResample1Count = OutAuthorityKm.Num();
#endif

	const float PerimeterKm = ComputeClosedPolylinePerimeterKm(OutAuthorityKm);
	const float MeanEdgeKm = PerimeterKm / FMath::Max(OutAuthorityKm.Num(), 3);
	const float MaxAllowedEdgeKm = FMath::Max(MeanEdgeKm * 3.f, 0.03f);
	SubdivideLongEdgesClosedPolylineKm(OutAuthorityKm, MaxAllowedEdgeKm);

#if !UE_BUILD_SHIPPING
	PostSubdivideCount = OutAuthorityKm.Num();
#endif

	ResampleAuthorityPolylineToCapKm(OutAuthorityKm, MaxVerts);

#if !UE_BUILD_SHIPPING
	PostResample2Count = OutAuthorityKm.Num();
#endif

	const int32 BeforeSanitize2 = OutAuthorityKm.Num();
	if (IHInvisibleHandSpec::IsCoastAuthoritySecondHairpinSanitizeEnabled())
	{
		SanitizeCoastPolylineHairpinsKm(OutAuthorityKm, MinFloor);
	}
	EnsureCounterClockwiseClosedPolylineKm(OutAuthorityKm);

	if (IHInvisibleHandSpec::bCoastC1f_AuthorityFinalC1dEnabled
		&& IHInvisibleHandSpec::IsCoastC1dGridArtifactRemedyEnabled())
	{
		const float C1dAmpScale = FMath::Max(NoiseAmpScale, 0.1f)
			* IHInvisibleHandSpec::CoastAuthorityMacroNoiseAmpScale;
		ApplyCoastC1dGridArtifactRemedyKm(
			OutAuthorityKm,
			MasterSeed,
			IslandIndex,
			PolySalt + 17,
			bLandInsidePoly,
			C1dAmpScale);
		EnsureCounterClockwiseClosedPolylineKm(OutAuthorityKm);
	}

#if !UE_BUILD_SHIPPING
	Sanitize2Removed = BeforeSanitize2 - OutAuthorityKm.Num();
	const float MaxEdgeM = ComputeClosedPolylineMaxEdgeKm(OutAuthorityKm) * 1000.f;
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT(
			"PrepareMainCoastAuthority island=%d cap=%d in=%d preResample=%d postBreaker=%d postMacro=%d postSmooth=%d "
			"sanitize1Removed=%d postSanitize1=%d postResample1=%d postSubdivide=%d postResample2=%d "
			"sanitize2Removed=%d out=%d maxEdge=%.0fm"),
		IslandIndex,
		MaxVerts,
		InCount,
		PostPreResampleCount,
		PostBreakerCount,
		PostMacroCount,
		PostSmoothCount,
		Sanitize1Removed,
		PostSanitize1Count,
		PostResample1Count,
		PostSubdivideCount,
		PostResample2Count,
		Sanitize2Removed,
		OutAuthorityKm.Num(),
		MaxEdgeM);
#endif
}

void FIHCoastPolylineSmoothing::PrepareMainCoastAuthorityPolylineLocalCm(
	const TArray<FVector2D>& SourceLocalCm,
	const int32 MaxVerts,
	const int32 MasterSeed,
	const int32 IslandIndex,
	const int32 PolySalt,
	const bool bLandInsidePoly,
	const float NoiseAmpScale,
	TArray<FVector2D>& OutAuthorityLocalCm)
{
	TArray<FVector2D> SourceKm;
	SourceKm.Reserve(SourceLocalCm.Num());
	for (const FVector2D& P : SourceLocalCm)
	{
		SourceKm.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
	}

	TArray<FVector2D> AuthorityKm;
	PrepareMainCoastAuthorityPolylineKm(
		SourceKm,
		MaxVerts,
		MasterSeed,
		IslandIndex,
		PolySalt,
		bLandInsidePoly,
		NoiseAmpScale,
		AuthorityKm);

	OutAuthorityLocalCm.Reset();
	OutAuthorityLocalCm.Reserve(AuthorityKm.Num());
	for (const FVector2D& P : AuthorityKm)
	{
		OutAuthorityLocalCm.Add(FVector2D(P.X * 100000.f, P.Y * 100000.f));
	}
}

void FIHCoastPolylineSmoothing::SanitizeClosedPolylineForCoastStrokeKm(
	TArray<FVector2D>& InOutPolylineKm)
{
	if (InOutPolylineKm.Num() < 3)
	{
		return;
	}

	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
	const int32 MinFloor = FMath::Max(64, IHInvisibleHandSpec::CoastRenderPolyVerts / 4);
	SanitizeCoastPolylineHairpinsKm(InOutPolylineKm, MinFloor);
	EnsureCounterClockwiseClosedPolylineKm(InOutPolylineKm);
}

void FIHCoastPolylineSmoothing::PrepareClosedPolylineForCoastStrokeKm(
	const TArray<FVector2D>& SourceKm,
	const int32 MaxVerts,
	TArray<FVector2D>& OutStrokeKm)
{
	const int32 InCount = SourceKm.Num();
	OutStrokeKm = SourceKm;
	if (OutStrokeKm.Num() < 3)
	{
		return;
	}

	TArray<FVector2D> ResampledKm;
	if (MaxVerts > 0 && OutStrokeKm.Num() > MaxVerts)
	{
		ResampleClosedPolylineUniformCount(OutStrokeKm, MaxVerts, ResampledKm);
		OutStrokeKm = ResampledKm;
	}
	else
	{
		ResampledKm = OutStrokeKm;
	}

	const int32 MinFloor = FMath::Max(64, MaxVerts > 0 ? MaxVerts / 4 : 64);
	const int32 BeforeSanitize = OutStrokeKm.Num();
	SanitizeCoastPolylineHairpinsKm(OutStrokeKm, MinFloor);
	const int32 SanitizeRemoved = BeforeSanitize - OutStrokeKm.Num();

	if (OutStrokeKm.Num() < 3)
	{
		OutStrokeKm = ResampledKm;
	}

	EnsureCounterClockwiseClosedPolylineKm(OutStrokeKm);

#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("PrepareClosedPolylineForCoastStroke: in=%d resampled=%d sanitizeRemoved=%d out=%d"),
		InCount,
		ResampledKm.Num(),
		SanitizeRemoved,
		OutStrokeKm.Num());
#endif
}

void FIHCoastPolylineSmoothing::PrepareClosedPolylineForCoastStrokeLocalCm(
	const TArray<FVector2D>& SourceLocalCm,
	const int32 MaxVerts,
	TArray<FVector2D>& OutStrokeLocalCm)
{
	TArray<FVector2D> SourceKm;
	SourceKm.Reserve(SourceLocalCm.Num());
	for (const FVector2D& P : SourceLocalCm)
	{
		SourceKm.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
	}

	TArray<FVector2D> StrokeKm;
	PrepareClosedPolylineForCoastStrokeKm(SourceKm, MaxVerts, StrokeKm);

	OutStrokeLocalCm.Reset();
	OutStrokeLocalCm.Reserve(StrokeKm.Num());
	for (const FVector2D& P : StrokeKm)
	{
		OutStrokeLocalCm.Add(FVector2D(P.X * 100000.f, P.Y * 100000.f));
	}
}

void FIHCoastPolylineSmoothing::ResolveCoastStrokePolylineLocalCm(
	const TArray<FVector2D>& SourceLocalCm,
	TArray<FVector2D>& OutStrokeLocalCm)
{
	OutStrokeLocalCm.Reset();
	if (SourceLocalCm.Num() < 3)
	{
		return;
	}

	TArray<FVector2D> StrokeKm;
	StrokeKm.Reserve(SourceLocalCm.Num());
	for (const FVector2D& P : SourceLocalCm)
	{
		StrokeKm.Add(FVector2D(P.X / 100.f, P.Y / 100.f));
	}

	if (IHInvisibleHandSpec::IsCoastAuthorityBakeAtExtractActive()
		&& StrokeKm.Num() <= IHInvisibleHandSpec::CoastRenderPolyVerts)
	{
		SanitizeClosedPolylineForCoastStrokeKm(StrokeKm);
	}
	else
	{
		PrepareClosedPolylineForCoastStrokeKm(
			StrokeKm, IHInvisibleHandSpec::CoastRenderPolyVerts, StrokeKm);
	}

	OutStrokeLocalCm.Reserve(StrokeKm.Num());
	for (const FVector2D& P : StrokeKm)
	{
		OutStrokeLocalCm.Add(FVector2D(P.X * 100.f, P.Y * 100.f));
	}
}
