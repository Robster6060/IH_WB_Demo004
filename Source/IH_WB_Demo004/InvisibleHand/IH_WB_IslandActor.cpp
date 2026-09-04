// Copyright Invisible Hand. All Rights Reserved.

#include "IH_WB_IslandActor.h"

#include "IHTerrainCellGraphGenerator.h"
#include "IHTerrainCellDiffusion.h"
#include "IHSectorFabric.h"
#include "IHCoastGenerationTypes.h"
#include "IHCoastPolylineSmoothing.h"
#include "IHMapSeedFrameworkTypes.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "IH_P1C08_MinimapSubsystem.h"
#include "IH_P1C08_IslandNavSubsystem.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IHDevViewRuntime.h"
#include "IH_ASLSlopeBiomeRow.h"
#include "IH_WorldBuilderDataSubsystem.h"
#include "Components/ArrowComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"

bool AIH_WB_IslandActor::bAslContourRibbonBakeDeferred = false;

namespace IH_WB_IslandActorPrivate
{
	/** IH-DEC-058, disabled 2026-09-03 (kept as 1.0, not deleted, so the mechanism/seam-consistency
	 * wiring stays intact if a future pass wants it back). Gamma<1 lifts low/mid elevations -
	 * tuned to 0.6 back when the (buggy, since-fixed) apex-height formula capped summits at
	 * 180-620m, where the compression was mild in absolute terms. Once the real apex formula went
	 * live the same session (up to a 2400m ceiling), the identical curve became severe: a cell
	 * barely above sea level (LinearNormalizedHeight=0.05) rendered at Pow(0.05,0.6)=~17% of a
	 * 2400m summit - ~400m of displayed elevation for what should read as flat beach. Real PIE
	 * evidence: no flat beachfront/coastal-plain terrain anywhere, island interiors "ubiquitously
	 * rugged" - not acceptable for gameplay. The curve's original justification (islands reading
	 * flat/uniform under the old low-capped formula) is now solved more directly by the apex-
	 * height bug fix itself (real per-island height variety from real diameter-driven summits) -
	 * gamma reshaping on top of that is no longer needed and actively harmful at this scale.
	 * Single source of truth for both IslandMesh's own vertex-Z formula (BuildMeshesFromCellGraph)
	 * and BuildWwfShelfSection's coastal seam-matching formula below — those two drifted out of
	 * sync once before (IslandMesh gained this gamma curve, ShelfMesh's match formula didn't),
	 * silently reopening the IslandMesh/ShelfMesh seam gap `54dfa15` had fixed. Change this value
	 * here only; never duplicate it as a second local constant. */
	static constexpr double IslandHeightReshapeGamma = 1.0;

	static int64 AcresFromAreaKm2(float InAreaKm2)
	{
		const double M2 = static_cast<double>(InAreaKm2) * 1.0e6;
		return FMath::Max<int64>(1, FMath::RoundToInt64(M2 / 4046.8564224));
	}

	/** Proper segment–segment intersection (closed-poly edges; skips touching endpoints). */
	static bool SegmentsIntersectProper2D(
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C,
		const FVector2D& D)
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
		return T > Eps && T < 1.f - Eps && U > Eps && U < 1.f - Eps;
	}

	static bool AreClosedEdgesAdjacent(const int32 EdgeA, const int32 EdgeB, const int32 N)
	{
		if (EdgeA == EdgeB)
		{
			return true;
		}
		return ((EdgeA + 1) % N) == EdgeB || ((EdgeB + 1) % N) == EdgeA;
	}

	/** Count proper self-crossing edge pairs on a closed polyline (km or any consistent XY). */
	static int32 CountClosedPolylineSelfCrossings(const TArray<FVector2D>& Poly)
	{
		const int32 N = Poly.Num();
		if (N < 4)
		{
			return 0;
		}
		int32 Crossings = 0;
		for (int32 EdgeI = 0; EdgeI < N; ++EdgeI)
		{
			const FVector2D& A = Poly[EdgeI];
			const FVector2D& B = Poly[(EdgeI + 1) % N];
			for (int32 EdgeJ = EdgeI + 2; EdgeJ < N; ++EdgeJ)
			{
				if (EdgeI == 0 && EdgeJ == N - 1)
				{
					continue;
				}
				if (AreClosedEdgesAdjacent(EdgeI, EdgeJ, N))
				{
					continue;
				}
				if (SegmentsIntersectProper2D(A, B, Poly[EdgeJ], Poly[(EdgeJ + 1) % N]))
				{
					++Crossings;
				}
			}
		}
		return Crossings;
	}

	/**
	 * Index-locked DeepOuter: Coast + Outward * Disp (no miter). Residual firth cleanup
	 * shrinks Disp only — never deletes verts (keeps 1:1 Coast↔Deep for cyan loft).
	 */
	static void BuildNormalPushDeepOuterKm(
		const TArray<FVector2D>& CoastKm,
		const TArray<FVector2D>& OutwardNormalsUnit,
		TArray<float>& InOutDispKm,
		TArray<FVector2D>& OutDeepKm,
		int32& OutSelfCrossAfter)
	{
		OutDeepKm.Reset();
		OutSelfCrossAfter = 0;
		const int32 N = CoastKm.Num();
		if (N < 3 || OutwardNormalsUnit.Num() != N || InOutDispKm.Num() != N)
		{
			OutDeepKm = CoastKm;
			return;
		}

		const float SheerKm = IHInvisibleHandSpec::SeaRootsDispSheerMeters / 1000.f;
		const float BeachKm = IHInvisibleHandSpec::SeaRootsDispBeachMeters / 1000.f;
		// Firth cleanup may go below Sheer so opposite walls do not keep crossing at 100 m.
		constexpr float MinCleanupKm = 0.010f; // 10 m
		constexpr int32 ShrinkPasses = 24;
		constexpr float ShrinkFactor = 0.60f;

		auto RebuildDeep = [&]()
		{
			OutDeepKm.SetNum(N);
			for (int32 i = 0; i < N; ++i)
			{
				InOutDispKm[i] = FMath::Clamp(InOutDispKm[i], MinCleanupKm, BeachKm);
				OutDeepKm[i] = CoastKm[i] + OutwardNormalsUnit[i] * InOutDispKm[i];
			}
		};

		RebuildDeep();

		// Cap Disp by clearance to opposite shore (firth walls face each other along Outward).
		{
			bool bClamped = false;
			for (int32 i = 0; i < N; ++i)
			{
				float MaxClearKm = BeachKm;
				for (int32 j = 0; j < N; ++j)
				{
					const int32 DistIdx = FMath::Min((j - i + N) % N, (i - j + N) % N);
					if (DistIdx <= 2)
					{
						continue;
					}
					const FVector2D Delta = CoastKm[j] - CoastKm[i];
					const float DistKm = Delta.Size();
					if (DistKm < 1e-5f)
					{
						continue;
					}
					const float TowardOut = FVector2D::DotProduct(Delta / DistKm, OutwardNormalsUnit[i]);
					if (TowardOut > 0.25f)
					{
						MaxClearKm = FMath::Min(MaxClearKm, DistKm * 0.40f);
					}
				}
				MaxClearKm = FMath::Clamp(MaxClearKm, MinCleanupKm, BeachKm);
				if (InOutDispKm[i] > MaxClearKm + 1e-6f)
				{
					InOutDispKm[i] = MaxClearKm;
					bClamped = true;
				}
			}
			if (bClamped)
			{
				RebuildDeep();
			}
		}

		for (int32 Pass = 0; Pass < ShrinkPasses; ++Pass)
		{
			bool bShrunk = false;
			const int32 CrossBefore = CountClosedPolylineSelfCrossings(OutDeepKm);
			if (CrossBefore == 0)
			{
				break;
			}

			// Deep verts that land inside the coast poly (offset folded inland on concave arcs).
			for (int32 i = 0; i < N; ++i)
			{
				if (FIHCoastPolylineSmoothing::IsPointInsideClosedPolylineKm(OutDeepKm[i], CoastKm))
				{
					const float NextDisp = FMath::Max(InOutDispKm[i] * ShrinkFactor, MinCleanupKm);
					if (NextDisp + 1e-6f < InOutDispKm[i])
					{
						InOutDispKm[i] = NextDisp;
						bShrunk = true;
					}
				}
			}

			// Vertices on self-crossing Deep edges — shrink both endpoints (+1 neighbor).
			for (int32 EdgeI = 0; EdgeI < N; ++EdgeI)
			{
				const FVector2D& A = OutDeepKm[EdgeI];
				const FVector2D& B = OutDeepKm[(EdgeI + 1) % N];
				for (int32 EdgeJ = EdgeI + 2; EdgeJ < N; ++EdgeJ)
				{
					if (EdgeI == 0 && EdgeJ == N - 1)
					{
						continue;
					}
					if (AreClosedEdgesAdjacent(EdgeI, EdgeJ, N))
					{
						continue;
					}
					if (!SegmentsIntersectProper2D(A, B, OutDeepKm[EdgeJ], OutDeepKm[(EdgeJ + 1) % N]))
					{
						continue;
					}
					const int32 Verts[6] = {
						EdgeI, (EdgeI + 1) % N, (EdgeI + N - 1) % N,
						EdgeJ, (EdgeJ + 1) % N, (EdgeJ + N - 1) % N
					};
					for (const int32 Vi : Verts)
					{
						const float NextDisp = FMath::Max(InOutDispKm[Vi] * ShrinkFactor, MinCleanupKm);
						if (NextDisp + 1e-6f < InOutDispKm[Vi])
						{
							InOutDispKm[Vi] = NextDisp;
							bShrunk = true;
						}
					}
				}
			}

			if (!bShrunk)
			{
				// Global taper as last resort to clear stubborn bow-ties.
				for (int32 i = 0; i < N; ++i)
				{
					InOutDispKm[i] = FMath::Max(InOutDispKm[i] * 0.85f, MinCleanupKm);
				}
				bShrunk = true;
			}
			RebuildDeep();
		}

		OutSelfCrossAfter = CountClosedPolylineSelfCrossings(OutDeepKm);
	}

	static UMaterialInterface* LoadOpaqueLitParentMaterial()
	{
		const TCHAR* Paths[] = {
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"),
			TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"),
		};
		for (const TCHAR* Path : Paths)
		{
			if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path))
			{
				return M;
			}
		}
		return nullptr;
	}

	/** DEV Contours/Features: opaque Color MID (never Flatten — translucent z-fight hides tints). */
	static UMaterialInstanceDynamic* MakeOpaqueRibbonMID(UObject* Outer, const FLinearColor& Tint)
	{
		UMaterialInterface* Parent = LoadOpaqueLitParentMaterial();
		if (!Parent)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
		if (!MID)
		{
			return nullptr;
		}
		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
			FName(TEXT("Emissive")), FName(TEXT("EmissiveColor")),
		};
		for (const FName& N : ColorNames)
		{
			MID->SetVectorParameterValue(N, Tint);
		}
		return MID;
	}

	static UMaterialInterface* LoadShelfBandMaterial()
	{
		const TCHAR* Paths[] = {
			// 2026-08-28: real root cause of the earlier "WWF unchanged" report was a dead-code
			// branch at the call site (MakeOpaqueShelfCyanMID always succeeded, so this function was
			// never reached at all) — fixed separately. Once actually applied, the first entry below
			// (Waterline's Island_Material, duplicated as M_IH_ShelfSand) rendered as large,
			// basalt-like ridging rather than fine sand — confirmed via headless inspection to
			// contain a MaterialExpressionLandscapeLayerCoords node, which only produces meaningful
			// UVs on an actual ULandscapeComponent, not the UProceduralMeshComponent ShelfMesh is.
			// Replaced as first choice with M_IH_WwfSand, an IH-owned duplicate of
			// /Game/watermaterials/Materials/M_Sand — a purpose-built underwater sea-bed sand
			// material (diffuse+normal textures, animated caustics overlay, wet/dry blending, a
			// tunable "Tiling" scalar parameter) using ordinary MaterialExpressionTextureCoordinate
			// nodes, not landscape-specific ones. ShelfMesh's own UVs are P.X/100, P.Y/100 (1 UV
			// unit = 1 meter, IH_WB_IslandActor.cpp's BuildWwfShelfSection) — the same kind of
			// meter-scaled convention this material's default Tiling=4.0 was very likely already
			// tuned against, unlike the landscape-coordinate case. The old Island_Material-derived
			// entry is kept as a fallback, not removed, in case this new material needs revisiting.
			TEXT("/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand"),
			TEXT("/Game/InvisibleHand/Materials/Waterline/M_IH_ShelfSand.M_IH_ShelfSand"),
			TEXT("/Game/InvisibleHand/Materials/M_IslandPieBandCyan.M_IslandPieBandCyan"),
			TEXT("/Game/InvisibleHand/Materials/M_IslandShoreBands.M_IslandShoreBands"),
		};
		for (const TCHAR* Path : Paths)
		{
			if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path))
			{
				return M;
			}
		}
		return nullptr;
	}

	/** Opaque cyan WWF fill MID — Flatten fallback was invisible Ocean OFF. */
	static UMaterialInstanceDynamic* MakeOpaqueShelfCyanMID(UObject* Outer)
	{
		return MakeOpaqueRibbonMID(Outer, IHInvisibleHandSpec::PieBandCyanDisplayColor);
	}

	/**
	 * Unitary, collidable, islet-inclusive Sea Shelf WWF mesh (IH-DEC-039, per-cell revision).
	 * Direct per-cell fan triangulation of every Ocean cell between the coast and the shelf floor,
	 * replacing the original two-polyline ring-loft (BuildWwfShelfRingLoftTriangles, removed) that
	 * paired an independently-traced Inner coastline / Outer shelf-floor boundary into quads. That
	 * pairing was the root cause of every WWF defect this session (rejected-quad gaps, the
	 * persistent cyan "frame" anomaly, "deep trench next to sandbar" holes) — two independently-
	 * traced loops don't correspond point-for-point on real, irregular coastlines no matter how the
	 * pairing is refined. The land mesh and this function's own flat floor cap never had that
	 * problem because they build geometry the other way: iterate Graph.Cells directly and
	 * fan-triangulate each cell's own real Cell.Boundary polygon. The Voronoi generator gives 100%
	 * cell coverage of the island's full local bounding box forever (confirmed this session —
	 * nothing ever erases a cell), so there is no shape to re-derive here, only to retain.
	 *
	 * Every non-Land cell from the coast out to BottomPlaneMaxDepthCoastDistance gets
	 * fan-triangulated. Z is computed per-cell from its own CoastDistance: a continuous Lerp from
	 * the coast down to the floor across the near band (CoastDistance >= BottomPlaneMinCoastDistance),
	 * then flat at the existing floor constant beyond it — the two meet exactly at
	 * BottomPlaneMinCoastDistance by construction, so there is no seam between "ramp" and "iceberg
	 * base." Smoothed across shared boundary vertices with the exact same neighbor-averaging idiom
	 * already proven for the land mesh above (VertexHeightAccum/QuantizeVertexKey/SmoothedHeightAt
	 * pattern, Plan Addendum 12), applied to per-cell Z instead of Cell.Height. No polylines, no
	 * vertex pairing, no validity checks, no possible gap by construction. Automatically
	 * islet-inclusive and automatically bridges mainland/islet shelf where they're close, because
	 * CoastDistance is already a graph-wide multi-source BFS metric.
	 *
	 * Box-edge exclusion (IH-DEC-039 follow-up, minor correctness fix, NOT the "frame" root cause -
	 * see below): the Voronoi clip rectangle (Graph.BoundsMinLocalCm/BoundsMaxLocalCm, the exact
	 * rectangle GenerateJitteredGridSites/the Voronoi clip used to build THIS graph) truncates any
	 * cell straddling it into a sliver polygon hugging the box edge exactly. Any cell with a
	 * boundary vertex within BoxEdgeEpsilonCm of the clip rectangle is excluded from the shelf fan.
	 * Real but rare (measured 4-6 excluded cells on some DAWNS6 islands, 0 on others) - worth
	 * keeping, but too small to explain the visible artifact below.
	 *
	 * The "frame" anomaly (IH-DEC-039 follow-up, root-caused): the long, axis-aligned, actor-
	 * attached straight edge reported across many realms this session is NOT a geometry defect. A
	 * magenta material-parameter debug override (the naive per-vertex Colors-array test gave a false
	 * negative first - this material family reads a vector PARAMETER, not vertex color) proved the
	 * shelf mesh itself is correctly shaped and fully filled, confirmed by two further headless
	 * measurements: minimum distance from any shelf-band cell to the box edge was 810-619387cm
	 * (nowhere near it), and zero shelf-band cells sat more than 3km outside the real landmass
	 * bounding box (no far-flung outliers, e.g. from trough-carving reaching out to sea). The real
	 * explanation: with Ocean toggled OFF (a dev-only diagnostic view with no water surface), the
	 * shelf's pale cyan fill has very low color contrast against the atmospheric-fog "empty ocean"
	 * background, so it reads as nearly invisible except at its own far edge - the steepest local
	 * color gradient - which the "GrabContrast" post-process (checked in every affected screenshot)
	 * specifically amplifies into a bright rim line. Confirmed absent with Ocean ON (real gameplay
	 * condition) - normal water simply covers the correctly-bounded, correctly-collidable shelf, no
	 * "sandbar" impression. Collision matches the real filled geometry (same triangles as the
	 * magenta test), not the rendered sliver, so this was a diagnostic-view rendering artifact only,
	 * never a walkability/physics issue.
	 */
	static void BuildWwfShelfSection(
		UProceduralMeshComponent* ShelfMeshComp,
		UObject* Outer,
		const FIHTerrainCellGraph& Graph,
		int32 BottomPlaneMinCoastDistance,
		int32 BottomPlaneMaxDepthCoastDistance,
		int32& OutShelfTriCount,
		int32& OutSlopedBandCellCount,
		int32& OutBottomPlaneCellCount,
		int32& OutBoxEdgeExcludedCellCount,
		const TMap<FIntPoint, TPair<double, int32>>& LandVertexHeightAccum,
		double LandThresholdParam,
		double HeightSpanParam,
		float SummitTopZCmParam)
	{
		OutShelfTriCount = 0;
		OutSlopedBandCellCount = 0;
		OutBottomPlaneCellCount = 0;
		OutBoxEdgeExcludedCellCount = 0;
		if (!ShelfMeshComp)
		{
			return;
		}

		ShelfMeshComp->ClearAllMeshSections();

		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;

		const float FloorCm = IHInvisibleHandSpec::ShelfFloorMeters * 100.f;
		constexpr float ShelfTopZCm = 0.f;
		constexpr float ZBiasCm = -2.f;
		const FColor ShelfVertColor(60, 180, 190);

		// Per-cell Z: continuous Lerp from the coast (ShelfTopZCm) to the floor (FloorCm) across the
		// sloped band, flat at FloorCm beyond it. The "+1" shift lines NormalizedDepth up with the
		// band's real edges (the first Ocean hop off the coast is CoastDistance -1, not 0) so both
		// ends land exactly on ShelfTopZCm and FloorCm instead of landing partway there.
		// LocalMinHops replaces the global BottomPlaneMinCoastDistance per-cell (see slope-aware
		// width block below) - a steep-coast cell gets a smaller (closer-to-zero) LocalMinHops, so
		// its ramp reaches the floor in fewer hops: steeper underwater slope, not just a shorter cap.
		auto ComputeCellZ = [FloorCm, ShelfTopZCm](const FIHTerrainCell& Cell, int32 LocalMinHops) -> double
		{
			if (Cell.CoastDistance >= LocalMinHops)
			{
				const double Denom = static_cast<double>(LocalMinHops) + 1.0;
				const double NormalizedDepth = (FMath::Abs(Denom) < 0.5) ? 1.0
					: FMath::Clamp((static_cast<double>(Cell.CoastDistance) + 1.0) / Denom, 0.0, 1.0);
				return FMath::Lerp(static_cast<double>(ShelfTopZCm), static_cast<double>(FloorCm), NormalizedDepth);
			}
			return static_cast<double>(FloorCm);
		};
		// Cheap pre-filter only - the CoastDistance far-bound check now depends on the per-cell
		// LocalMaxHops (slope-aware width block below), so it's applied inline in the main loops
		// instead of here.
		auto IsCandidateShelfCell = [](const FIHTerrainCell& Cell) -> bool
		{
			return Cell.Feature != EIHCellFeature::Land && Cell.Boundary.Num() >= 3;
		};
		// See function comment (box-edge exclusion). 50cm epsilon: tiny relative to a real cell's
		// footprint (~7500cm across at this graph's cell density) but large enough to reliably catch
		// a clip-produced vertex sitting exactly on (or float-noise-close to) the box edge.
		constexpr double BoxEdgeEpsilonCm = 50.0;
		const FVector2D BoundsMin = Graph.BoundsMinLocalCm;
		const FVector2D BoundsMax = Graph.BoundsMaxLocalCm;
		auto IsBoxEdgeClipped = [BoundsMin, BoundsMax](const FIHTerrainCell& Cell) -> bool
		{
			for (const FVector2D& P : Cell.Boundary)
			{
				if (FMath::Abs(P.X - BoundsMin.X) < BoxEdgeEpsilonCm || FMath::Abs(P.X - BoundsMax.X) < BoxEdgeEpsilonCm
					|| FMath::Abs(P.Y - BoundsMin.Y) < BoxEdgeEpsilonCm || FMath::Abs(P.Y - BoundsMax.Y) < BoxEdgeEpsilonCm)
				{
					return true;
				}
			}
			return false;
		};

		// Slope-aware shelf width (IH-DEC-039 phase-2 refinement): narrow the shelf near steep/
		// cliff coastal cells, keep today's full width near flat/beach coastal cells, driven by
		// real local coastal height gradient - directly answers the canon rule that the Outer
		// Cyan Rim should terminate a short distance off a cliff rather than reading as a uniform
		// "sandbar." For every Haven cell (Cell.bHaven - a real coastal Land cell with >=1 Ocean/
		// Lake neighbor), steepness is how far it drops toward its nearest wet neighbor's Height
		// (already confirmed this session to extend smoothly below LandThreshold for Ocean cells
		// too, so this is a real, physically meaningful measurement, not a new tracer). Normalized
		// PER ISLAND (this island's own min/max), not a fixed absolute-degree threshold -
		// IH-DEC-038 already found this diffusion height field rarely produces genuinely "Steep"
		// terrain by an absolute measure even on Volcanic islands, so a fixed threshold would mean
		// nothing ever narrows; per-island relative normalization guarantees every island's own
		// steepest stretches show real variation from its own flattest.
		struct FHavenNarrowing { FVector2D SitePos; double NarrowingFactor; };
		TArray<FHavenNarrowing> HavenNarrowing;
		{
			double MinSteepness = TNumericLimits<double>::Max();
			double MaxSteepness = -TNumericLimits<double>::Max();
			for (const FIHTerrainCell& Cell : Graph.Cells)
			{
				if (Cell.Feature != EIHCellFeature::Land || !Cell.bHaven)
				{
					continue;
				}
				double MinWetNeighborHeight = TNumericLimits<double>::Max();
				for (const int32 NeighborIdx : Cell.Neighbors)
				{
					if (!Graph.Cells.IsValidIndex(NeighborIdx))
					{
						continue;
					}
					const FIHTerrainCell& Neighbor = Graph.Cells[NeighborIdx];
					if (Neighbor.Feature == EIHCellFeature::Ocean || Neighbor.Feature == EIHCellFeature::Lake)
					{
						MinWetNeighborHeight = FMath::Min(MinWetNeighborHeight, Neighbor.Height);
					}
				}
				if (MinWetNeighborHeight > 1.0e17) // no wet neighbor found despite bHaven - shouldn't happen
				{
					continue;
				}
				// NarrowingFactor field temporarily holds raw steepness until the normalization
				// pass below converts it.
				const double SteepnessRaw = FMath::Max(0.0, Cell.Height - MinWetNeighborHeight);
				HavenNarrowing.Add(FHavenNarrowing{Cell.SitePos, SteepnessRaw});
				MinSteepness = FMath::Min(MinSteepness, SteepnessRaw);
				MaxSteepness = FMath::Max(MaxSteepness, SteepnessRaw);
			}
			// Tunable - 1.0 (unchanged today's width) for the flattest coastal cells down to
			// MinNarrowingFactor for the steepest; refine against real PIE grabs.
			constexpr double MinNarrowingFactor = 0.3;
			const double SteepnessRange = FMath::Max(MaxSteepness - MinSteepness, KINDA_SMALL_NUMBER);
			for (FHavenNarrowing& H : HavenNarrowing)
			{
				const double Steepness01 = FMath::Clamp((H.NarrowingFactor - MinSteepness) / SteepnessRange, 0.0, 1.0);
				H.NarrowingFactor = FMath::Lerp(1.0, MinNarrowingFactor, Steepness01);
			}
		}
		auto NearestHavenNarrowingFactor = [&HavenNarrowing](const FVector2D& SitePos) -> double
		{
			if (HavenNarrowing.Num() == 0)
			{
				return 1.0;
			}
			double BestDistSq = TNumericLimits<double>::Max();
			double BestFactor = 1.0;
			for (const FHavenNarrowing& H : HavenNarrowing)
			{
				const double DistSq = FVector2D::DistSquared(SitePos, H.SitePos);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestFactor = H.NarrowingFactor;
				}
			}
			return BestFactor;
		};

		// Precompute once per cell rather than once per loop below (this nearest-Haven scan is the
		// O(shelf cells x Haven cells) cost in this function - see IH-DEC-039 phase-2 plan for the
		// spatial-grid fast-follow, TPointHashGrid2d, already used for the analogous problem in
		// WeldNearDuplicateVertices, if elapsedS below ever shows this needs it).
		TArray<double> CellNarrowingFactor;
		CellNarrowingFactor.SetNumUninitialized(Graph.Cells.Num());
		double SumNarrowingFactor = 0.0;
		double MinNarrowingFactorObserved = TNumericLimits<double>::Max();
		double MaxNarrowingFactorObserved = -TNumericLimits<double>::Max();
		int32 NarrowingFactorSampleCount = 0;
		for (int32 CellIdx = 0; CellIdx < Graph.Cells.Num(); ++CellIdx)
		{
			const FIHTerrainCell& Cell = Graph.Cells[CellIdx];
			if (!IsCandidateShelfCell(Cell))
			{
				CellNarrowingFactor[CellIdx] = 1.0;
				continue;
			}
			const double Factor = NearestHavenNarrowingFactor(Cell.SitePos);
			CellNarrowingFactor[CellIdx] = Factor;
			SumNarrowingFactor += Factor;
			MinNarrowingFactorObserved = FMath::Min(MinNarrowingFactorObserved, Factor);
			MaxNarrowingFactorObserved = FMath::Max(MaxNarrowingFactorObserved, Factor);
			++NarrowingFactorSampleCount;
		}
		if (NarrowingFactorSampleCount == 0)
		{
			MinNarrowingFactorObserved = 1.0;
			MaxNarrowingFactorObserved = 1.0;
		}

		// Same quantized-position neighbor-averaging idiom as the land mesh above (Plan Addendum 12)
		// — welds duplicate corners from adjacent cells (each cell still emits its own copy of every
		// boundary point) so the shelf reads as one continuous surface, not stepped per-cell tiles.
		TMap<FIntPoint, TPair<double, int32>> VertexZAccum;
		auto QuantizeVertexKey = [](const FVector2D& P) -> FIntPoint
		{
			constexpr double QuantCm = 1.0;
			return FIntPoint(FMath::RoundToInt32(P.X / QuantCm), FMath::RoundToInt32(P.Y / QuantCm));
		};
		for (int32 CellIdx = 0; CellIdx < Graph.Cells.Num(); ++CellIdx)
		{
			const FIHTerrainCell& Cell = Graph.Cells[CellIdx];
			if (!IsCandidateShelfCell(Cell))
			{
				continue;
			}
			const double NarrowingFactor = CellNarrowingFactor[CellIdx];
			const int32 LocalMinHops = FMath::RoundToInt(BottomPlaneMinCoastDistance * NarrowingFactor);
			const int32 LocalMaxHops = FMath::RoundToInt(BottomPlaneMaxDepthCoastDistance * NarrowingFactor);
			if (Cell.CoastDistance < LocalMaxHops)
			{
				continue;
			}
			if (IsBoxEdgeClipped(Cell))
			{
				++OutBoxEdgeExcludedCellCount;
				continue;
			}
			const double CellZ = ComputeCellZ(Cell, LocalMinHops);
			for (const FVector2D& P : Cell.Boundary)
			{
				TPair<double, int32>& Accum = VertexZAccum.FindOrAdd(QuantizeVertexKey(P));
				Accum.Key += CellZ;
				Accum.Value += 1;
			}
		}
		auto SmoothedZAt = [&VertexZAccum, &QuantizeVertexKey](const FVector2D& P, double Fallback) -> double
		{
			if (const TPair<double, int32>* Accum = VertexZAccum.Find(QuantizeVertexKey(P)))
			{
				return Accum->Value > 0 ? Accum->Key / Accum->Value : Fallback;
			}
			return Fallback;
		};

		for (int32 CellIdx = 0; CellIdx < Graph.Cells.Num(); ++CellIdx)
		{
			const FIHTerrainCell& Cell = Graph.Cells[CellIdx];
			if (!IsCandidateShelfCell(Cell))
			{
				continue;
			}
			const double NarrowingFactor = CellNarrowingFactor[CellIdx];
			const int32 LocalMinHops = FMath::RoundToInt(BottomPlaneMinCoastDistance * NarrowingFactor);
			const int32 LocalMaxHops = FMath::RoundToInt(BottomPlaneMaxDepthCoastDistance * NarrowingFactor);
			if (Cell.CoastDistance < LocalMaxHops || IsBoxEdgeClipped(Cell))
			{
				continue;
			}
			const double CellZ = ComputeCellZ(Cell, LocalMinHops);
			const bool bSloped = Cell.CoastDistance >= LocalMinHops;
			(bSloped ? OutSlopedBandCellCount : OutBottomPlaneCellCount)++;

			const int32 BaseVertId = Verts.Num();
			for (const FVector2D& P : Cell.Boundary)
			{
				// Coastal-weld fix: this vertex's Z was previously always computed from
				// ComputeCellZ's coast-is-exactly-0 assumption, but IslandMesh's own coastal
				// boundary Z can rise well above 0 wherever a shared Voronoi corner's height gets
				// pulled up by a taller inland neighbor during its own smoothing pass (Plan
				// Addendum 12) — most visible near cliffs/hills. The two meshes share identical
				// X/Y at every coincident boundary point (both read the same Graph.Cells), so
				// wherever THIS vertex position also belongs to a real land cell, use IslandMesh's
				// own Z formula exactly (not an approximation) so the two meshes weld with zero
				// gap. Cells further from the coast (no matching land vertex) fall through to the
				// original shelf-only Z unchanged.
				float FinalZCm;
				if (const TPair<double, int32>* LandAccum = LandVertexHeightAccum.Find(QuantizeVertexKey(P)))
				{
					const double LandSmoothedRaw = LandAccum->Value > 0
						? LandAccum->Key / LandAccum->Value : LandThresholdParam;
					const double LinearNormalizedHeight =
						FMath::Clamp((LandSmoothedRaw - LandThresholdParam) / HeightSpanParam, 0.0, 1.0);
					// Must match IslandMesh's own vertex-Z formula exactly (IH-DEC-058 gamma
					// included) - see IslandHeightReshapeGamma's own comment above.
					const double NormalizedHeight = FMath::Pow(LinearNormalizedHeight, IslandHeightReshapeGamma);
					FinalZCm = FMath::Max(1.f, static_cast<float>(NormalizedHeight) * SummitTopZCmParam);
				}
				else
				{
					const double SmoothedZ = SmoothedZAt(P, CellZ);
					FinalZCm = static_cast<float>(SmoothedZ) + ZBiasCm;
				}
				Verts.Add(FVector(P.X, P.Y, FinalZCm));
				Normals.Add(FVector::UpVector);
				UVs.Add(FVector2D(P.X / 100.0, P.Y / 100.0));
				Colors.Add(ShelfVertColor);
				Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
			}
			const FVector2D& P0 = Cell.Boundary[0];
			for (int32 i = 1; i + 1 < Cell.Boundary.Num(); ++i)
			{
				const FVector2D& Pi = Cell.Boundary[i];
				const FVector2D& Pi1 = Cell.Boundary[i + 1];
				const double Cross = (Pi.X - P0.X) * (Pi1.Y - P0.Y) - (Pi.Y - P0.Y) * (Pi1.X - P0.X);
				if (Cross >= 0.0)
				{
					Tris.Add(BaseVertId); Tris.Add(BaseVertId + i + 1); Tris.Add(BaseVertId + i);
				}
				else
				{
					Tris.Add(BaseVertId); Tris.Add(BaseVertId + i); Tris.Add(BaseVertId + i + 1);
				}
			}
		}

		OutShelfTriCount = Tris.Num() / 3;
		if (OutShelfTriCount < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("IH_WB_IslandActor: ShelfMesh per-cell fill produced 0 triangles"));
			return;
		}

		ShelfMeshComp->CreateMeshSection(
			0, Verts, Tris, Normals, UVs, Colors, Tangents,
			IHInvisibleHandSpec::IsWwfShelfCollisionEnabled());
		// 2026-08-28: was `MakeOpaqueShelfCyanMID` first, `LoadShelfBandMaterial` (the sand material)
		// as an "else if" fallback — but MakeOpaqueShelfCyanMID always succeeds (it only needs the
		// engine's always-present BasicShapeMaterial), so the sand-material branch was structurally
		// unreachable dead code the entire time, regardless of whether M_IH_ShelfSand itself loaded
		// or looked correct. This is the real, complete explanation for the earlier "WWF unchanged"
		// report — not a UV/lighting/water-opacity issue, the material swap was simply never applied.
		// Swapped to match LoadShelfBandMaterial's own header comment, which already documented the
		// INTENDED order ("Tried first; falls back to the original flat cyan fill if it fails to
		// load") that the old branch order never actually implemented.
		if (UMaterialInterface* ShelfMat = LoadShelfBandMaterial())
		{
			ShelfMeshComp->SetMaterial(0, ShelfMat);
		}
		else if (UMaterialInstanceDynamic* Mid = MakeOpaqueShelfCyanMID(Outer))
		{
			ShelfMeshComp->SetMaterial(0, Mid);
		}
		ShelfMeshComp->SetVisibility(true);
		ShelfMeshComp->SetHiddenInGame(false);
		if (IHInvisibleHandSpec::IsWwfShelfCollisionEnabled())
		{
			ShelfMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			ShelfMeshComp->SetCollisionObjectType(ECC_WorldStatic);
			ShelfMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
			ShelfMeshComp->ContainsPhysicsTriMeshData(true);
		}
		else
		{
			ShelfMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		const double AvgNarrowingFactor =
			NarrowingFactorSampleCount > 0 ? SumNarrowingFactor / NarrowingFactorSampleCount : 1.0;
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: ShelfMesh shelfTris=%d slopedBandCells=%d bottomPlaneCells=%d ")
			TEXT("boxEdgeExcludedCells=%d floorCm=%.0f collision=%d havenCells=%d avgNarrowing=%.2f ")
			TEXT("minNarrowing=%.2f maxNarrowing=%.2f"),
			OutShelfTriCount, OutSlopedBandCellCount, OutBottomPlaneCellCount, OutBoxEdgeExcludedCellCount,
			FloorCm, IHInvisibleHandSpec::IsWwfShelfCollisionEnabled() ? 1 : 0, HavenNarrowing.Num(),
			AvgNarrowingFactor, MinNarrowingFactorObserved, MaxNarrowingFactorObserved);
	}

	/**
	 * Closed dry-silhouette rings from the same HF triangulation IslandMesh uses (Z-omit rim):
	 * edges of fully-dry tris that are not shared with another fully-dry tri.
	 */
	static void ExtractDrySilhouetteRingsLocalCm(
		const TArray<float>& HeightsMeters,
		const int32 Side,
		const double HalfExtentMeters,
		const double SampleSpacingMeters,
		TArray<TArray<FVector2D>>& OutRingsLocalCm)
	{
		OutRingsLocalCm.Reset();
		if (Side < 2 || HeightsMeters.Num() < Side * Side)
		{
			return;
		}

		auto IsDry = [&](const int32 Idx) -> bool
		{
			return HeightsMeters.IsValidIndex(Idx) && HeightsMeters[Idx] >= 0.f;
		};
		auto PosMeters = [&](const int32 Idx) -> FVector2D
		{
			const int32 X = Idx % Side;
			const int32 Y = Idx / Side;
			return FVector2D(
				-HalfExtentMeters + X * SampleSpacingMeters,
				-HalfExtentMeters + Y * SampleSpacingMeters);
		};
		auto EdgeKey = [](int32 A, int32 B) -> uint64
		{
			if (A > B)
			{
				Swap(A, B);
			}
			return (static_cast<uint64>(static_cast<uint32>(A)) << 32)
				| static_cast<uint32>(B);
		};

		TMap<uint64, int32> EdgeUseCount;
		TMap<uint64, TPair<int32, int32>> EdgeEnds;
		auto CountDryTri = [&](const int32 I0, const int32 I1, const int32 I2)
		{
			if (!IsDry(I0) || !IsDry(I1) || !IsDry(I2))
			{
				return;
			}
			const int32 E[3][2] = {{I0, I1}, {I1, I2}, {I2, I0}};
			for (int32 EIdx = 0; EIdx < 3; ++EIdx)
			{
				const uint64 Key = EdgeKey(E[EIdx][0], E[EIdx][1]);
				EdgeUseCount.FindOrAdd(Key)++;
				EdgeEnds.Add(Key, TPair<int32, int32>(E[EIdx][0], E[EIdx][1]));
			}
		};

		for (int32 Y = 0; Y < Side - 1; ++Y)
		{
			for (int32 X = 0; X < Side - 1; ++X)
			{
				const int32 I0 = Y * Side + X;
				const int32 I1 = I0 + 1;
				const int32 I2 = I0 + Side;
				const int32 I3 = I2 + 1;
				CountDryTri(I0, I2, I1);
				CountDryTri(I1, I2, I3);
			}
		}

		TArray<TPair<FVector2D, FVector2D>> SegmentsMeters;
		SegmentsMeters.Reserve(EdgeUseCount.Num());
		for (const TPair<uint64, int32>& Pair : EdgeUseCount)
		{
			if (Pair.Value != 1)
			{
				continue;
			}
			const TPair<int32, int32>* Ends = EdgeEnds.Find(Pair.Key);
			if (!Ends)
			{
				continue;
			}
			SegmentsMeters.Emplace(PosMeters(Ends->Key), PosMeters(Ends->Value));
		}
		if (SegmentsMeters.Num() < 3)
		{
			return;
		}

		const auto QuantKey = [](const FVector2D& P) -> int64
		{
			const int32 Xi = FMath::RoundToInt(P.X * 20.0);
			const int32 Yi = FMath::RoundToInt(P.Y * 20.0);
			return (static_cast<int64>(Xi) << 32) ^ static_cast<uint32>(Yi);
		};

		TMap<int64, TArray<int32>> Adjacency;
		TArray<uint8> Used;
		Used.Init(0, SegmentsMeters.Num());
		for (int32 I = 0; I < SegmentsMeters.Num(); ++I)
		{
			Adjacency.FindOrAdd(QuantKey(SegmentsMeters[I].Key)).Add(I);
			Adjacency.FindOrAdd(QuantKey(SegmentsMeters[I].Value)).Add(I);
		}

		auto OtherEnd = [&SegmentsMeters](const int32 Seg, const FVector2D& At) -> FVector2D
		{
			const double D0 = FVector2D::DistSquared(SegmentsMeters[Seg].Key, At);
			const double D1 = FVector2D::DistSquared(SegmentsMeters[Seg].Value, At);
			return D0 <= D1 ? SegmentsMeters[Seg].Value : SegmentsMeters[Seg].Key;
		};

		auto PerimeterCm = [](const TArray<FVector2D>& Ring) -> float
		{
			float Sum = 0.f;
			for (int32 I = 0; I + 1 < Ring.Num(); ++I)
			{
				Sum += FVector2D::Distance(Ring[I], Ring[I + 1]);
			}
			if (Ring.Num() >= 3)
			{
				Sum += FVector2D::Distance(Ring.Last(), Ring[0]);
			}
			return Sum;
		};

		auto WalkRingFrom = [&](const int32 StartSeg) -> TArray<FVector2D>
		{
			TArray<FVector2D> Ring;
			FVector2D Cursor = SegmentsMeters[StartSeg].Key;
			Ring.Reserve(SegmentsMeters.Num() + 1);
			Ring.Add(Cursor * 100.f);
			Used[StartSeg] = 1;
			Cursor = OtherEnd(StartSeg, Cursor);
			Ring.Add(Cursor * 100.f);
			for (int32 Guard = 0; Guard < SegmentsMeters.Num(); ++Guard)
			{
				const TArray<int32>* Candidates = Adjacency.Find(QuantKey(Cursor));
				if (!Candidates)
				{
					break;
				}
				int32 Next = INDEX_NONE;
				for (const int32 Seg : *Candidates)
				{
					if (!Used[Seg])
					{
						Next = Seg;
						break;
					}
				}
				if (Next == INDEX_NONE)
				{
					break;
				}
				Used[Next] = 1;
				Cursor = OtherEnd(Next, Cursor);
				Ring.Add(Cursor * 100.f);
			}
			return Ring;
		};

		TArray<TArray<FVector2D>> Rings;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 I = 0; I < SegmentsMeters.Num(); ++I)
			{
				if (Used[I])
				{
					continue;
				}
				if (Pass == 0)
				{
					const int32 DegA = Adjacency.FindRef(QuantKey(SegmentsMeters[I].Key)).Num();
					const int32 DegB = Adjacency.FindRef(QuantKey(SegmentsMeters[I].Value)).Num();
					if (DegA != 1 && DegB != 1)
					{
						continue;
					}
				}
				TArray<FVector2D> Ring = WalkRingFrom(I);
				if (Ring.Num() >= 8)
				{
					Rings.Add(MoveTemp(Ring));
				}
			}
		}

		Rings.Sort([&PerimeterCm](const TArray<FVector2D>& A, const TArray<FVector2D>& B)
		{
			return PerimeterCm(A) > PerimeterCm(B);
		});

		constexpr float MinRingPerimeterCm = 5000.f;
		OutRingsLocalCm.Reserve(Rings.Num());
		for (int32 R = 0; R < Rings.Num(); ++R)
		{
			if (R == 0 || PerimeterCm(Rings[R]) >= MinRingPerimeterCm)
			{
				OutRingsLocalCm.Add(MoveTemp(Rings[R]));
			}
		}
	}

	/**
	 * Weld skirt: IslandMesh dry-silhouette (inner, HF Z) → ContourGold (outer ≈ ShelfMesh inner).
	 * Seals Z-omit waterline teeth without merging cyan ShelfMesh yet.
	 */
	static void BuildSandApronSections(
		UProceduralMeshComponent* ApronMesh,
		UObject* Outer,
		const TArray<TArray<FVector2D>>& GoldRingsLocalCm,
		const TArray<FVector2D>& MainCoastFallbackLocalCm,
		const TArray<float>& HeightsMeters,
		const int32 SamplesPerSide,
		const double HalfExtentMeters,
		const double SampleSpacingMeters,
		int32& OutApronTriCount)
	{
		OutApronTriCount = 0;
		if (!ApronMesh)
		{
			return;
		}
		ApronMesh->ClearAllMeshSections();

		TArray<const TArray<FVector2D>*> GoldRings;
		if (GoldRingsLocalCm.Num() > 0)
		{
			for (const TArray<FVector2D>& Ring : GoldRingsLocalCm)
			{
				if (Ring.Num() >= 8)
				{
					GoldRings.Add(&Ring);
				}
			}
		}
		else if (MainCoastFallbackLocalCm.Num() >= 8)
		{
			GoldRings.Add(&MainCoastFallbackLocalCm);
		}
		if (GoldRings.Num() < 1 || HeightsMeters.Num() < 4 || SamplesPerSide < 2)
		{
			ApronMesh->SetVisibility(false);
			ApronMesh->SetHiddenInGame(true);
			return;
		}

		TArray<TArray<FVector2D>> DryRingsLocalCm;
		ExtractDrySilhouetteRingsLocalCm(
			HeightsMeters, SamplesPerSide, HalfExtentMeters, SampleSpacingMeters, DryRingsLocalCm);

		const float SpacingM = FMath::Max(1.f, static_cast<float>(SampleSpacingMeters));
		const float ApronWidthM = FMath::Max(25.f, IHInvisibleHandSpec::SandApronWidthCells * SpacingM);
		const float SeawardM = FMath::Max(5.f, IHInvisibleHandSpec::SandApronSeawardCells * SpacingM);
		const float ApronWidthKm = ApronWidthM / 1000.f;
		const float SeawardKm = SeawardM / 1000.f;
		const float ApronFloorZ = IHInvisibleHandSpec::SandApronZCm;
		const int32 MaxVerts = IHInvisibleHandSpec::SandApronMaxRingVerts;
		const FLinearColor SandColor = IHInvisibleHandSpec::GetTopographyTierDisplayColor(0);
		const int32 Side = SamplesPerSide;
		const bool bDryGold = DryRingsLocalCm.Num() > 0;

		auto SampleHeightCmAtLocal = [&](const FVector2D& LocalCm) -> float
		{
			const double Xm = LocalCm.X / 100.0;
			const double Ym = LocalCm.Y / 100.0;
			const double Fx = (Xm + HalfExtentMeters) / SampleSpacingMeters;
			const double Fy = (Ym + HalfExtentMeters) / SampleSpacingMeters;
			const int32 X0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fx)), 0, Side - 1);
			const int32 Y0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fy)), 0, Side - 1);
			const int32 X1 = FMath::Min(X0 + 1, Side - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, Side - 1);
			const float Tx = static_cast<float>(Fx - X0);
			const float Ty = static_cast<float>(Fy - Y0);
			const float H00 = HeightsMeters[Y0 * Side + X0];
			const float H10 = HeightsMeters[Y0 * Side + X1];
			const float H01 = HeightsMeters[Y1 * Side + X0];
			const float H11 = HeightsMeters[Y1 * Side + X1];
			const float Hm = FMath::Lerp(
				FMath::Lerp(H00, H10, Tx),
				FMath::Lerp(H01, H11, Tx),
				Ty);
			return Hm * 100.f;
		};

		auto RingCentroidCm = [](const TArray<FVector2D>& Ring) -> FVector2D
		{
			FVector2D C = FVector2D::ZeroVector;
			for (const FVector2D& P : Ring)
			{
				C += P;
			}
			return Ring.Num() > 0 ? C / static_cast<float>(Ring.Num()) : C;
		};

		TArray<uint8> DryUsed;
		DryUsed.Init(0, DryRingsLocalCm.Num());

		int32 Section = 0;
		int32 TotalTris = 0;
		for (const TArray<FVector2D>* GoldPtr : GoldRings)
		{
			const TArray<FVector2D>& GoldLocalCm = *GoldPtr;
			TArray<FVector2D> OuterKm;
			OuterKm.Reserve(GoldLocalCm.Num());
			for (const FVector2D& P : GoldLocalCm)
			{
				OuterKm.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
			}
			FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(OuterKm);

			TArray<FVector2D> InnerKm;
			bool bUsedDryPair = false;
			if (bDryGold)
			{
				const FVector2D GoldC = RingCentroidCm(GoldLocalCm);
				int32 BestDry = INDEX_NONE;
				float BestDistSq = TNumericLimits<float>::Max();
				for (int32 D = 0; D < DryRingsLocalCm.Num(); ++D)
				{
					if (DryUsed[D])
					{
						continue;
					}
					const float DistSq = FVector2D::DistSquared(GoldC, RingCentroidCm(DryRingsLocalCm[D]));
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						BestDry = D;
					}
				}
				if (BestDry != INDEX_NONE)
				{
					DryUsed[BestDry] = 1;
					TArray<FVector2D> DryKm;
					DryKm.Reserve(DryRingsLocalCm[BestDry].Num());
					for (const FVector2D& P : DryRingsLocalCm[BestDry])
					{
						DryKm.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
					}
					FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(DryKm);
					FIHCoastPolylineSmoothing::ResampleClosedPolylineToReferenceArcLength(
						OuterKm, DryKm, InnerKm);
					bUsedDryPair = InnerKm.Num() == OuterKm.Num() && InnerKm.Num() >= 3;
				}
			}

			if (!bUsedDryPair)
			{
				// Fallback: ContourGold inset → gold+seaward (prior skirt).
				TArray<FVector2D> CoastKm = OuterKm;
				if (CoastKm.Num() > MaxVerts)
				{
					TArray<FVector2D> Capped;
					Capped.Reserve(MaxVerts);
					const int32 NFull = CoastKm.Num();
					for (int32 k = 0; k < MaxVerts; ++k)
					{
						const int32 Idx = static_cast<int32>(
							(static_cast<int64>(k) * static_cast<int64>(NFull)) / static_cast<int64>(MaxVerts));
						Capped.Add(CoastKm[FMath::Clamp(Idx, 0, NFull - 1)]);
					}
					CoastKm = MoveTemp(Capped);
				}
				TArray<FVector2D> FallbackInner;
				TArray<FVector2D> FallbackOuter;
				FIHCoastPolylineSmoothing::OffsetClosedPolylineOutwardKm(
					CoastKm, -ApronWidthKm, true, FallbackInner);
				FIHCoastPolylineSmoothing::OffsetClosedPolylineOutwardKm(
					CoastKm, SeawardKm, true, FallbackOuter);
				if (FallbackInner.Num() != CoastKm.Num() || FallbackOuter.Num() != CoastKm.Num() || CoastKm.Num() < 3)
				{
					continue;
				}
				InnerKm = MoveTemp(FallbackInner);
				OuterKm = MoveTemp(FallbackOuter);
			}

			if (OuterKm.Num() > MaxVerts && InnerKm.Num() == OuterKm.Num())
			{
				TArray<FVector2D> CapOuter;
				TArray<FVector2D> CapInner;
				CapOuter.Reserve(MaxVerts);
				CapInner.Reserve(MaxVerts);
				const int32 NFull = OuterKm.Num();
				for (int32 k = 0; k < MaxVerts; ++k)
				{
					const int32 Idx = static_cast<int32>(
						(static_cast<int64>(k) * static_cast<int64>(NFull)) / static_cast<int64>(MaxVerts));
					const int32 Clamped = FMath::Clamp(Idx, 0, NFull - 1);
					CapOuter.Add(OuterKm[Clamped]);
					CapInner.Add(InnerKm[Clamped]);
				}
				OuterKm = MoveTemp(CapOuter);
				InnerKm = MoveTemp(CapInner);
			}

			if (InnerKm.Num() != OuterKm.Num() || InnerKm.Num() < 3)
			{
				continue;
			}

			const int32 N = OuterKm.Num();
			TArray<FVector> Verts;
			TArray<int32> Tris;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FColor> Colors;
			TArray<FProcMeshTangent> Tangents;
			Verts.Reserve(N * 2);
			Normals.Reserve(N * 2);
			UVs.Reserve(N * 2);
			Colors.Reserve(N * 2);
			Tangents.Reserve(N * 2);
			Tris.Reserve(N * 6);

			const FColor SandVert = SandColor.ToFColor(true);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D InnerCm(InnerKm[i].X * 100000.f, InnerKm[i].Y * 100000.f);
				const FVector2D OuterCm(OuterKm[i].X * 100000.f, OuterKm[i].Y * 100000.f);
				const float InnerZ = FMath::Max(SampleHeightCmAtLocal(InnerCm), ApronFloorZ);
				const float OuterZ = ApronFloorZ;
				Verts.Add(FVector(InnerCm.X, InnerCm.Y, InnerZ));
				Verts.Add(FVector(OuterCm.X, OuterCm.Y, OuterZ));
				Normals.Add(FVector::UpVector);
				Normals.Add(FVector::UpVector);
				UVs.Add(FVector2D(static_cast<float>(i) / N, 0.f));
				UVs.Add(FVector2D(static_cast<float>(i) / N, 1.f));
				Colors.Add(SandVert);
				Colors.Add(SandVert);
				Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
				Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
			}

			for (int32 i = 0; i < N; ++i)
			{
				const int32 J = (i + 1) % N;
				const int32 I0 = i * 2;
				const int32 O0 = I0 + 1;
				const int32 I1 = J * 2;
				const int32 O1 = I1 + 1;
				Tris.Add(I0); Tris.Add(I1); Tris.Add(O1);
				Tris.Add(I0); Tris.Add(O1); Tris.Add(O0);
			}

			const int32 SectionTris = Tris.Num() / 3;
			if (SectionTris < 1)
			{
				continue;
			}
			ApronMesh->CreateMeshSection(
				Section, Verts, Tris, Normals, UVs, Colors, Tangents, false);
			if (UMaterialInstanceDynamic* Mid = MakeOpaqueRibbonMID(Outer, SandColor))
			{
				ApronMesh->SetMaterial(Section, Mid);
			}
			TotalTris += SectionTris;
			++Section;
		}

		OutApronTriCount = TotalTris;
		const bool bShow = TotalTris > 0;
		ApronMesh->SetVisibility(bShow);
		ApronMesh->SetHiddenInGame(!bShow);
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: SandApron apronTris=%d rings=%d dryGold=%d dryRings=%d widthM=%.1f seawardM=%.1f sloped=1"),
			OutApronTriCount, Section, bDryGold ? 1 : 0, DryRingsLocalCm.Num(), ApronWidthM, SeawardM);
	}

	/**
	 * ContourGold rim-strip v2: polyline owns coast silhouette edges (inlet-safe).
	 * Rejects ContourGold outer edges that are too long or span wet HF (flood-fill chords).
	 * Does not revive SandApron / dryGold; ShelfMesh / DeepOuter untouched.
	 */
	static void AppendContourGoldRimStrip(
		TArray<FVector>& Vertices,
		TArray<int32>& Triangles,
		TArray<FVector>& Normals,
		TArray<FVector2D>& UV0,
		TArray<FProcMeshTangent>& Tangents,
		const TArray<TArray<FVector2D>>& GoldRingsLocalCm,
		const TArray<FVector2D>& MainCoastFallbackLocalCm,
		const TArray<float>& HeightsMeters,
		const int32 SamplesPerSide,
		const double HalfExtentMeters,
		const double SampleSpacingMeters,
		int32& OutRimStripTris,
		int32& OutRimStripRings,
		int32& OutRimStripRejected)
	{
		OutRimStripTris = 0;
		OutRimStripRings = 0;
		OutRimStripRejected = 0;
		if (!IHInvisibleHandSpec::IsContourGoldRimStripEnabled())
		{
			return;
		}

		TArray<const TArray<FVector2D>*> GoldRings;
		if (GoldRingsLocalCm.Num() > 0)
		{
			for (const TArray<FVector2D>& Ring : GoldRingsLocalCm)
			{
				if (Ring.Num() >= 8)
				{
					GoldRings.Add(&Ring);
				}
			}
		}
		else if (MainCoastFallbackLocalCm.Num() >= 8)
		{
			GoldRings.Add(&MainCoastFallbackLocalCm);
		}
		if (GoldRings.Num() < 1 || HeightsMeters.Num() < 4 || SamplesPerSide < 2)
		{
			return;
		}

		TArray<TArray<FVector2D>> DryRingsLocalCm;
		ExtractDrySilhouetteRingsLocalCm(
			HeightsMeters, SamplesPerSide, HalfExtentMeters, SampleSpacingMeters, DryRingsLocalCm);

		const float SpacingM = FMath::Max(1.f, static_cast<float>(SampleSpacingMeters));
		const float InlandOffsetKm = FMath::Max(SpacingM, 0.85f * SpacingM) / 1000.f;
		const float MaxOuterEdgeCm =
			IHInvisibleHandSpec::ContourGoldRimStripMaxEdgeSpacingMul * SpacingM * 100.f;
		const int32 MaxVerts = IHInvisibleHandSpec::ContourGoldRimStripMaxRingVerts;
		const int32 Side = SamplesPerSide;

		auto SampleHeightCmAtLocal = [&](const FVector2D& LocalCm) -> float
		{
			const double Xm = LocalCm.X / 100.0;
			const double Ym = LocalCm.Y / 100.0;
			const double Fx = (Xm + HalfExtentMeters) / SampleSpacingMeters;
			const double Fy = (Ym + HalfExtentMeters) / SampleSpacingMeters;
			const int32 X0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fx)), 0, Side - 1);
			const int32 Y0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fy)), 0, Side - 1);
			const int32 X1 = FMath::Min(X0 + 1, Side - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, Side - 1);
			const float Tx = static_cast<float>(Fx - X0);
			const float Ty = static_cast<float>(Fy - Y0);
			const float H00 = HeightsMeters[Y0 * Side + X0];
			const float H10 = HeightsMeters[Y0 * Side + X1];
			const float H01 = HeightsMeters[Y1 * Side + X0];
			const float H11 = HeightsMeters[Y1 * Side + X1];
			const float Hm = FMath::Lerp(
				FMath::Lerp(H00, H10, Tx),
				FMath::Lerp(H01, H11, Tx),
				Ty);
			return Hm * 100.f;
		};

		auto RingCentroidCm = [](const TArray<FVector2D>& Ring) -> FVector2D
		{
			FVector2D C = FVector2D::ZeroVector;
			for (const FVector2D& P : Ring)
			{
				C += P;
			}
			return Ring.Num() > 0 ? C / static_cast<float>(Ring.Num()) : C;
		};

		auto CapPairedRings = [MaxVerts](TArray<FVector2D>& OuterKm, TArray<FVector2D>& InnerKm)
		{
			if (OuterKm.Num() <= MaxVerts || InnerKm.Num() != OuterKm.Num())
			{
				return;
			}
			TArray<FVector2D> CapOuter;
			TArray<FVector2D> CapInner;
			CapOuter.Reserve(MaxVerts);
			CapInner.Reserve(MaxVerts);
			const int32 NFull = OuterKm.Num();
			for (int32 k = 0; k < MaxVerts; ++k)
			{
				const int32 Idx = static_cast<int32>(
					(static_cast<int64>(k) * static_cast<int64>(NFull)) / static_cast<int64>(MaxVerts));
				const int32 Clamped = FMath::Clamp(Idx, 0, NFull - 1);
				CapOuter.Add(OuterKm[Clamped]);
				CapInner.Add(InnerKm[Clamped]);
			}
			OuterKm = MoveTemp(CapOuter);
			InnerKm = MoveTemp(CapInner);
		};

		TArray<uint8> DryUsed;
		DryUsed.Init(0, DryRingsLocalCm.Num());

		for (const TArray<FVector2D>* GoldPtr : GoldRings)
		{
			const TArray<FVector2D>& GoldLocalCm = *GoldPtr;
			TArray<FVector2D> OuterKm;
			OuterKm.Reserve(GoldLocalCm.Num());
			for (const FVector2D& P : GoldLocalCm)
			{
				OuterKm.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
			}
			FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(OuterKm);

			TArray<FVector2D> InnerKm;
			bool bUsedDryPair = false;
			if (DryRingsLocalCm.Num() > 0)
			{
				const FVector2D GoldC = RingCentroidCm(GoldLocalCm);
				int32 BestDry = INDEX_NONE;
				float BestDistSq = TNumericLimits<float>::Max();
				for (int32 D = 0; D < DryRingsLocalCm.Num(); ++D)
				{
					if (DryUsed[D])
					{
						continue;
					}
					const float DistSq = FVector2D::DistSquared(GoldC, RingCentroidCm(DryRingsLocalCm[D]));
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						BestDry = D;
					}
				}
				if (BestDry != INDEX_NONE)
				{
					DryUsed[BestDry] = 1;
					TArray<FVector2D> DryKm;
					DryKm.Reserve(DryRingsLocalCm[BestDry].Num());
					for (const FVector2D& P : DryRingsLocalCm[BestDry])
					{
						DryKm.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
					}
					FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(DryKm);
					FIHCoastPolylineSmoothing::ResampleClosedPolylineToReferenceArcLength(
						OuterKm, DryKm, InnerKm);
					bUsedDryPair = InnerKm.Num() == OuterKm.Num() && InnerKm.Num() >= 3;
				}
			}

			if (!bUsedDryPair)
			{
				TArray<FVector2D> CoastKm = OuterKm;
				if (CoastKm.Num() > MaxVerts)
				{
					TArray<FVector2D> Capped;
					Capped.Reserve(MaxVerts);
					const int32 NFull = CoastKm.Num();
					for (int32 k = 0; k < MaxVerts; ++k)
					{
						const int32 Idx = static_cast<int32>(
							(static_cast<int64>(k) * static_cast<int64>(NFull)) / static_cast<int64>(MaxVerts));
						Capped.Add(CoastKm[FMath::Clamp(Idx, 0, NFull - 1)]);
					}
					CoastKm = MoveTemp(Capped);
					OuterKm = CoastKm;
				}
				TArray<FVector2D> FallbackInner;
				FIHCoastPolylineSmoothing::OffsetClosedPolylineOutwardKm(
					CoastKm, -InlandOffsetKm, true, FallbackInner);
				if (FallbackInner.Num() != CoastKm.Num() || CoastKm.Num() < 3)
				{
					continue;
				}
				InnerKm = MoveTemp(FallbackInner);
			}

			CapPairedRings(OuterKm, InnerKm);
			if (InnerKm.Num() != OuterKm.Num() || InnerKm.Num() < 3)
			{
				continue;
			}

			const int32 RN = OuterKm.Num();
			const int32 BaseVert = Vertices.Num();
			Vertices.Reserve(BaseVert + RN * 2);
			Normals.Reserve(BaseVert + RN * 2);
			UV0.Reserve(BaseVert + RN * 2);
			Tangents.Reserve(BaseVert + RN * 2);

			for (int32 i = 0; i < RN; ++i)
			{
				const FVector2D InnerCm(InnerKm[i].X * 100000.f, InnerKm[i].Y * 100000.f);
				const FVector2D OuterCm(OuterKm[i].X * 100000.f, OuterKm[i].Y * 100000.f);
				const float InnerZ = FMath::Max(SampleHeightCmAtLocal(InnerCm), 0.f);
				const float OuterZ = 0.f;
				Vertices.Add(FVector(InnerCm.X, InnerCm.Y, InnerZ));
				Vertices.Add(FVector(OuterCm.X, OuterCm.Y, OuterZ));

				const FVector2D Edge = OuterCm - InnerCm;
				FVector Nrm(-Edge.Y, Edge.X, 0.f);
				if (!Nrm.Normalize())
				{
					Nrm = FVector::UpVector;
				}
				else
				{
					Nrm = (Nrm + FVector::UpVector * 2.5f).GetSafeNormal();
				}
				Normals.Add(Nrm);
				Normals.Add(Nrm);
				UV0.Add(FVector2D(static_cast<float>(i) / RN, 0.f));
				UV0.Add(FVector2D(static_cast<float>(i) / RN, 1.f));
				Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
				Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
			}

			int32 RingAcceptedTris = 0;
			for (int32 i = 0; i < RN; ++i)
			{
				const int32 J = (i + 1) % RN;
				const FVector2D OuterA(OuterKm[i].X * 100000.f, OuterKm[i].Y * 100000.f);
				const FVector2D OuterB(OuterKm[J].X * 100000.f, OuterKm[J].Y * 100000.f);
				const FVector2D InnerA(InnerKm[i].X * 100000.f, InnerKm[i].Y * 100000.f);
				const FVector2D InnerB(InnerKm[J].X * 100000.f, InnerKm[J].Y * 100000.f);
				const float OuterLenCm = FVector2D::Distance(OuterA, OuterB);
				if (OuterLenCm > MaxOuterEdgeCm)
				{
					++OutRimStripRejected;
					continue;
				}
				// Wet mid-edge / strip mid = water-span chord across inlet.
				const FVector2D OuterMid = (OuterA + OuterB) * 0.5f;
				const FVector2D StripMid = (OuterMid + (InnerA + InnerB) * 0.5f) * 0.5f;
				if (SampleHeightCmAtLocal(OuterMid) < 0.f || SampleHeightCmAtLocal(StripMid) < 0.f)
				{
					++OutRimStripRejected;
					continue;
				}

				const int32 I0 = BaseVert + i * 2;
				const int32 O0 = I0 + 1;
				const int32 I1 = BaseVert + J * 2;
				const int32 O1 = I1 + 1;
				Triangles.Add(I0); Triangles.Add(O0); Triangles.Add(O1);
				Triangles.Add(I0); Triangles.Add(O1); Triangles.Add(I1);
				RingAcceptedTris += 2;
			}

			if (RingAcceptedTris > 0)
			{
				OutRimStripTris += RingAcceptedTris;
				++OutRimStripRings;
			}
		}
	}

	/**
	 * ContourGold coastal-belt remesh v1 (Problem A / gameplay IslandMesh).
	 * ContourGold outer ↔ inland offset — no dry-silhouette pairing, no Cheb MS omit.
	 * Max-edge + wet mid-edge reject (inlet-safe). Do No Harm: disable via
	 * bContourGoldCoastBeltEnabled if C1 flood-fill chords return.
	 */
	static void AppendContourGoldCoastBelt(
		TArray<FVector>& Vertices,
		TArray<int32>& Triangles,
		TArray<FVector>& Normals,
		TArray<FVector2D>& UV0,
		TArray<FProcMeshTangent>& Tangents,
		const TArray<TArray<FVector2D>>& GoldRingsLocalCm,
		const TArray<FVector2D>& MainCoastFallbackLocalCm,
		const TArray<float>& HeightsMeters,
		const int32 SamplesPerSide,
		const double HalfExtentMeters,
		const double SampleSpacingMeters,
		int32& OutCoastBeltTris,
		int32& OutCoastBeltRings,
		int32& OutCoastBeltRejected)
	{
		OutCoastBeltTris = 0;
		OutCoastBeltRings = 0;
		OutCoastBeltRejected = 0;
		if (!IHInvisibleHandSpec::IsContourGoldCoastBeltEnabled())
		{
			return;
		}

		TArray<const TArray<FVector2D>*> GoldRings;
		if (GoldRingsLocalCm.Num() > 0)
		{
			for (const TArray<FVector2D>& Ring : GoldRingsLocalCm)
			{
				if (Ring.Num() >= 8)
				{
					GoldRings.Add(&Ring);
				}
			}
		}
		else if (MainCoastFallbackLocalCm.Num() >= 8)
		{
			GoldRings.Add(&MainCoastFallbackLocalCm);
		}
		if (GoldRings.Num() < 1 || HeightsMeters.Num() < 4 || SamplesPerSide < 2)
		{
			return;
		}

		const float SpacingM = FMath::Max(1.f, static_cast<float>(SampleSpacingMeters));
		const float InlandOffsetKm =
			FMath::Max(SpacingM, IHInvisibleHandSpec::ContourGoldCoastBeltInlandSpacingMul * SpacingM)
			/ 1000.f;
		const float MaxOuterEdgeCm =
			IHInvisibleHandSpec::ContourGoldCoastBeltMaxEdgeSpacingMul * SpacingM * 100.f;
		const int32 MaxVerts = IHInvisibleHandSpec::ContourGoldCoastBeltMaxRingVerts;
		const int32 Side = SamplesPerSide;

		auto SampleHeightCmAtLocal = [&](const FVector2D& LocalCm) -> float
		{
			const double Xm = LocalCm.X / 100.0;
			const double Ym = LocalCm.Y / 100.0;
			const double Fx = (Xm + HalfExtentMeters) / SampleSpacingMeters;
			const double Fy = (Ym + HalfExtentMeters) / SampleSpacingMeters;
			const int32 X0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fx)), 0, Side - 1);
			const int32 Y0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fy)), 0, Side - 1);
			const int32 X1 = FMath::Min(X0 + 1, Side - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, Side - 1);
			const float Tx = static_cast<float>(Fx - X0);
			const float Ty = static_cast<float>(Fy - Y0);
			const float H00 = HeightsMeters[Y0 * Side + X0];
			const float H10 = HeightsMeters[Y0 * Side + X1];
			const float H01 = HeightsMeters[Y1 * Side + X0];
			const float H11 = HeightsMeters[Y1 * Side + X1];
			const float Hm = FMath::Lerp(
				FMath::Lerp(H00, H10, Tx),
				FMath::Lerp(H01, H11, Tx),
				Ty);
			return Hm * 100.f;
		};

		auto CapPairedRings = [MaxVerts](TArray<FVector2D>& OuterKm, TArray<FVector2D>& InnerKm)
		{
			if (OuterKm.Num() <= MaxVerts || InnerKm.Num() != OuterKm.Num())
			{
				return;
			}
			TArray<FVector2D> CapOuter;
			TArray<FVector2D> CapInner;
			CapOuter.Reserve(MaxVerts);
			CapInner.Reserve(MaxVerts);
			const int32 NFull = OuterKm.Num();
			for (int32 k = 0; k < MaxVerts; ++k)
			{
				const int32 Idx = static_cast<int32>(
					(static_cast<int64>(k) * static_cast<int64>(NFull)) / static_cast<int64>(MaxVerts));
				const int32 Clamped = FMath::Clamp(Idx, 0, NFull - 1);
				CapOuter.Add(OuterKm[Clamped]);
				CapInner.Add(InnerKm[Clamped]);
			}
			OuterKm = MoveTemp(CapOuter);
			InnerKm = MoveTemp(CapInner);
		};

		for (const TArray<FVector2D>* GoldPtr : GoldRings)
		{
			const TArray<FVector2D>& GoldLocalCm = *GoldPtr;
			TArray<FVector2D> OuterKm;
			OuterKm.Reserve(GoldLocalCm.Num());
			for (const FVector2D& P : GoldLocalCm)
			{
				OuterKm.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
			}
			FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(OuterKm);

			if (OuterKm.Num() > MaxVerts)
			{
				TArray<FVector2D> Capped;
				Capped.Reserve(MaxVerts);
				const int32 NFull = OuterKm.Num();
				for (int32 k = 0; k < MaxVerts; ++k)
				{
					const int32 Idx = static_cast<int32>(
						(static_cast<int64>(k) * static_cast<int64>(NFull)) / static_cast<int64>(MaxVerts));
					Capped.Add(OuterKm[FMath::Clamp(Idx, 0, NFull - 1)]);
				}
				OuterKm = MoveTemp(Capped);
			}

			TArray<FVector2D> InnerKm;
			FIHCoastPolylineSmoothing::OffsetClosedPolylineOutwardKm(
				OuterKm, -InlandOffsetKm, true, InnerKm);
			if (InnerKm.Num() != OuterKm.Num() || OuterKm.Num() < 3)
			{
				continue;
			}
			CapPairedRings(OuterKm, InnerKm);
			if (InnerKm.Num() != OuterKm.Num() || InnerKm.Num() < 3)
			{
				continue;
			}

			const int32 RN = OuterKm.Num();
			const int32 BaseVert = Vertices.Num();
			Vertices.Reserve(BaseVert + RN * 2);
			Normals.Reserve(BaseVert + RN * 2);
			UV0.Reserve(BaseVert + RN * 2);
			Tangents.Reserve(BaseVert + RN * 2);

			for (int32 i = 0; i < RN; ++i)
			{
				const FVector2D InnerCm(InnerKm[i].X * 100000.f, InnerKm[i].Y * 100000.f);
				const FVector2D OuterCm(OuterKm[i].X * 100000.f, OuterKm[i].Y * 100000.f);
				const float InnerZ = FMath::Max(SampleHeightCmAtLocal(InnerCm), 0.f);
				const float OuterZ = 0.f;
				Vertices.Add(FVector(InnerCm.X, InnerCm.Y, InnerZ));
				Vertices.Add(FVector(OuterCm.X, OuterCm.Y, OuterZ));

				const FVector2D Edge = OuterCm - InnerCm;
				FVector Nrm(-Edge.Y, Edge.X, 0.f);
				if (!Nrm.Normalize())
				{
					Nrm = FVector::UpVector;
				}
				else
				{
					Nrm = (Nrm + FVector::UpVector * 2.5f).GetSafeNormal();
				}
				Normals.Add(Nrm);
				Normals.Add(Nrm);
				UV0.Add(FVector2D(static_cast<float>(i) / RN, 0.f));
				UV0.Add(FVector2D(static_cast<float>(i) / RN, 1.f));
				Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
				Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
			}

			int32 RingAcceptedTris = 0;
			for (int32 i = 0; i < RN; ++i)
			{
				const int32 J = (i + 1) % RN;
				const FVector2D OuterA(OuterKm[i].X * 100000.f, OuterKm[i].Y * 100000.f);
				const FVector2D OuterB(OuterKm[J].X * 100000.f, OuterKm[J].Y * 100000.f);
				const FVector2D InnerA(InnerKm[i].X * 100000.f, InnerKm[i].Y * 100000.f);
				const FVector2D InnerB(InnerKm[J].X * 100000.f, InnerKm[J].Y * 100000.f);
				const float OuterLenCm = FVector2D::Distance(OuterA, OuterB);
				if (OuterLenCm > MaxOuterEdgeCm)
				{
					++OutCoastBeltRejected;
					continue;
				}
				const FVector2D OuterMid = (OuterA + OuterB) * 0.5f;
				const FVector2D StripMid = (OuterMid + (InnerA + InnerB) * 0.5f) * 0.5f;
				if (SampleHeightCmAtLocal(OuterMid) < 0.f || SampleHeightCmAtLocal(StripMid) < 0.f)
				{
					++OutCoastBeltRejected;
					continue;
				}

				const int32 I0 = BaseVert + i * 2;
				const int32 O0 = I0 + 1;
				const int32 I1 = BaseVert + J * 2;
				const int32 O1 = I1 + 1;
				Triangles.Add(I0); Triangles.Add(O0); Triangles.Add(O1);
				Triangles.Add(I0); Triangles.Add(O1); Triangles.Add(I1);
				RingAcceptedTris += 2;
			}

			if (RingAcceptedTris > 0)
			{
				OutCoastBeltTris += RingAcceptedTris;
				++OutCoastBeltRings;
			}
		}
	}

	/** Defined below, alongside the rest of the DT biome classifier — forward-declared here since
	 * CreateIslandBiomeMaterial needs it and this file compiles top-to-bottom in one pass. */
	static FLinearColor ParseBiomeHexColor(const FString& HexIn);

	static UMaterialInstanceDynamic* CreateIslandBiomeMaterial(UObject* Outer, const FIHASLSlopeBiomeRow& Row)
	{
		UMaterialInterface* Parent = LoadOpaqueLitParentMaterial();
		if (!Parent || !Outer)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
		if (!MID)
		{
			return nullptr;
		}
		const FLinearColor BiomeColor = ParseBiomeHexColor(Row.biomeColor);
		const float AlbedoScale = IHDevViewRuntime::IsGrabContrastEnabled()
			? IHInvisibleHandSpec::TopographyGrabContrastAlbedoScale
			: 1.f;
		const FLinearColor DisplayColor(
			BiomeColor.R * AlbedoScale,
			BiomeColor.G * AlbedoScale,
			BiomeColor.B * AlbedoScale,
			1.f);
		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
		};
		for (const FName& N : ColorNames)
		{
			MID->SetVectorParameterValue(N, DisplayColor);
		}
		MID->SetScalarParameterValue(FName(TEXT("Roughness")),
			IHDevViewRuntime::IsGrabContrastEnabled() ? 0.96f : 0.92f);
		MID->SetScalarParameterValue(FName(TEXT("Specular")),
			IHDevViewRuntime::IsGrabContrastEnabled() ? 0.06f : 0.10f);
		MID->SetScalarParameterValue(FName(TEXT("Opacity")), 1.f);
		return MID;
	}

	/** Artist-authored biome band color, sRGB hex as picked in Excel — must gamma-decode via
	 * FromSRGBColor, not a naive byte-divide (FColor::ReinterpretAsLinear() would read too dark). */
	static FLinearColor ParseBiomeHexColor(const FString& HexIn)
	{
		FString Hex = HexIn;
		Hex.RemoveFromStart(TEXT("#"));
		if (Hex.Len() < 6)
		{
			return FLinearColor::Gray;
		}
		return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
	}

	/** Resolves the live DT_ASLSlopeBiome rows once per island build (not per-triangle) — Outer
	 * must be a UObject whose GetWorld() reaches a live UGameInstance (true for
	 * AIH_WB_IslandActor). Every row is a real biome band post-IH-DEC-054 (the SEA LEVEL marker
	 * row is gone, along with bPcgEligible) — sorts by sortOrder so match order is explicit rather
	 * than relying on DataTable/CSV row order alone. */
	static TArray<const FIHASLSlopeBiomeRow*> GetBiomeRowsSortedForClassification(UObject* Outer)
	{
		TArray<const FIHASLSlopeBiomeRow*> Rows;
		const UWorld* World = Outer ? Outer->GetWorld() : nullptr;
		const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		const UIH_WorldBuilderDataSubsystem* Subsystem =
			GI ? GI->GetSubsystem<UIH_WorldBuilderDataSubsystem>() : nullptr;
		UDataTable* Table = Subsystem ? Subsystem->GetASLSlopeBiomeTable() : nullptr;
		if (!Table)
		{
			return Rows;
		}
		TArray<FIHASLSlopeBiomeRow*> RawRows;
		Table->GetAllRows(TEXT("IslandBiomeClassify"), RawRows);
		Rows.Reserve(RawRows.Num());
		for (const FIHASLSlopeBiomeRow* R : RawRows)
		{
			if (R)
			{
				Rows.Add(R);
			}
		}
		Rows.Sort([](const FIHASLSlopeBiomeRow& A, const FIHASLSlopeBiomeRow& B)
		{
			return A.sortOrder < B.sortOrder;
		});
		return Rows;
	}

	/** DT-driven replacement for the old 5-tier smoothstep classifier (IH-DEC-052 revised Phase 2
	 * — decided: full replace, not kept as a fallback). Matches real elevation (meters) + face
	 * slope (degrees) against DT_ASLSlopeBiome's PCG-eligible rows; first match by sortOrder wins
	 * (decided tie-break). Latitude zone gating is deliberately skipped for now — Phase 3 (latitude
	 * selector) doesn't exist yet, so every row is zone-eligible until then (decided). Falls back
	 * to the nearest ASL-only match (ignoring slope) if no row's slope range covers this point, so
	 * a real elevation is never left unclassified by a slope-range gap; returns INDEX_NONE only if
	 * no row's ASL range covers this point at all. */
	static int32 ClassifyBiomeRowIndex(
		const TArray<const FIHASLSlopeBiomeRow*>& Rows, const float Zmeters, const float SlopeDeg)
	{
		int32 NearestAslOnlyIndex = INDEX_NONE;
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			const FIHASLSlopeBiomeRow* Row = Rows[i];
			if (Zmeters < Row->aslLowerM || Zmeters > Row->aslUpperM)
			{
				continue;
			}
			if (NearestAslOnlyIndex == INDEX_NONE)
			{
				NearestAslOnlyIndex = i;
			}
			if (Row->bSlopeAgnostic || (SlopeDeg >= Row->minSlopeDeg && SlopeDeg <= Row->maxSlopeDeg))
			{
				return i;
			}
		}
		return NearestAslOnlyIndex;
	}

	/** Per-triangle DT biome match, using real face geometry (average elevation + true face-normal
	 * slope angle in degrees) rather than per-vertex voting — DT rows are hard-bounded bands, not
	 * smoothstep-blended tiers, so a single face-level sample is enough and needs no separate
	 * cliff-force/summit-override special cases (those existed only to compensate for the old
	 * system's mushy blending; real per-row ASL+slope ranges already cover steep faces and summits
	 * directly, e.g. Apex Caps 2201-2400m/81-90 deg, Volcanic Rim, the Ore Escarpment rows). */
	static int32 ClassifyTriangleBiomeRow(
		const TArray<const FIHASLSlopeBiomeRow*>& Rows,
		const TArray<FVector>& Vertices,
		const int32 I0,
		const int32 I1,
		const int32 I2,
		const bool bWaterlineClamp)
	{
		if (!Vertices.IsValidIndex(I0) || !Vertices.IsValidIndex(I1) || !Vertices.IsValidIndex(I2))
		{
			return INDEX_NONE;
		}

		const FVector& P0 = Vertices[I0];
		const FVector& P1 = Vertices[I1];
		const FVector& P2 = Vertices[I2];

		const bool bWet0 = P0.Z < 0.f;
		const bool bWet1 = P1.Z < 0.f;
		const bool bWet2 = P2.Z < 0.f;
		if (bWet0 && bWet1 && bWet2)
		{
			// All-wet: omit (no seaward ASL0 checkerboard). Mixed/dry kept when clamp is on.
			return INDEX_NONE;
		}
		if (!bWaterlineClamp && (bWet0 || bWet1 || bWet2))
		{
			// Legacy open-rim omit (sawtooth) — kept only if clamp flag is off.
			return INDEX_NONE;
		}

		const float Z0 = bWaterlineClamp ? FMath::Max(P0.Z, 0.f) : P0.Z;
		const float Z1 = bWaterlineClamp ? FMath::Max(P1.Z, 0.f) : P1.Z;
		const float Z2 = bWaterlineClamp ? FMath::Max(P2.Z, 0.f) : P2.Z;

		const FVector C0(P0.X, P0.Y, Z0);
		const FVector C1(P1.X, P1.Y, Z1);
		const FVector C2(P2.X, P2.Y, Z2);
		const FVector FaceCross = FVector::CrossProduct(C1 - C0, C2 - C0);
		const float FaceCrossLen = FaceCross.Size();
		float SlopeDeg = 0.f;
		if (FaceCrossLen > KINDA_SMALL_NUMBER)
		{
			const float CosSlope = FMath::Clamp(FMath::Abs(FaceCross.Z / FaceCrossLen), 0.f, 1.f);
			SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(CosSlope));
		}

		const float AvgZmeters = (Z0 + Z1 + Z2) / 3.f / 100.f;
		return ClassifyBiomeRowIndex(Rows, AvgZmeters, SlopeDeg);
	}

	/** DT-driven biome-color island appearance (IH-DEC-052 revised Phase 2) — replaces the old
	 * 5-tier Sand/Grass/Dirt/Rock/Snow smoothstep system entirely (decided: full replace, not kept
	 * as a fallback). One PMC section per distinct DT_ASLSlopeBiome row actually matched on this
	 * island, each with its own constant-color lit MID sourced from that row's own biomeColor hex
	 * (the source chart's own artist-authored Biome Name cell fill, not a derived value). */
	static void ApplyDtBiomeColorBands(
		UProceduralMeshComponent* Mesh,
		UObject* Outer,
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		const TArray<FVector>& Normals,
		const TArray<FVector2D>& UV0,
		const TArray<FProcMeshTangent>& Tangents,
		int32& OutClassifiedTriCount,
		int32& OutDistinctBiomeCount,
		int32& OutMixedClampTris)
	{
		OutClassifiedTriCount = 0;
		OutDistinctBiomeCount = 0;
		OutMixedClampTris = 0;
		if (!Mesh)
		{
			return;
		}

		const TArray<const FIHASLSlopeBiomeRow*> Rows = GetBiomeRowsSortedForClassification(Outer);
		if (Rows.Num() == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("IH_WB_IslandActor: DT_ASLSlopeBiome unavailable — IslandMesh left unclassified (no color bands)."));
			return;
		}

		const bool bWaterlineClamp = IHInvisibleHandSpec::IsIslandWaterlineClampEnabled();

		// Clamp wet verts to ASL 0 for kept (dry/mixed) tris — seals open Z-omit rim teeth.
		TArray<FVector> MeshVerts = Vertices;
		if (bWaterlineClamp)
		{
			for (FVector& V : MeshVerts)
			{
				V.Z = FMath::Max(V.Z, 0.f);
			}
		}

		TMap<int32, TArray<int32>> RowTriangles;
		for (int32 TriBase = 0; TriBase + 2 < Triangles.Num(); TriBase += 3)
		{
			const int32 I0 = Triangles[TriBase];
			const int32 I1 = Triangles[TriBase + 1];
			const int32 I2 = Triangles[TriBase + 2];
			const int32 RowIdx = ClassifyTriangleBiomeRow(Rows, Vertices, I0, I1, I2, bWaterlineClamp);
			if (RowIdx == INDEX_NONE)
			{
				continue;
			}
			if (bWaterlineClamp
				&& Vertices.IsValidIndex(I0) && Vertices.IsValidIndex(I1) && Vertices.IsValidIndex(I2)
				&& (Vertices[I0].Z < 0.f || Vertices[I1].Z < 0.f || Vertices[I2].Z < 0.f))
			{
				++OutMixedClampTris;
			}
			TArray<int32>& RowTris = RowTriangles.FindOrAdd(RowIdx);
			RowTris.Add(I0);
			RowTris.Add(I1);
			RowTris.Add(I2);
			++OutClassifiedTriCount;
		}

		Mesh->ClearAllMeshSections();

		// Shared vert buffers; per-matched-row index lists. Collision on every section (unitary walk).
		int32 SectionIdx = 0;
		for (const TPair<int32, TArray<int32>>& Pair : RowTriangles)
		{
			const TArray<int32>& RowTris = Pair.Value;
			if (RowTris.Num() < 3)
			{
				continue;
			}

			TArray<FColor> DummyColors;
			DummyColors.Init(FColor::White, MeshVerts.Num());
			Mesh->CreateMeshSection(
				SectionIdx, MeshVerts, RowTris, Normals, UV0, DummyColors, Tangents, true);
			if (UMaterialInstanceDynamic* Mid = CreateIslandBiomeMaterial(Outer, *Rows[Pair.Key]))
			{
				Mesh->SetMaterial(SectionIdx, Mid);
			}
			++SectionIdx;
			++OutDistinctBiomeCount;
		}

		// Fallback: if every tri was omitted (e.g. DT never actually matched anything), keep a
		// dry-only hull for collision, colored from the first available row rather than left blank.
		if (SectionIdx == 0 && Triangles.Num() >= 3)
		{
			TArray<int32> DryTris;
			DryTris.Reserve(Triangles.Num());
			for (int32 TriBase = 0; TriBase + 2 < Triangles.Num(); TriBase += 3)
			{
				const int32 I0 = Triangles[TriBase];
				const int32 I1 = Triangles[TriBase + 1];
				const int32 I2 = Triangles[TriBase + 2];
				if (!Vertices.IsValidIndex(I0) || !Vertices.IsValidIndex(I1) || !Vertices.IsValidIndex(I2))
				{
					continue;
				}
				if (Vertices[I0].Z < 0.f || Vertices[I1].Z < 0.f || Vertices[I2].Z < 0.f)
				{
					continue;
				}
				DryTris.Add(I0);
				DryTris.Add(I1);
				DryTris.Add(I2);
			}
			if (DryTris.Num() >= 3)
			{
				TArray<FColor> DummyColors;
				DummyColors.Init(FColor::White, MeshVerts.Num());
				Mesh->CreateMeshSection(0, MeshVerts, DryTris, Normals, UV0, DummyColors, Tangents, true);
				if (UMaterialInstanceDynamic* Mid = CreateIslandBiomeMaterial(Outer, *Rows[0]))
				{
					Mesh->SetMaterial(0, Mid);
				}
			}
		}

		Mesh->ContainsPhysicsTriMeshData(true);
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: IslandMesh waterlineClamp=%d mixedClampTris=%d classifiedTris=%d distinctBiomes=%d"),
			bWaterlineClamp ? 1 : 0, OutMixedClampTris, OutClassifiedTriCount, OutDistinctBiomeCount);
	}
}

AIH_WB_IslandActor::AIH_WB_IslandActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	IslandMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("IslandMesh"));
	IslandMesh->SetupAttachment(SceneRoot);
	IslandMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	IslandMesh->SetCollisionObjectType(ECC_WorldStatic);
	IslandMesh->SetCollisionResponseToAllChannels(ECR_Block);
	IslandMesh->ComponentTags.Add(FName(TEXT("IH_Island")));

	ShelfMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ShelfMesh"));
	ShelfMesh->SetupAttachment(SceneRoot);
	ShelfMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SandApronMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SandApronMesh"));
	SandApronMesh->SetupAttachment(SceneRoot);
	SandApronMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ContourRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ContourRibbonMesh"));
	ContourRibbonMesh->SetupAttachment(SceneRoot);
	ContourRibbonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContourRibbonMesh->SetHiddenInGame(true);
	ContourRibbonMesh->SetVisibility(false);

	FeatureRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FeatureRibbonMesh"));
	FeatureRibbonMesh->SetupAttachment(SceneRoot);
	FeatureRibbonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FeatureRibbonMesh->SetHiddenInGame(true);
	FeatureRibbonMesh->SetVisibility(false);

	SelectionReticle = CreateDefaultSubobject<UArrowComponent>(TEXT("SelectionReticle"));
	SelectionReticle->SetupAttachment(SceneRoot);
	SelectionReticle->SetHiddenInGame(true);
	SelectionReticle->ArrowSize = 9.f;
	SelectionReticle->ArrowColor = FColor(0, 255, 255);
}

void AIH_WB_IslandActor::BeginPlay()
{
	Super::BeginPlay();
	RegisterCollision();
	RefreshMinimapCoastline();
}

void AIH_WB_IslandActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterCollision();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			Minimap->UnregisterCoastlineForIsland(TankIslandIndex);
			Minimap->UnregisterSeaRootsExtentForIsland(TankIslandIndex);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AIH_WB_IslandActor::SetAslContourRibbonBakeDeferred(bool bDeferred)
{
	bAslContourRibbonBakeDeferred = bDeferred;
}

bool AIH_WB_IslandActor::IsAslContourRibbonBakeDeferred()
{
	return bAslContourRibbonBakeDeferred;
}

void AIH_WB_IslandActor::FlushDeferredAslContourRibbonBake()
{
	// Contours default OFF — defer heavy bake until the checkbox is checked (lazy).
	if (IHInvisibleHandSpec::IsDevDemoAslContourLinesEnabled())
	{
		EnsureAslContourRibbonsBaked();
		ApplyDevContoursVisibility(true);
	}
}

void AIH_WB_IslandActor::ApplyDevContoursVisibility(bool bVisible)
{
	if (bVisible)
	{
		EnsureAslContourRibbonsBaked();
	}
	if (ContourRibbonMesh)
	{
		ContourRibbonMesh->SetVisibility(bVisible, true);
		ContourRibbonMesh->SetHiddenInGame(!bVisible, true);
	}
}

void AIH_WB_IslandActor::ApplyDevFeaturesVisibility(bool bVisible)
{
	if (bVisible)
	{
		EnsureFeatureRibbonsBaked();
	}
	if (FeatureRibbonMesh)
	{
		FeatureRibbonMesh->SetVisibility(bVisible, true);
		FeatureRibbonMesh->SetHiddenInGame(!bVisible, true);
	}
}

void AIH_WB_IslandActor::ApplyDevGrabContrastMaterials(const bool bGrabContrast)
{
	if (!IslandMesh)
	{
		return;
	}
	const float AlbedoScale = bGrabContrast
		? IHInvisibleHandSpec::TopographyGrabContrastAlbedoScale
		: 1.f;
	const float Roughness = bGrabContrast ? 0.96f : 0.92f;
	const float Specular = bGrabContrast ? 0.06f : 0.10f;
	static const FName ColorNames[] = {
		FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
	};
	const int32 NumSections = IslandMesh->GetNumSections();
	for (int32 Section = 0; Section < NumSections; ++Section)
	{
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(IslandMesh->GetMaterial(Section));
		if (!MID)
		{
			continue;
		}
		FLinearColor CurColor = FLinearColor::White;
		MID->GetVectorParameterValue(FName(TEXT("Color")), CurColor);
		int32 BestTier = 0;
		float BestDist = TNumericLimits<float>::Max();
		for (int32 Tier = 0; Tier < IHInvisibleHandSpec::TopographyTierCount; ++Tier)
		{
			const FLinearColor Base = IHInvisibleHandSpec::GetTopographyTierDisplayColor(Tier);
			const FLinearColor Scaled(
				Base.R * IHInvisibleHandSpec::TopographyGrabContrastAlbedoScale,
				Base.G * IHInvisibleHandSpec::TopographyGrabContrastAlbedoScale,
				Base.B * IHInvisibleHandSpec::TopographyGrabContrastAlbedoScale,
				Base.A);
			const float DistBase = FMath::Abs(CurColor.R - Base.R) + FMath::Abs(CurColor.G - Base.G)
				+ FMath::Abs(CurColor.B - Base.B);
			const float DistScaled = FMath::Abs(CurColor.R - Scaled.R) + FMath::Abs(CurColor.G - Scaled.G)
				+ FMath::Abs(CurColor.B - Scaled.B);
			const float Dist = FMath::Min(DistBase, DistScaled);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				BestTier = Tier;
			}
		}
		const FLinearColor TierColor = IHInvisibleHandSpec::GetTopographyTierDisplayColor(BestTier);
		const FLinearColor DisplayColor(
			TierColor.R * AlbedoScale,
			TierColor.G * AlbedoScale,
			TierColor.B * AlbedoScale,
			TierColor.A);
		for (const FName& N : ColorNames)
		{
			MID->SetVectorParameterValue(N, DisplayColor);
		}
		MID->SetScalarParameterValue(FName(TEXT("Roughness")), Roughness);
		MID->SetScalarParameterValue(FName(TEXT("Specular")), Specular);
	}
}

void AIH_WB_IslandActor::EnsureAslContourRibbonsBaked()
{
	if (bAslContourRibbonsBaked)
	{
		return;
	}
	BakeAslContourRibbons();
}

void AIH_WB_IslandActor::BakeAslContourRibbons()
{
	if (!ContourRibbonMesh)
	{
		return;
	}

	ContourRibbonMesh->ClearAllMeshSections();
	bAslContourRibbonsBaked = false;

	constexpr float RibbonLiftCm = 400.f; // 4 m above max(surface, isoline) — readable at fly ASL
	constexpr float WhiteRibbonLiftCm = 550.f; // slightly higher so +25 inland bands stay visible

	auto SampleHeightCmAtLocal = [this](const FVector2D& LocalCm) -> float
	{
		if (HeightsMeters.Num() == 0 || SamplesPerSide < 2 || SampleSpacingMeters <= 0.0)
		{
			return 0.f;
		}
		const double Xm = LocalCm.X * 0.01;
		const double Ym = LocalCm.Y * 0.01;
		const double Fx = (Xm + HalfExtentMeters) / SampleSpacingMeters;
		const double Fy = (Ym + HalfExtentMeters) / SampleSpacingMeters;
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fx)), 0, SamplesPerSide - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fy)), 0, SamplesPerSide - 1);
		const int32 X1 = FMath::Min(X0 + 1, SamplesPerSide - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, SamplesPerSide - 1);
		const float Tx = static_cast<float>(Fx - X0);
		const float Ty = static_cast<float>(Fy - Y0);
		const float H00 = HeightsMeters[Y0 * SamplesPerSide + X0];
		const float H10 = HeightsMeters[Y0 * SamplesPerSide + X1];
		const float H01 = HeightsMeters[Y1 * SamplesPerSide + X0];
		const float H11 = HeightsMeters[Y1 * SamplesPerSide + X1];
		const float Hx0 = FMath::Lerp(H00, H10, Tx);
		const float Hx1 = FMath::Lerp(H01, H11, Tx);
		return FMath::Lerp(Hx0, Hx1, Ty) * 100.f;
	};

	auto RibbonZ = [&](const FVector2D& LocalCm, const float IsolineZCm, const float LiftCm) -> float
	{
		return FMath::Max(SampleHeightCmAtLocal(LocalCm), IsolineZCm) + LiftCm;
	};

	auto AppendRibbon = [&](
		const TArray<FVector2D>& PolyLocalCm,
		const float IsolineZCm,
		const float HalfWidthCm,
		const float LiftCm,
		TArray<FVector>& OutVerts,
		TArray<int32>& OutTris,
		TArray<FVector>& OutNormals,
		TArray<FVector2D>& OutUVs,
		TArray<FColor>& OutColors,
		TArray<FProcMeshTangent>& OutTangents,
		const FColor VertColor,
		const bool bFollowHeightfield = true)
	{
		const int32 NumPts = PolyLocalCm.Num();
		if (NumPts < 2)
		{
			return;
		}
		const bool bClosed = NumPts >= 3
			&& FVector2D::DistSquared(PolyLocalCm[0], PolyLocalCm[NumPts - 1]) < FMath::Square(50.f);
		const int32 SegCount = bClosed ? NumPts : (NumPts - 1);
		const float FlatZCm = IsolineZCm + LiftCm;
		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVector2D A = PolyLocalCm[i];
			const FVector2D B = PolyLocalCm[(i + 1) % NumPts];
			FVector2D Dir = B - A;
			const float Len = Dir.Size();
			if (Len < 1.f)
			{
				continue;
			}
			Dir /= Len;
			const FVector2D Perp(-Dir.Y * HalfWidthCm, Dir.X * HalfWidthCm);
			const float ZA = bFollowHeightfield ? RibbonZ(A, IsolineZCm, LiftCm) : FlatZCm;
			const float ZB = bFollowHeightfield ? RibbonZ(B, IsolineZCm, LiftCm) : FlatZCm;
			const int32 Base = OutVerts.Num();
			OutVerts.Add(FVector(A.X + Perp.X, A.Y + Perp.Y, ZA));
			OutVerts.Add(FVector(A.X - Perp.X, A.Y - Perp.Y, ZA));
			OutVerts.Add(FVector(B.X + Perp.X, B.Y + Perp.Y, ZB));
			OutVerts.Add(FVector(B.X - Perp.X, B.Y - Perp.Y, ZB));
			OutNormals.Add(FVector::UpVector);
			OutNormals.Add(FVector::UpVector);
			OutNormals.Add(FVector::UpVector);
			OutNormals.Add(FVector::UpVector);
			OutUVs.Add(FVector2D(0.f, 0.f));
			OutUVs.Add(FVector2D(0.f, 1.f));
			OutUVs.Add(FVector2D(1.f, 0.f));
			OutUVs.Add(FVector2D(1.f, 1.f));
			OutColors.Add(VertColor);
			OutColors.Add(VertColor);
			OutColors.Add(VertColor);
			OutColors.Add(VertColor);
			OutTangents.Add(FProcMeshTangent(Dir.X, Dir.Y, 0.f));
			OutTangents.Add(FProcMeshTangent(Dir.X, Dir.Y, 0.f));
			OutTangents.Add(FProcMeshTangent(Dir.X, Dir.Y, 0.f));
			OutTangents.Add(FProcMeshTangent(Dir.X, Dir.Y, 0.f));
			OutTris.Append({ Base, Base + 2, Base + 1, Base + 1, Base + 2, Base + 3 });
			OutTris.Append({ Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2 });
		}
	};

	auto AppendAllRings = [&](
		const TArray<TArray<FVector2D>>& Rings,
		const TArray<FVector2D>& FallbackLargest,
		const float IsolineZCm,
		const float HalfWidthCm,
		const float LiftCm,
		TArray<FVector>& OutVerts,
		TArray<int32>& OutTris,
		TArray<FVector>& OutNormals,
		TArray<FVector2D>& OutUVs,
		TArray<FColor>& OutColors,
		TArray<FProcMeshTangent>& OutTangents,
		const FColor VertColor,
		const bool bFollowHeightfield = true) -> int32
	{
		int32 PointTotal = 0;
		if (Rings.Num() > 0)
		{
			for (const TArray<FVector2D>& Ring : Rings)
			{
				AppendRibbon(
					Ring, IsolineZCm, HalfWidthCm, LiftCm,
					OutVerts, OutTris, OutNormals, OutUVs, OutColors, OutTangents, VertColor,
					bFollowHeightfield);
				PointTotal += Ring.Num();
			}
		}
		else
		{
			AppendRibbon(
				FallbackLargest, IsolineZCm, HalfWidthCm, LiftCm,
				OutVerts, OutTris, OutNormals, OutUVs, OutColors, OutTangents, VertColor,
				bFollowHeightfield);
			PointTotal = FallbackLargest.Num();
		}
		return PointTotal;
	};

	const float GoldHalf = IHInvisibleHandSpec::DevDemo_AslContourRibbonGoldThicknessCm * 0.5f;
	const float MagHalf = IHInvisibleHandSpec::DevDemo_AslContourRibbonMagentaThicknessCm * 0.5f;

	int32 GoldPts = 0;
	int32 ShelfPts = 0;
	int32 Plus25Pts = 0;

	// Section 0 — gold ASL 0 waterline (all significant rings; MainCoast = rings[0])
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		GoldPts = AppendAllRings(
			ContourGoldRingsLocalCm, MainCoastPolylineLocalCm, 0.f, GoldHalf, RibbonLiftCm,
			Verts, Tris, Normals, UVs, Colors, Tangents,
			IHInvisibleHandSpec::DevDemo_AslContourGoldColor);
		if (Verts.Num() > 0)
		{
			ContourRibbonMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
			if (UMaterialInstanceDynamic* GoldMID = IH_WB_IslandActorPrivate::MakeOpaqueRibbonMID(
					this, FLinearColor(IHInvisibleHandSpec::DevDemo_AslContourGoldColor)))
			{
				ContourRibbonMesh->SetMaterial(0, GoldMID);
			}
		}
	}

	// Section 1 — magenta gold-governed WWF rim (flat Z at ShelfFloor+lift; no HF climb)
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		const float MagentaIsolineZ = IHInvisibleHandSpec::ShelfFloorMeters * 100.f;
		ShelfPts = AppendAllRings(
			ContourGovernedWwfRingsLocalCm, GovernedWwfOuterLocalCm, MagentaIsolineZ, MagHalf, RibbonLiftCm,
			Verts, Tris, Normals, UVs, Colors, Tangents,
			IHInvisibleHandSpec::DevDemo_AslContourShelfMagentaColor,
			/*bFollowHeightfield=*/false);
		if (Verts.Num() > 0)
		{
			ContourRibbonMesh->CreateMeshSection(1, Verts, Tris, Normals, UVs, Colors, Tangents, false);
			if (UMaterialInstanceDynamic* MagMID = IH_WB_IslandActorPrivate::MakeOpaqueRibbonMID(
					this, FLinearColor(IHInvisibleHandSpec::DevDemo_AslContourShelfMagentaColor)))
			{
				ContourRibbonMesh->SetMaterial(1, MagMID);
			}
		}
	}

	// Section 2 — white +25 m dry ASL band (all significant rings)
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		const float Plus25IsolineZ = IHInvisibleHandSpec::DevDemo_AslContourPlus25Meters * 100.f;
		const float Plus25Half =
			IHInvisibleHandSpec::DevDemo_AslContourRibbonPlus25ThicknessCm * 0.5f;
		Plus25Pts = AppendAllRings(
			ContourPlus25RingsLocalCm, Plus25PolylineLocalCm, Plus25IsolineZ, Plus25Half,
			WhiteRibbonLiftCm,
			Verts, Tris, Normals, UVs, Colors, Tangents,
			IHInvisibleHandSpec::DevDemo_AslContourWhiteColor);
		if (Verts.Num() > 0)
		{
			ContourRibbonMesh->CreateMeshSection(2, Verts, Tris, Normals, UVs, Colors, Tangents, false);
			if (UMaterialInstanceDynamic* WhiteMID = IH_WB_IslandActorPrivate::MakeOpaqueRibbonMID(
					this, FLinearColor(IHInvisibleHandSpec::DevDemo_AslContourWhiteColor)))
			{
				ContourRibbonMesh->SetMaterial(2, WhiteMID);
			}
		}
	}

	bAslContourRibbonsBaked = ContourRibbonMesh->GetNumSections() > 0;
	ContourRibbonMesh->SetCastShadow(false);

	const int32 GoldRings = ContourGoldRingsLocalCm.Num() > 0
		? ContourGoldRingsLocalCm.Num()
		: (MainCoastPolylineLocalCm.Num() >= 2 ? 1 : 0);
	const int32 ShelfRings = ContourGovernedWwfRingsLocalCm.Num() > 0
		? ContourGovernedWwfRingsLocalCm.Num()
		: (GovernedWwfOuterLocalCm.Num() >= 2 ? 1 : 0);
	const int32 Plus25Rings = ContourPlus25RingsLocalCm.Num() > 0
		? ContourPlus25RingsLocalCm.Num()
		: (Plus25PolylineLocalCm.Num() >= 2 ? 1 : 0);
	const int32 MainCoastPts = MainCoastPolylineLocalCm.Num();

	UE_LOG(LogTemp, Log,
		TEXT("IH_WB_IslandActor: ASL contour ribbons island=%d goldPts=%d goldRings=%d mainCoastPts=%d shelfPts=%d shelfRings=%d plus25Pts=%d plus25Rings=%d sections=%d"),
		TankIslandIndex, GoldPts, GoldRings, MainCoastPts, ShelfPts, ShelfRings, Plus25Pts, Plus25Rings,
		ContourRibbonMesh->GetNumSections());
}

void AIH_WB_IslandActor::EnsureFeatureRibbonsBaked()
{
	if (bFeatureRibbonsBaked)
	{
		return;
	}
	BakeFeatureRibbons();
}

void AIH_WB_IslandActor::BakeFeatureRibbons()
{
	if (!FeatureRibbonMesh)
	{
		return;
	}
	FeatureRibbonMesh->ClearAllMeshSections();
	bFeatureRibbonsBaked = false;
	if (MainCoastPolylineLocalCm.Num() < 8 || CoastCharacterRing.Num() < 8)
	{
		return;
	}

	constexpr float RibbonLiftCm = 500.f; // sit above Contours lift
	auto SampleHeightCmAtLocal = [this](const FVector2D& LocalCm) -> float
	{
		if (HeightsMeters.Num() == 0 || SamplesPerSide < 2 || SampleSpacingMeters <= 0.0)
		{
			return 0.f;
		}
		const double Xm = LocalCm.X * 0.01;
		const double Ym = LocalCm.Y * 0.01;
		const double Fx = (Xm + HalfExtentMeters) / SampleSpacingMeters;
		const double Fy = (Ym + HalfExtentMeters) / SampleSpacingMeters;
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fx)), 0, SamplesPerSide - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fy)), 0, SamplesPerSide - 1);
		const int32 X1 = FMath::Min(X0 + 1, SamplesPerSide - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, SamplesPerSide - 1);
		const float Tx = static_cast<float>(Fx - X0);
		const float Ty = static_cast<float>(Fy - Y0);
		const float H00 = HeightsMeters[Y0 * SamplesPerSide + X0];
		const float H10 = HeightsMeters[Y0 * SamplesPerSide + X1];
		const float H01 = HeightsMeters[Y1 * SamplesPerSide + X0];
		const float H11 = HeightsMeters[Y1 * SamplesPerSide + X1];
		const float Hx0 = FMath::Lerp(H00, H10, Tx);
		const float Hx1 = FMath::Lerp(H01, H11, Tx);
		return FMath::Lerp(Hx0, Hx1, Ty) * 100.f;
	};

	auto AppendRibbon = [&](
		const TArray<FVector2D>& PolyLocalCm,
		const float HalfWidthCm,
		TArray<FVector>& OutVerts,
		TArray<int32>& OutTris,
		TArray<FVector>& OutNormals,
		TArray<FVector2D>& OutUVs,
		TArray<FColor>& OutColors,
		TArray<FProcMeshTangent>& OutTangents,
		const FColor VertColor)
	{
		const int32 NumPts = PolyLocalCm.Num();
		if (NumPts < 2) return;
		// Class runs are open arcs along MainCoast — never close-wrap (would chord across gaps).
		const int32 SegCount = NumPts - 1;
		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVector2D A = PolyLocalCm[i];
			const FVector2D B = PolyLocalCm[i + 1];
			FVector2D Dir = B - A;
			const float Len = Dir.Size();
			if (Len < 1.f) continue;
			Dir /= Len;
			const FVector2D Perp(-Dir.Y * HalfWidthCm, Dir.X * HalfWidthCm);
			const float ZA = FMath::Max(SampleHeightCmAtLocal(A), 0.f) + RibbonLiftCm;
			const float ZB = FMath::Max(SampleHeightCmAtLocal(B), 0.f) + RibbonLiftCm;
			const int32 Base = OutVerts.Num();
			OutVerts.Add(FVector(A.X + Perp.X, A.Y + Perp.Y, ZA));
			OutVerts.Add(FVector(A.X - Perp.X, A.Y - Perp.Y, ZA));
			OutVerts.Add(FVector(B.X + Perp.X, B.Y + Perp.Y, ZB));
			OutVerts.Add(FVector(B.X - Perp.X, B.Y - Perp.Y, ZB));
			for (int32 k = 0; k < 4; ++k)
			{
				OutNormals.Add(FVector::UpVector);
				OutColors.Add(VertColor);
				OutTangents.Add(FProcMeshTangent(Dir.X, Dir.Y, 0.f));
			}
			OutUVs.Add(FVector2D(0.f, 0.f));
			OutUVs.Add(FVector2D(0.f, 1.f));
			OutUVs.Add(FVector2D(1.f, 0.f));
			OutUVs.Add(FVector2D(1.f, 1.f));
			OutTris.Append({ Base, Base + 2, Base + 1, Base + 1, Base + 2, Base + 3 });
			OutTris.Append({ Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2 });
		}
	};

	const int32 RingN = CoastCharacterRing.Num();
	auto ClassAtCoastPoint = [&](const FVector2D& LocalCm) -> EIHCoastCharacterClass
	{
		const double A = FMath::Atan2(LocalCm.Y, LocalCm.X);
		double T = (A + PI) / UE_TWO_PI;
		T = FMath::Clamp(T, 0.0, 0.999999);
		const int32 Idx = FMath::Clamp(FMath::FloorToInt(static_cast<float>(T * RingN)), 0, RingN - 1);
		EIHCoastCharacterClass Cls = static_cast<EIHCoastCharacterClass>(CoastCharacterRing[Idx]);

		// Local slope override: Beach/Gentle Feature on residual sheer faces → Bluff reporter.
		// Grab1–4: tighten so yellow Beach ribbon does not paint bluff/cliff waterlines
		// (TheaterYellow = walkable Beach pad, not any yellow stroke). Do NOT flatten HF.
		if ((Cls == EIHCoastCharacterClass::Beach || Cls == EIHCoastCharacterClass::Gentle)
			&& HeightsMeters.Num() >= SamplesPerSide * SamplesPerSide
			&& SamplesPerSide >= 2 && SampleSpacingMeters > 0.0)
		{
			const float Len = LocalCm.Size();
			if (Len > 1.f)
			{
				const float SpacingCm = static_cast<float>(SampleSpacingMeters * 100.0);
				const float ZCoast = SampleHeightCmAtLocal(LocalCm);
				float MaxSlope = 0.f;
				float MaxRiseM = 0.f;
				// Wider inland sample + along-coast neighbors catch sheer under Beach class.
				for (const float CellsIn : {1.0f, 2.0f, 3.0f, 4.0f})
				{
					const float InDist = SpacingCm * CellsIn;
					if (InDist >= Len)
					{
						continue;
					}
					const FVector2D Inward = LocalCm * ((Len - InDist) / Len);
					const float ZIn = SampleHeightCmAtLocal(Inward);
					const float RiseM = (ZIn - ZCoast) * 0.01f;
					const float RunM = static_cast<float>(SampleSpacingMeters) * CellsIn;
					MaxSlope = FMath::Max(MaxSlope, RiseM / FMath::Max(RunM, 1.f));
					MaxRiseM = FMath::Max(MaxRiseM, RiseM);
				}
				const FVector2D Tang(-LocalCm.Y, LocalCm.X);
				const float TangLen = Tang.Size();
				if (TangLen > KINDA_SMALL_NUMBER)
				{
					const FVector2D TDir = Tang / TangLen;
					for (const float SideSign : {-1.f, 1.f})
					{
						const FVector2D SideP = LocalCm + TDir * (SpacingCm * 1.5f * SideSign);
						const float ZSide = SampleHeightCmAtLocal(SideP);
						const float RiseM = (ZSide - ZCoast) * 0.01f;
						MaxRiseM = FMath::Max(MaxRiseM, FMath::Abs(RiseM));
						MaxSlope = FMath::Max(MaxSlope, FMath::Abs(RiseM) / FMath::Max(static_cast<float>(SampleSpacingMeters) * 1.5f, 1.f));
					}
				}
				// ~tan(6°) — tighter than prior tan(8°) so bluff faces lose yellow Beach paint.
				if (MaxSlope > 0.105f || MaxRiseM > 3.5f)
				{
					Cls = EIHCoastCharacterClass::Bluff;
				}
			}
		}
		return Cls;
	};

	// One open polyline per contiguous class run — never concatenate with gap markers
	// (gap markers still connected end-of-A → start-of-B as inlet chords in AppendRibbon).
	TArray<TArray<FVector2D>> BeachRuns;
	TArray<TArray<FVector2D>> GentleRuns;
	TArray<TArray<FVector2D>> BluffRuns;

	auto FlushRun = [](TArray<TArray<FVector2D>>& DestRuns, TArray<FVector2D>& Run)
	{
		if (Run.Num() >= 2)
		{
			DestRuns.Add(Run);
		}
		Run.Reset();
	};

	// Class runs sample post-Chaikin MainCoast (bake-time SmoothRingLocalCm ×2 @0.22).
	// CoastCharacterRing stays 256-bin azimuth LUT — not MS vertex-index locked.
	TArray<FVector2D> Run;
	EIHCoastCharacterClass Prev = EIHCoastCharacterClass::Gentle;
	bool bHavePrev = false;
	for (const FVector2D& P : MainCoastPolylineLocalCm)
	{
		const EIHCoastCharacterClass C = ClassAtCoastPoint(P);
		if (!bHavePrev || C != Prev)
		{
			if (bHavePrev)
			{
				if (Prev == EIHCoastCharacterClass::Beach) FlushRun(BeachRuns, Run);
				else if (Prev == EIHCoastCharacterClass::Bluff) FlushRun(BluffRuns, Run);
				else FlushRun(GentleRuns, Run);
			}
			Prev = C;
			bHavePrev = true;
			Run.Reset();
		}
		Run.Add(P);
	}
	if (bHavePrev)
	{
		if (Prev == EIHCoastCharacterClass::Beach) FlushRun(BeachRuns, Run);
		else if (Prev == EIHCoastCharacterClass::Bluff) FlushRun(BluffRuns, Run);
		else FlushRun(GentleRuns, Run);
	}

	const float HalfW = 900.f;
	const FColor BeachCol(0xFF, 0xD2, 0x4A);
	const FColor GentleCol(0x2E, 0xC4, 0xB6);
	const FColor BluffCol(0xE9, 0x1E, 0x63);

	auto CountPts = [](const TArray<TArray<FVector2D>>& Runs) -> int32
	{
		int32 N = 0;
		for (const TArray<FVector2D>& R : Runs)
		{
			N += R.Num();
		}
		return N;
	};

	auto BakeClassRuns = [&](const TArray<TArray<FVector2D>>& Runs, const int32 Section, const FColor Col)
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		for (const TArray<FVector2D>& Ring : Runs)
		{
			AppendRibbon(Ring, HalfW, Verts, Tris, Normals, UVs, Colors, Tangents, Col);
		}
		if (Verts.Num() == 0)
		{
			return;
		}
		FeatureRibbonMesh->CreateMeshSection(Section, Verts, Tris, Normals, UVs, Colors, Tangents, false);
		if (UMaterialInstanceDynamic* MID = IH_WB_IslandActorPrivate::MakeOpaqueRibbonMID(this, FLinearColor(Col)))
		{
			FeatureRibbonMesh->SetMaterial(Section, MID);
		}
	};

	BakeClassRuns(BeachRuns, 0, BeachCol);
	BakeClassRuns(GentleRuns, 1, GentleCol);
	BakeClassRuns(BluffRuns, 2, BluffCol);

	bFeatureRibbonsBaked = FeatureRibbonMesh->GetNumSections() > 0;
	FeatureRibbonMesh->SetCastShadow(false);

	UE_LOG(LogTemp, Log,
		TEXT("IH_WB_IslandActor: Feature ribbons island=%d beachPts=%d beachRuns=%d gentlePts=%d gentleRuns=%d bluffPts=%d bluffRuns=%d sections=%d"),
		TankIslandIndex,
		CountPts(BeachRuns), BeachRuns.Num(),
		CountPts(GentleRuns), GentleRuns.Num(),
		CountPts(BluffRuns), BluffRuns.Num(),
		FeatureRibbonMesh->GetNumSections());
}

void AIH_WB_IslandActor::RefreshIslandActorTickEnabled()
{
	SetActorTickEnabled(false);
}

void AIH_WB_IslandActor::SetSelectionHighlighted(bool bHighlighted)
{
	bSelectionHighlighted = bHighlighted;
	if (SelectionReticle)
	{
		SelectionReticle->SetHiddenInGame(!bHighlighted);
	}
	if (IslandMesh)
	{
		IslandMesh->SetRenderCustomDepth(bHighlighted);
		IslandMesh->SetCustomDepthStencilValue(bHighlighted ? 1 : 0);
	}
	if (ShelfMesh)
	{
		ShelfMesh->SetRenderCustomDepth(bHighlighted);
		ShelfMesh->SetCustomDepthStencilValue(bHighlighted ? 2 : 0);
	}
}

void AIH_WB_IslandActor::GetWaterlineFootprintCm(float& OutSemiMajorCm, float& OutSemiMinorCm) const
{
	OutSemiMajorCm = SemiMajorAxisCm;
	OutSemiMinorCm = SemiMajorAxisCm / 1.6180339887f;
}

void AIH_WB_IslandActor::GetShorelinePolygonWorldCm(TArray<FVector2D>& OutWorldCm) const
{
	OutWorldCm.Reset();
	OutWorldCm.Reserve(MainCoastPolylineLocalCm.Num());
	for (const FVector2D& Local : MainCoastPolylineLocalCm)
	{
		OutWorldCm.Add(LocalCmToWorldCm(Local));
	}
}

void AIH_WB_IslandActor::GetSelectionRingWorldCm(TArray<FVector2D>& OutWorldCm) const
{
	GetShorelinePolygonWorldCm(OutWorldCm);
	constexpr int32 Stride = 4;
	if (OutWorldCm.Num() > 64)
	{
		TArray<FVector2D> Decimated;
		for (int32 i = 0; i < OutWorldCm.Num(); i += Stride)
		{
			Decimated.Add(OutWorldCm[i]);
		}
		OutWorldCm = MoveTemp(Decimated);
	}
}

FVector2D AIH_WB_IslandActor::LocalCmToWorldCm(const FVector2D& LocalCm) const
{
	const FVector World = GetActorTransform().TransformPosition(FVector(LocalCm.X, LocalCm.Y, 0.f));
	return FVector2D(World.X, World.Y);
}

void AIH_WB_IslandActor::ApplyTankLayout(int32 InTankIslandIndex, float InSemiMajorAxisCm, float InSummitTopZCm, float InAreaKm2, int32 MasterSeed)
{
	ApplyTankLayout(InTankIslandIndex, InSemiMajorAxisCm, InSummitTopZCm, InAreaKm2, MasterSeed,
		EIHIslandProfile::Low);
}

void AIH_WB_IslandActor::ApplyTankLayout(int32 InTankIslandIndex, float InSemiMajorAxisCm, float InSummitTopZCm, float InAreaKm2, int32 MasterSeed, EIHIslandProfile Profile)
{
	TankIslandIndex = InTankIslandIndex;
	SemiMajorAxisCm = InSemiMajorAxisCm;
	SummitTopZCm = InSummitTopZCm;
	AreaKm2 = InAreaKm2;
	CachedCoastEnvelopeWorldCm = InSemiMajorAxisCm * 1.48f;
	CachedProfile = Profile;
	BuildMeshesFromCellGraph(MasterSeed);
	RegisterCollision();
	RefreshMinimapCoastline();
}

void AIH_WB_IslandActor::BuildMeshesFromCellGraph(int32 MasterSeed)
{
	const double StartSeconds = FPlatformTime::Seconds();

	// Box sized off this island's actual target dry acreage (AreaKm2, from the preserved RealmSeed
	// layout solve), not CachedCoastEnvelopeWorldCm - that constant is a much larger tank-layout
	// spacing envelope (SemiMajorAxisCm * 1.48, sized for shelf/neighbor-overlap margin), and using
	// it directly as the generation box produced a box orders of magnitude bigger than the island
	// itself (landFraction ~0.001 in the first headless check - the hill was a speck in empty sea).
	// TargetLandFraction ~= DefaultTargetEffectiveLandFraction so the box the diffusion hill has to
	// fill roughly matches the same land:sea ratio the realm layout already assumes.
	constexpr double TargetLandFractionForBox = 0.30;
	const double TargetAreaM2 = FMath::Max(1.0, static_cast<double>(AreaKm2)) * 1.0e6;
	const double BoxAreaM2 = TargetAreaM2 / TargetLandFractionForBox;
	const double BoxHalfSideCm = FMath::Sqrt(BoxAreaM2) * 100.0 * 0.5;

	FIHTerrainCellGraphGenerator::FBuildParams BuildParams;
	BuildParams.CenterLocalCm = FVector2D::ZeroVector;
	BuildParams.HalfExtentXCm = FMath::Clamp(BoxHalfSideCm, 15000.0, static_cast<double>(CachedCoastEnvelopeWorldCm));
	BuildParams.HalfExtentYCm = BuildParams.HalfExtentXCm;
	BuildParams.TargetCellWidthCm = 7500.0; // 75 m - Cove/Harborage-throat resolution (IH canon table)
	BuildParams.MasterSeed = MasterSeed;
	BuildParams.IslandIndex = TankIslandIndex;

	FIHTerrainCellGraph Graph;
	if (!FIHTerrainCellGraphGenerator::BuildGraph(BuildParams, Graph) || Graph.Num() < 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("IH_WB_IslandActor: cell graph build failed island=%d"), TankIslandIndex);
		return;
	}

	FRandomStream Stream(MasterSeed ^ static_cast<int32>(TankIslandIndex * 2654435761u));

	// Reverted the multi-hill restructuring attempted here (plan Addendum 1, Bug 3b): it produced
	// an unstable, non-monotonic result across the 3 real ABBEY3 islands in the very next headless
	// check - island 0 (26 overlapping hills) landed at landFraction=0.977 (a nearly-solid box,
	// elapsedS=10.6 - a real perf hit too), island 1 (16 hills) at a reasonable 0.147, island 2
	// (10 hills) at a thin 0.051 - additive overlapping hills cross an apparent percolation
	// threshold once density gets high enough, making this recipe unpredictable to tune. Back to
	// the single-dominant-hill approach (analytically BlobPower-scaled by THIS graph's own hop
	// radius, plan Addendum 1's original fix) - the fragmentation problem (Bug 3a) is addressed
	// separately below via a stronger Smooth pass, isolated from this change so its effect can be
	// judged on its own instead of conflated with an unstable hill-count change.
	const double HopRadius = FMath::Max(1.0, FMath::Sqrt(static_cast<double>(Graph.Num())) * 0.5);
	constexpr double ReferenceHopRadius = 20.0;
	// Density-scaling fallout (2026-09-03), real mechanistic root cause of the landFraction
	// blowup (0.70-1.00 at 512,000 acres) - confirmed via DiffuseFromSeeds's own termination
	// condition (IHTerrainCellDiffusion.cpp, "if (Abs(CurChange) <= 1.0) continue"). At this
	// island's real HopRadius (392 for the largest ABBEY3 island, vs ReferenceHopRadius=20), the
	// UNCLAMPED formula already approaches 1.0 (near-zero decay); the OLD clamp ceiling (0.997)
	// still lets a SeedHeight=70 hill take ~1362 hops to decay below the diffusion's own stop
	// threshold - over 3.4x this island's own HopRadius. Every hill floods nearly the whole graph
	// before it can decay, regardless of hill COUNT - a prior, already-documented incident hit the
	// identical symptom from a different trigger ("26 overlapping hills... landFraction=0.977, a
	// nearly-solid box," comment above).
	//
	// 2026-09-04 (Track C, small-island root cause): the 0.98/0.978 ceiling above (self-test-driven,
	// not derived) fixed the ORIGINAL large-island case but created the SAME failure at the small
	// end for a different reason - IH_DIAG SmallIslandCheck data (GIZMO7, 7 islands) showed
	// mainHillDecayHopsOverHopRadius climbing 0.52 -> 1.76 from largest to smallest island while
	// MainBlobPower stayed pinned at the flat 0.98 ceiling for most of that range (it only started
	// dropping below HopRadius~101, and even then far too gently to compensate - island6's ratio was
	// STILL worse than island5's despite BlobPower correctly decreasing). Root cause: hill/trough
	// SEED HEIGHTS were left as flat constants (not re-derived per island size the way BlobPower
	// itself was supposed to be), and decay hop count depends only LOGARITHMICALLY on seed height
	// (DiffuseFromSeeds's NextChange = Pow(CurChange, PowerExp) means hop count solves
	// SeedHeight^(PowerExp^n) = NearThreshold, i.e. n = ln(K)/ln(PowerExp) for
	// K = ln(NearThreshold)/ln(SeedHeight)) - so PowerExp, not SeedHeight, is the only lever with
	// enough leverage to hit a target ratio. Replaced the old heuristic clamp with the ANALYTICAL
	// solution for PowerExp that keeps mainHillDecayHops/HopRadius at a constant TargetDecayRatio
	// (0.5, matching island0's own already-PIE-praised "excellent... organic natural island look" -
	// this is a re-derivation, not a re-tuning, of the exact same behavior at the large end, while
	// scaling correctly all the way down instead of plateauing at a flat ceiling). NearThreshold
	// (1/0.9) is the same jitter-reach proxy validated against this session's own "~210 hops at
	// HopRadius=392" reference point (this formula gives ~188 there - same order of magnitude).
	// AccentBlobPower gets its own K from its own (smaller) seed height ceiling, same derivation.
	constexpr double TargetDecayRatio = 0.5;
	constexpr double DecayNearThreshold = 1.0 / 0.9;
	constexpr double MainSeedHeightForCalibration = 70.0; // largest MainBlobPower-driven seed height (primary + main-accent hills)
	constexpr double AccentSeedHeightForCalibration = 28.0; // largest AccentBlobPower-driven seed height
	const double LnDecayNearThreshold = FMath::Loge(DecayNearThreshold);
	const double LnMainK = FMath::Loge(LnDecayNearThreshold / FMath::Loge(MainSeedHeightForCalibration));
	const double LnAccentK = FMath::Loge(LnDecayNearThreshold / FMath::Loge(AccentSeedHeightForCalibration));
	const double MainBlobPower = FMath::Clamp(FMath::Exp(LnMainK / (TargetDecayRatio * HopRadius)), 0.80, 0.98);
	const double AccentBlobPower = FMath::Clamp(FMath::Exp(LnAccentK / (TargetDecayRatio * HopRadius)), 0.80, 0.978);

	// TEMPORARY diagnostic instrumentation (2026-09-04, IH-DEC pending "Track C"): the user asked
	// whether some island-generation parameter calibrated for large islands is carried forward
	// unchanged to small ones, "overwhelming" them. IH-DEC-070 made BlobPower (decay RATE) size-
	// relative via HopRadius above, but hill/trough SEED HEIGHTS (e.g. main hill 50-70, immediately
	// below) and LandThreshold=20.0 (further below) are still flat constants, never re-derived per
	// island size. Simulates DiffuseFromSeeds's own exact per-hop formula (IHTerrainCellDiffusion.cpp:
	// NextChange = Pow(Abs(CurChange), PowerExp), ignoring jitter for a deterministic estimate) for a
	// representative main-hill seed height, without touching that shared function at all, to test
	// this hypothesis with real per-island numbers before implementing any fix.
	{
		// Iterating x -> Pow(x, PowerExp) for 0<PowerExp<1, x>1 converges to exactly 1.0 only in the
		// limit - it mathematically never crosses a strict ">1.0" test in finite steps (confirmed by
		// a first attempt here hitting a 100,000-iteration cap with room to spare). The REAL
		// DiffuseFromSeeds only terminates because of its per-hop Jitter (0.9-1.1 range) occasionally
		// pushing a near-1 value below the threshold - approximate that without touching the shared
		// function or its RNG stream by stopping once Change is within the jitter's own downward
		// reach (<=1.0/0.9 = 1.111, the point where a single min-jitter draw could cross 1.0).
		// Cross-checked: at HopRadius=392/MainBlobPower=0.98 (this session's own documented "~210
		// hops to decay" reference point), this proxy gives ~188 hops - same order of magnitude,
		// good enough for a relative small-vs-large-island comparison, not claimed as exact.
		auto SimulateDecayHops = [](double SeedHeight, double PowerExp) -> int32
		{
			double Change = SeedHeight;
			int32 Hops = 0;
			constexpr double NearThreshold = 1.0 / 0.9;
			constexpr int32 MaxSimHops = 100000; // guaranteed termination regardless of PowerExp
			while (FMath::Abs(Change) > NearThreshold && Hops < MaxSimHops)
			{
				Change = FMath::Pow(FMath::Abs(Change), PowerExp);
				++Hops;
			}
			return Hops;
		};
		const int32 MainHillDecayHops = SimulateDecayHops(/*SeedHeight=*/70.0, MainBlobPower);
		const int32 AccentHillDecayHops = SimulateDecayHops(/*SeedHeight=*/28.0, AccentBlobPower);
		UE_LOG(LogTemp, Log,
			TEXT("IH_DIAG SmallIslandCheck island=%d cells=%d HopRadius=%.1f MainBlobPower=%.4f AccentBlobPower=%.4f mainHillDecayHops=%d mainHillDecayHopsOverHopRadius=%.2f accentHillDecayHops=%d accentHillDecayHopsOverHopRadius=%.2f"),
			TankIslandIndex, Graph.Num(), HopRadius, MainBlobPower, AccentBlobPower,
			MainHillDecayHops, MainHillDecayHops / HopRadius, AccentHillDecayHops, AccentHillDecayHops / HopRadius);
	}

	// Plan Addendum 5: golden-ratio scalene seed triangle, replacing the point-seeded circular
	// base shape. Side LENGTHS in ratio 1:phi:phi^2 are degenerate (1 + phi == phi^2 exactly, the
	// defining golden-ratio identity - a "triangle" with those side lengths has zero area). Using
	// the same 1:phi:phi^2 ratio on ANGLES instead avoids that: any three positive numbers sum to
	// a valid triangle once normalized to 180 degrees, so this is the correct way to honor "1:phi:
	// phi^2 golden-ratio proportions" for a genuinely scalene triangle. Works out to
	// approximately a 34/56/90-degree scalene right triangle.
	constexpr double Phi = 1.6180339887498949;
	const double AngleUnitSum = 1.0 + Phi + Phi * Phi;
	const double AngleA = (1.0 / AngleUnitSum) * PI;
	const double AngleB = (Phi / AngleUnitSum) * PI;
	// Law of sines: side length opposite an angle is proportional to sin(angle). Place vertex A at
	// the origin, vertex B along +X at distance SideC (=AB, opposite angle C), vertex C at
	// distance SideB (=AC, opposite angle B) and angle AngleA from the AB baseline.
	const double SideC = FMath::Sin(PI - AngleA - AngleB);
	const double SideB = FMath::Sin(AngleB);
	FVector2D TriVerts[3] = {
		FVector2D(0.0, 0.0),
		FVector2D(SideC, 0.0),
		FVector2D(SideB * FMath::Cos(AngleA), SideB * FMath::Sin(AngleA)),
	};
	const double RawLongestSide = FMath::Max3(
		FVector2D::Distance(TriVerts[0], TriVerts[1]),
		FVector2D::Distance(TriVerts[1], TriVerts[2]),
		FVector2D::Distance(TriVerts[2], TriVerts[0]));
	const double TargetLongestSideCm = FMath::Min(BuildParams.HalfExtentXCm, BuildParams.HalfExtentYCm) * 1.3;
	const double TriScale = RawLongestSide > KINDA_SMALL_NUMBER ? TargetLongestSideCm / RawLongestSide : 1.0;
	FVector2D TriCentroid = FVector2D::ZeroVector;
	for (FVector2D& V : TriVerts)
	{
		V *= TriScale;
		TriCentroid += V;
	}
	TriCentroid /= 3.0;
	// Independently randomized rotation per island (user requirement) - no two islands' triangles
	// point the same direction on one map.
	const double RotationRad = Stream.FRandRange(0.0, 2.0 * PI);
	const double CosR = FMath::Cos(RotationRad);
	const double SinR = FMath::Sin(RotationRad);
	for (FVector2D& V : TriVerts)
	{
		const FVector2D Centered = V - TriCentroid;
		V = FVector2D(Centered.X * CosR - Centered.Y * SinR, Centered.X * SinR + Centered.Y * CosR)
			+ BuildParams.CenterLocalCm;
	}

	auto PointInTriangle = [](const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C) -> bool
	{
		auto Sign3 = [](const FVector2D& P1, const FVector2D& P2, const FVector2D& P3) -> double
		{
			return (P1.X - P3.X) * (P2.Y - P3.Y) - (P2.X - P3.X) * (P1.Y - P3.Y);
		};
		const double D1 = Sign3(P, A, B);
		const double D2 = Sign3(P, B, C);
		const double D3 = Sign3(P, C, A);
		const bool bHasNeg = (D1 < 0.0) || (D2 < 0.0) || (D3 < 0.0);
		const bool bHasPos = (D1 > 0.0) || (D2 > 0.0) || (D3 > 0.0);
		return !(bHasNeg && bHasPos);
	};
	TArray<int32> TriangleCellIndices;
	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		if (PointInTriangle(Graph.Cells[i].SitePos, TriVerts[0], TriVerts[1], TriVerts[2]))
		{
			TriangleCellIndices.Add(i);
		}
	}
	// FindNearestCellIdx (found the cell nearest a triangle-vertex-derived point, e.g. TriVerts[0]/
	// [2]) removed alongside HIGH/VOLC (IH-DEC-064/069) - it had no other caller; Low seeds from
	// TriangleCellIndices directly, never needed nearest-point lookup.

	// HIGH/VOLC retired (IH-DEC-064/069) - the two-summit-ridgeline (High) and single-cone (Volc)
	// seeding that used to live here (TriVerts[0]/[2], the triangle's extreme corners) is gone;
	// only Low's interior-point seeding (TriangleCellIndices) remains, so the profile switch
	// collapses to a single unconditional block. The triangle CONSTRUCTION above (TriVerts,
	// TriangleCellIndices) stays - Low still needs it, it was never High/Volc-exclusive.
	//
	// Gentle slopes, broad central highlands (canon). Previously seeded the WHOLE triangle
	// interior at once - many co-seeded points collapsed the effective hop-radius the diffusion
	// needed to travel, so the coastline traced close to the triangle's own straight edges
	// instead of the organic, jitter-shaped look the user preferred (plan Addendum 6). Switched
	// to sparse point-seeds instead - 1 primary + a scaled number of accents drawn from within
	// the triangle, mirroring the original "1 main + N accent hills" pattern that produced the
	// liked circular look - this constrains seed POSITIONS to the triangle's asymmetric footprint
	// without filling it solid, so the multi-hop power-law jitter is what shapes the coastline.
	//
	// LOW-island density scaling (2026-09-03, recalibrated same day after a headless self-test
	// caught a real blowup): the fixed Stream.RandRange(2, 3) accent-hill count (unchanged since
	// Demo003) is what produced the "starburst" look once the realm grew from 128,000 to 512,000
	// acres - a few widely-separated hills each reaching a spike out from a mostly-empty center.
	// AreaScale reuses the same HopRadius/ReferenceHopRadius signal MainBlobPower/AccentBlobPower
	// already use above, SQUARED (not linear/sqrt) since HopRadius scales with island DIAMETER,
	// not area. First attempt capped this at 40 assuming ReferenceHopRadius=20 was calibrated
	// against a "128k-acre island" scale - wrong: at the real 512,000-acre realm's largest island
	// (614,656 cells), AreaScale itself came out to ~384, saturating that cap immediately and
	// producing enough hills to fill the ENTIRE sample box solid - all 3 self-tested islands
	// logged "cell graph produced no coastline" (zero Land/Ocean boundary inside the box at all).
	// Cap cut to 12 (from 40) - since the ratio saturates any reasonable multiplier at this scale
	// regardless of the formula's exact shape, the cap itself is the real control lever here, not
	// the exponent. Re-tested clean at 12 (see -RealmSeed= self-test log, same commit).
	if (TriangleCellIndices.Num() > 0)
	{
		const int32 PrimaryIdx = TriangleCellIndices[Stream.RandRange(0, TriangleCellIndices.Num() - 1)];
		FIHTerrainCellDiffusion::AddHillFromCellSet(
			Graph, { PrimaryIdx }, /*HeightMin=*/50.0, /*HeightMax=*/70.0, MainBlobPower, Stream);

		const double AreaScale = FMath::Square(HopRadius / ReferenceHopRadius);
		const int32 NumLowAccents = FMath::Clamp(
			FMath::RoundToInt(Stream.FRandRange(2.0, 3.0) * AreaScale), 2, 12);
		for (int32 AccentSeedIdx = 0; AccentSeedIdx < NumLowAccents; ++AccentSeedIdx)
		{
			const int32 AccentIdx = TriangleCellIndices[Stream.RandRange(0, TriangleCellIndices.Num() - 1)];
			FIHTerrainCellDiffusion::AddHillFromCellSet(
				Graph, { AccentIdx }, /*HeightMin=*/30.0, /*HeightMax=*/50.0, MainBlobPower, Stream);
		}
	}

	// Modest accent hills near the triangle for coastal irregularity/barrier-islet character,
	// independent of profile (matches the "barrier islets" look the user liked in earlier rounds).
	// Plan Addendum 14: Count 2 -> 4, a general islet-quantity lever (more independent diffusion
	// blobs = more islet candidates for the IH-DEC-020 budget filter to keep), separate from the
	// already-relaxed area/distance budget which governs size, not count.
	// 2026-08-18: Count 4 -> 6 was tried and self-tested via -RealmSeed=PRAMS5/ABBEY2 - reverted
	// (Count back to 4) after real data showed it isn't a reliable "more islets" lever: budget/
	// distance discards stayed at 0 in every test, but more accent-hill blobs simply increased the
	// chance a blob merges into the main landmass (or into another blob) instead of staying
	// separate - on 2 of 5 PRAMS5 islands this merged away EVERY existing islet (3->0, 1->0), even
	// though other islands gained many more (3->13, 2->5). Narrowing HeightMax 28->22 was also
	// tried and self-tested - roughly a wash (PRAMS5 total islets 8 vs. the 9-islet baseline,
	// still volatile per-island), reverted back to 28.
	// Attacking the actual mechanism instead: AddHill picks each of its Count seeds fully
	// independently, so two seeds landing close together (more likely as Count rises) is exactly
	// what causes their blobs to merge into one connected mass instead of staying separate islet
	// candidates. PickMinSpacedCellsInFractionalRange rejects/re-rolls a candidate seed within
	// MinAccentSeedSpacingCm of an already-picked one this call (bounded retries, safe fallback),
	// then AddHillFromCellSet diffuses each pre-spaced seed independently - same per-seed height
	// draw and diffusion behavior as AddHill itself, just with seed POSITIONS deliberately kept
	// apart first. Self-test before trusting this, same as every change this session.
	constexpr double MinAccentSeedSpacingCm = 45000.0; // 450m, 6x TargetCellWidthCm=7500 - comfortably
														// larger than a HeightMax=28 blob's typical
														// footprint above LandThreshold=20
	TArray<int32> AccentSeedIndices;
	FIHTerrainCellDiffusion::PickMinSpacedCellsInFractionalRange(
		Graph, /*Count=*/4, FVector2D(0.22, 0.78), FVector2D(0.22, 0.78),
		MinAccentSeedSpacingCm, Stream, AccentSeedIndices);
	for (const int32 AccentSeedCellIdx : AccentSeedIndices)
	{
		FIHTerrainCellDiffusion::AddHillFromCellSet(
			Graph, { AccentSeedCellIdx }, /*HeightMin=*/15.0, /*HeightMax=*/28.0, AccentBlobPower, Stream);
	}

	// Plan Addendum 2: primary troughs previously drew BOTH endpoints independently from
	// (0.05,0.95) - nearly the whole box - while the landmass sits within (0.35,0.65). A trough
	// spanning near-opposite corners routinely pathfound straight through the (already thin,
	// landFraction ~2-6%) landmass and severed it into 2-3 disconnected islands (confirmed via
	// Grab 6's wide shot). AddRange can't take independent start/end windows in one call, so use
	// AddRangeBetweenCells directly with two separately-drawn cells: Start near the landmass core
	// (coast-to-interior), End reaching outward - bounding each trough to a real inlet's length
	// instead of a corner-to-corner cut. Also reduced count/depth (3->2 primaries, 2->1 daughter
	// each, shallower HeightMin) so a still-thin landmass is less likely to be cut clean through
	// even where a trough does cross it - the land-fraction gap itself (Addendum 1, still open)
	// is deliberately left alone this round to avoid another unstable swing like the multi-hill
	// attempt that overshot to landFraction=0.977.
	// 2026-09-04 (IH-DEC pending): both windows below are centered on the graph's own AABB center
	// (0.32-0.68 and 0.10-0.90 both straddle 0.5), so the window itself is always symmetric about
	// GraphCenter regardless of RotationRad - rotating each CANDIDATE site by -RotationRad about
	// GraphCenter before the axis-aligned bounds test is exactly equivalent to testing against the
	// window rotated by +RotationRad in world space, with no change to the window's shape/bias
	// (still narrow-center-to-wide-edge, still avoids corner-to-corner cuts through thin land - see
	// the 2999-3010 comment above). RotationRad defaults to 0.0 (unchanged behavior) for every OTHER
	// caller of this lambda; only the primary-trough loop below passes a nonzero, per-trough random
	// value. Root cause this fixes: investigated fresh this session (user asked whether the
	// consistent cross-island trough angle traced to the golden-ratio scalene triangle - it does
	// NOT; the triangle gets its own independent per-island rotation and shares nothing but a
	// sequential FRandomStream with these windows). The real cause is that this window pair was
	// ALWAYS sampled in the graph's fixed local X/Y axes, never rotated - for two points drawn
	// independently from same-center axis-aligned squares, the difference-vector's angular density
	// is provably denser near the axis directions than the diagonals (a square domain isn't
	// azimuthally uniform the way a disk is), and since the frame never varied, that bias repeated
	// identically across every seed/island - exactly the "same angle every time" the user observed.
	auto PickRandomCellInFracWindow = [&Graph](const FVector2D& XFrac, const FVector2D& YFrac, FRandomStream& RandStream, double RotationRad = 0.0) -> int32
	{
		if (Graph.Num() == 0)
		{
			return INDEX_NONE;
		}
		const FVector2D BoundsSize = Graph.BoundsMaxLocalCm - Graph.BoundsMinLocalCm;
		const FVector2D GraphCenter = Graph.BoundsMinLocalCm + BoundsSize * 0.5;
		const FVector2D WindowMin = Graph.BoundsMinLocalCm + FVector2D(BoundsSize.X * XFrac.X, BoundsSize.Y * YFrac.X);
		const FVector2D WindowMax = Graph.BoundsMinLocalCm + FVector2D(BoundsSize.X * XFrac.Y, BoundsSize.Y * YFrac.Y);
		const double CosNegR = FMath::Cos(-RotationRad);
		const double SinNegR = FMath::Sin(-RotationRad);
		int32 BestIdx = INDEX_NONE;
		double BestDistSqCm = TNumericLimits<double>::Max();
		constexpr int32 MaxTries = 60;
		for (int32 Try = 0; Try < MaxTries; ++Try)
		{
			const int32 Idx = RandStream.RandRange(0, Graph.Num() - 1);
			const FVector2D& RawP = Graph.Cells[Idx].SitePos;
			FVector2D P = RawP;
			if (RotationRad != 0.0)
			{
				const FVector2D Local = RawP - GraphCenter;
				P = GraphCenter + FVector2D(Local.X * CosNegR - Local.Y * SinNegR, Local.X * SinNegR + Local.Y * CosNegR);
			}
			if (P.X >= WindowMin.X && P.X <= WindowMax.X && P.Y >= WindowMin.Y && P.Y <= WindowMax.Y)
			{
				return Idx;
			}
			const FVector2D Clamped(FMath::Clamp(P.X, WindowMin.X, WindowMax.X), FMath::Clamp(P.Y, WindowMin.Y, WindowMax.Y));
			const double DistSqCm = FVector2D::DistSquared(P, Clamped);
			if (DistSqCm < BestDistSqCm)
			{
				BestDistSqCm = DistSqCm;
				BestIdx = Idx;
			}
		}
		return BestIdx;
	};

	// LOW-island stabilization (2026-09-02), re-scaled (2026-09-03), then reverted again same day:
	// PickRandomCellInFracWindow below picks every primary trough's START from a narrow CENTER
	// window (0.32-0.68) and its END from a wide EDGE window (0.10-0.90) - troughs are center-to-
	// edge radial cuts BY CONSTRUCTION. Scaling their count up (2->10) didn't add nesting, it
	// directly multiplied the number of spokes - real PIE evidence: a persisting "spindly
	// starfish" look plus "conspicuous deep linear troughs," even after the hill-density fix
	// (below) independently resolved the original starburst diagnosis. Recalibrating the cap
	// (the fix that worked for hills) can't fix this - the geometry itself is radial, not a
	// density problem. Back to IH_WB_Demo003's original fixed count - richness comes from the
	// sub-inlet (coastal-window-biased, not radial) and daughter-trough (biased off a primary
	// trough's own path, not center-to-edge) layers below instead, neither of which shares this
	// radial-by-construction problem.
	constexpr int32 NumPrimaryTroughs = 2;
	TArray<TArray<FVector2D>> PrimaryTroughPaths;
	// Hoisted (round 3): shared by the primary-trough length cap below AND the sub-inlet length
	// targeting further down (Ask 2) - was previously recomputed per-primary-trough-iteration.
	const double IslandDiagonalCm = (Graph.BoundsMaxLocalCm - Graph.BoundsMinLocalCm).Size();

	// 2026-09-04 (round 4): CANON CORRECTION, see this session's own IH-DEC entry. Earlier rounds'
	// comments here claimed "continuous trough curvature is Terrain Stamps' job" as a standing
	// 2026-09-01 decision - checked directly against IH_Canonical_Decisions.md this round and that
	// is NOT what happened. IH-DEC-060 (2026-09-01) designed, tried, reverted TWICE for a real bug
	// (chaining independently-carved segments multiplied total carved depth and severed the
	// landmass), then SUCCESSFULLY FIXED AND SHIPPED the same day (commit 9a2638b): warp a single
	// reference path via FindPathOnly + sine-varying PickCellNearPath offsets, diffuse it ONCE via
	// DiffuseAlongCells (one height draw, one diffusion pass over the whole warped cell list - same
	// cost as a straight carve, just bent) - user-confirmed acceptable in PIE at 8% amplitude.
	// IH-DEC-065 (same day) reverted this working carve back to straight, but explicitly logged as
	// "unrelated to the WWF bug; part of the HIGH/VOLC-adjacent tuning churn" swept out during the
	// Terrain Stamps pivot's broad pre-churn regression - never a deliberate curvature-ownership
	// decision. The "Terrain Stamp's job" phrasing was this session's OWN round-1 inference upon
	// finding the already-reverted state, then mistakenly cited as "the standing decision" in round
	// 2's plan and again in round 4's own clarifying question - compounding the error without
	// re-checking the source. User confirmed (round 4): reverse it. Restoring IH-DEC-060's PROVEN
	// mechanism below - lower risk than round 2/3's discrete elbow-hook workaround, since this one has
	// already been through both failure modes and the fix, not a fresh design.
	//
	// Shared 2D segment-intersection test (Ask C, round 4): standard parametric-orientation test,
	// used to keep primary troughs, sub-inlets, and daughter troughs from visibly crossing each other
	// (user's own red-line-annotated "X" observations). Local to this function, not a new shared
	// utility - narrow, single-purpose use.
	auto SegmentsIntersect = [](const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D) -> bool
	{
		auto Orient = [](const FVector2D& O, const FVector2D& P1, const FVector2D& P2) -> double
		{
			return (P1.X - O.X) * (P2.Y - O.Y) - (P1.Y - O.Y) * (P2.X - O.X);
		};
		const double D1 = Orient(C, D, A);
		const double D2 = Orient(C, D, B);
		const double D3 = Orient(A, B, C);
		const double D4 = Orient(A, B, D);
		return ((D1 > 0.0) != (D2 > 0.0)) && ((D3 > 0.0) != (D4 > 0.0));
	};
	// Every carved path on this island (primary troughs, sub-inlets, daughter troughs, in carving
	// order) - candidates are checked against this BEFORE committing a carve, so later features avoid
	// earlier ones. Deliberately island-local (not cross-island) - no shared coordinate frame need.
	TArray<TArray<FVector2D>> AllCarvedPathsThisIsland;
	auto PathCrossesExisting = [&AllCarvedPathsThisIsland, &SegmentsIntersect](const TArray<FVector2D>& Candidate) -> bool
	{
		if (Candidate.Num() < 2)
		{
			return false;
		}
		for (const TArray<FVector2D>& Existing : AllCarvedPathsThisIsland)
		{
			for (int32 E = 1; E < Existing.Num(); ++E)
			{
				for (int32 C = 1; C < Candidate.Num(); ++C)
				{
					if (SegmentsIntersect(Existing[E - 1], Existing[E], Candidate[C - 1], Candidate[C]))
					{
						return true;
					}
				}
			}
		}
		return false;
	};

	// Restored IH-DEC-060 mechanism (round 4): reference path once (FindPathOnly), sample waypoints
	// along it with a sine-varying lateral offset (PickCellNearPath), diffuse the resulting cell list
	// ONCE (DiffuseAlongCells) - never chaining independent segments, the exact structural cause of
	// both historical severing regressions. AmplitudeFrac answers "bend radius" (how far the path
	// bulges); Periods answers "sinusoidal, less linear" (how many bulges - 1.0 is IH-DEC-060's own
	// proven single-S-curve baseline, self-test-calibrated up from there this round).
	// 2026-09-04 (round 6, IH-DEC-082): round 5's fix (scaling waypoint COUNT) reduced the average
	// gap between independently-picked waypoints but didn't guarantee any two consecutive ones were
	// actually graph-adjacent - PickCellNearPath finds the nearest cell to an offset TARGET POINT,
	// with no awareness of Cell.Neighbors connectivity, and its own per-call jitter
	// (Stream.FRandRange(0.9,1.1) on the lateral offset) means even closely-spaced waypoints can pick
	// cells several hops apart. DiffuseFromSeeds' multi-source BFS then pinches shallow in real gaps
	// between disconnected seeds (each seed's own decay reaching only partway across), reading as a
	// chain of small circular "craters" instead of one continuous trough - user-confirmed, common on
	// almost every island. Real fix: guarantee hop-continuity, not just higher sample density. Pick a
	// SMALL number of bend anchors along the reference path (the sine offset targets), then connect
	// EVERY consecutive anchor pair with FindPathIndicesOnly - a real graph-adjacent hop-chain, not
	// another independent nearest-point search - so the final seed list has zero connectivity gaps by
	// construction. Cheaper than round 5's approach too: ~8 expensive NearestCellToPoint lookups
	// (one per anchor) instead of up to 80, plus a handful of cheap graph-BFS connects bounded by
	// hop-distance between anchors (not O(graph size) each).
	auto BuildSineCurveCandidate = [&Graph](
		int32 StartIdx, int32 EndIdx, double AmplitudeFrac, double Periods, double PathRandomness,
		FRandomStream& RandStream, TArray<int32>& OutSeedIndices, TArray<FVector2D>& OutPositions) -> bool
	{
		OutSeedIndices.Reset();
		OutPositions.Reset();
		TArray<FVector2D> RefPath;
		if (!FIHTerrainCellDiffusion::FindPathOnly(Graph, StartIdx, EndIdx, PathRandomness, RandStream, RefPath)
			|| RefPath.Num() < 2)
		{
			return false;
		}
		double RefPathLenCm = 0.0;
		for (int32 P = 1; P < RefPath.Num(); ++P)
		{
			RefPathLenCm += FVector2D::Distance(RefPath[P - 1], RefPath[P]);
		}

		constexpr int32 NumAnchors = 8; // bend-offset targets, not a density sample count
		TArray<int32> AnchorIndices;
		AnchorIndices.Add(StartIdx);
		for (int32 A = 1; A < NumAnchors - 1; ++A)
		{
			const double AlongFrac = static_cast<double>(A) / static_cast<double>(NumAnchors - 1);
			const double LateralOffsetCm = AmplitudeFrac * RefPathLenCm * FMath::Sin(2.0 * PI * Periods * AlongFrac);
			const int32 AnchorIdx = FIHTerrainCellDiffusion::PickCellNearPath(Graph, RefPath, AlongFrac, LateralOffsetCm, RandStream);
			if (AnchorIdx != INDEX_NONE && AnchorIdx != AnchorIndices.Last())
			{
				AnchorIndices.Add(AnchorIdx);
			}
		}
		if (AnchorIndices.Last() != EndIdx)
		{
			AnchorIndices.Add(EndIdx);
		}

		OutSeedIndices.Add(StartIdx);
		OutPositions.Add(Graph.Cells[StartIdx].SitePos);
		for (int32 A = 1; A < AnchorIndices.Num(); ++A)
		{
			const int32 FromIdx = AnchorIndices[A - 1];
			const int32 ToIdx = AnchorIndices[A];
			if (FromIdx == ToIdx)
			{
				continue;
			}
			TArray<int32> SubChain;
			if (!FIHTerrainCellDiffusion::FindPathIndicesOnly(Graph, FromIdx, ToIdx, PathRandomness, RandStream, SubChain)
				|| SubChain.Num() < 2)
			{
				continue; // couldn't connect this anchor pair - skip it, keep the rest of the chain
			}
			for (int32 S = 1; S < SubChain.Num(); ++S) // S=1: SubChain[0]==FromIdx, already the last seed added
			{
				if (OutSeedIndices.Last() != SubChain[S])
				{
					OutSeedIndices.Add(SubChain[S]);
					OutPositions.Add(Graph.Cells[SubChain[S]].SitePos);
				}
			}
		}
		return OutSeedIndices.Num() >= 2;
	};

	constexpr double PrimaryAmplitudeFrac = 0.12; // self-test-calibrated, up from IH-DEC-060's proven 0.08
	constexpr double PrimaryPeriods = 1.4; // >1.0 (IH-DEC-060's single S-curve) for a wavier look
	constexpr double MaxTroughLengthFracOfDiagonal = 0.65;
	const double MaxTroughLengthCm = IslandDiagonalCm * MaxTroughLengthFracOfDiagonal;
	constexpr int32 MaxCrossingRetries = 6;

	for (int32 i = 0; i < NumPrimaryTroughs; ++i)
	{
		TArray<int32> ChosenSeedIndices;
		TArray<FVector2D> ChosenPositions;
		for (int32 Retry = 0; Retry < MaxCrossingRetries; ++Retry)
		{
			// 2026-09-04: fresh per-trough angle (not reused from the triangle's own rotation, so
			// multiple troughs on one island don't also all align with each other) - see the
			// PickRandomCellInFracWindow comment above for why this fixes the cross-island fixed-angle
			// pattern the user flagged.
			const double TroughRotationRad = Stream.FRandRange(0.0, 2.0 * PI);
			const int32 StartIdx = PickRandomCellInFracWindow(FVector2D(0.32, 0.68), FVector2D(0.32, 0.68), Stream, TroughRotationRad);
			const int32 RawEndIdx = PickRandomCellInFracWindow(FVector2D(0.10, 0.90), FVector2D(0.10, 0.90), Stream, TroughRotationRad);
			if (StartIdx == INDEX_NONE || RawEndIdx == INDEX_NONE || StartIdx == RawEndIdx)
			{
				continue;
			}

			// Length cap (round 3, Ask 1 - re-derived from a SHORT reference each retry, not the
			// original path's own length, per that round's own hard-won lesson): if the raw pick
			// exceeds the cap, replace End with a capped-distance point along the same direction
			// before the sine warp ever runs, so the warp bends a short reference, not a long one.
			int32 EndIdx = RawEndIdx;
			const double StraightDistCm = FVector2D::Distance(Graph.Cells[StartIdx].SitePos, Graph.Cells[RawEndIdx].SitePos);
			if (StraightDistCm > MaxTroughLengthCm)
			{
				TArray<FVector2D> UncappedRefPath;
				if (FIHTerrainCellDiffusion::FindPathOnly(Graph, StartIdx, RawEndIdx, /*PathRandomness=*/0.55, Stream, UncappedRefPath)
					&& UncappedRefPath.Num() >= 2)
				{
					double UncappedLenCm = 0.0;
					for (int32 P = 1; P < UncappedRefPath.Num(); ++P)
					{
						UncappedLenCm += FVector2D::Distance(UncappedRefPath[P - 1], UncappedRefPath[P]);
					}
					const double CapFrac = UncappedLenCm > 0.0
						? FMath::Clamp(MaxTroughLengthCm / UncappedLenCm, 0.15, 0.95)
						: 1.0;
					const int32 CappedEndIdx = FIHTerrainCellDiffusion::PickCellNearPath(Graph, UncappedRefPath, CapFrac, 0.0, Stream);
					if (CappedEndIdx != INDEX_NONE && CappedEndIdx != StartIdx)
					{
						EndIdx = CappedEndIdx;
					}
				}
			}

			TArray<int32> CandidateSeedIndices;
			TArray<FVector2D> CandidatePositions;
			if (!BuildSineCurveCandidate(StartIdx, EndIdx, PrimaryAmplitudeFrac, PrimaryPeriods, /*PathRandomness=*/0.55,
				Stream, CandidateSeedIndices, CandidatePositions))
			{
				continue;
			}
			if (PathCrossesExisting(CandidatePositions) && Retry < MaxCrossingRetries - 1)
			{
				continue; // a non-crossing redraw is preferred but not required - last retry accepts regardless (Ask C)
			}
			ChosenSeedIndices = MoveTemp(CandidateSeedIndices);
			ChosenPositions = MoveTemp(CandidatePositions);
			break;
		}

		if (ChosenSeedIndices.Num() >= 2)
		{
			FIHTerrainCellDiffusion::DiffuseAlongCells(
				Graph, ChosenSeedIndices, /*HeightMin=*/-35.0, /*HeightMax=*/-20.0, /*LinePower=*/0.85, Stream);
			AllCarvedPathsThisIsland.Add(ChosenPositions);
			PrimaryTroughPaths.Add(MoveTemp(ChosenPositions));
		}
	}

	// Plan Addendum 12: the proven NestedInletShape automation test (broadBaySpans=2
	// bestSpanSubInlets=4) uses a THREE-layer recipe - major-bay, sub-inlet, guaranteed-nesting -
	// but only the major-bay (primary trough) and guaranteed-nesting (daughter trough) layers ever
	// made it into this real recipe. The middle "sub-inlet" layer - narrower/deeper troughs in the
	// same coastal window, an independent statistical layer per Azgaar's own layering mechanism
	// (heightmap-templates.ts), not an explicit hierarchy - was missing entirely. Porting the
	// test's own proven window/LinePower/height values directly rather than re-deriving them.
	// Primary troughs reverted to a fixed 2 above (2026-09-03, radial-spoke fix). That alone caused
	// a severe regression (landFraction ~1.0), self-test-caught - but the real cause turned out to
	// be MainBlobPower/AccentBlobPower's clamp ceiling (see above), not a missing negative-carving
	// counterweight: at this island's real HopRadius, each hill was flooding nearly the entire
	// graph regardless of trough count, since its diffusion couldn't decay within the graph's own
	// extent. Sub-inlets were scaled up here as a workaround before that root cause was found -
	// with BlobPower now properly bounded (hills decay within a reasonable fraction of HopRadius
	// again), that workaround overshot HARD the other way (landFraction 0.02-0.14, self-test-
	// caught) - reverted back to the original fixed count, matching IH_WB_Demo003.
	//
	// 2026-09-04 (Track A): this window was never rotated per island either - the same fixed-axis
	// cross-island angle bias diagnosed and fixed for primary troughs above, just never propagated
	// to this second carving layer. Fresh per-call rotation, independent of the primary troughs' own
	// rotations, so sub-inlets don't correlate with them either.
	//
	// 2026-09-04 (round 3, Ask 2): AddRange's plain Start/End picking (both drawn independently
	// within the same window, no distance targeting) gave sub-inlets an uncontrolled, arbitrary
	// length. Per the user's own request for ~33%-of-diagonal "side troughs," replaced the AddRange
	// call with a local Count=2 loop that reuses PickRandomCellInFracWindow (already in scope, same
	// lambda primary troughs use above) for Start, then draws up to MaxLengthTries End candidates
	// (fresh rotation each try) and keeps whichever lands closest to a per-inlet target length - the
	// same "retry-and-keep-best" idiom PickRandomCellInFracWindow itself already uses internally, not
	// a new geometric search. Carves via AddRangeBetweenCells with AddRange's own former parameters.
	// 2026-09-04 (round 4): sine-curve carve (same restored IH-DEC-060 mechanism as primary troughs
	// above) + crossing avoidance (Ask C), gentler amplitude than primary troughs since sub-inlets are
	// already shorter (~33% diagonal vs. primary's 65% cap).
	constexpr double SubInletAmplitudeFrac = 0.10;
	constexpr double SubInletPeriods = 1.2;
	constexpr int32 NumSubInlets = 2;
	constexpr int32 MaxSubInletLengthTries = 12;
	for (int32 SubInletIdx = 0; SubInletIdx < NumSubInlets; ++SubInletIdx)
	{
		TArray<int32> ChosenSeedIndices;
		TArray<FVector2D> ChosenPositions;
		for (int32 CrossingRetry = 0; CrossingRetry < MaxCrossingRetries; ++CrossingRetry)
		{
			const double SubInletStartRotationRad = Stream.FRandRange(0.0, 2.0 * PI);
			const int32 SubInletStartIdx = PickRandomCellInFracWindow(FVector2D(0.30, 0.70), FVector2D(0.30, 0.70), Stream, SubInletStartRotationRad);
			if (SubInletStartIdx == INDEX_NONE)
			{
				continue;
			}
			const double TargetSubInletLenCm = Stream.FRandRange(0.28, 0.38) * IslandDiagonalCm;
			int32 BestEndIdx = INDEX_NONE;
			double BestLenDeltaCm = TNumericLimits<double>::Max();
			for (int32 Try = 0; Try < MaxSubInletLengthTries; ++Try)
			{
				const double CandidateRotationRad = Stream.FRandRange(0.0, 2.0 * PI);
				const int32 CandidateEndIdx = PickRandomCellInFracWindow(FVector2D(0.30, 0.70), FVector2D(0.30, 0.70), Stream, CandidateRotationRad);
				if (CandidateEndIdx == INDEX_NONE || CandidateEndIdx == SubInletStartIdx)
				{
					continue;
				}
				const double CandidateLenCm = FVector2D::Distance(Graph.Cells[SubInletStartIdx].SitePos, Graph.Cells[CandidateEndIdx].SitePos);
				const double LenDeltaCm = FMath::Abs(CandidateLenCm - TargetSubInletLenCm);
				if (LenDeltaCm < BestLenDeltaCm)
				{
					BestLenDeltaCm = LenDeltaCm;
					BestEndIdx = CandidateEndIdx;
				}
			}
			if (BestEndIdx == INDEX_NONE)
			{
				continue;
			}

			TArray<int32> CandidateSeedIndices;
			TArray<FVector2D> CandidatePositions;
			if (!BuildSineCurveCandidate(SubInletStartIdx, BestEndIdx, SubInletAmplitudeFrac, SubInletPeriods,
				/*PathRandomness=*/0.5, Stream, CandidateSeedIndices, CandidatePositions))
			{
				continue;
			}
			if (PathCrossesExisting(CandidatePositions) && CrossingRetry < MaxCrossingRetries - 1)
			{
				continue;
			}
			ChosenSeedIndices = MoveTemp(CandidateSeedIndices);
			ChosenPositions = MoveTemp(CandidatePositions);
			break;
		}

		if (ChosenSeedIndices.Num() >= 2)
		{
			FIHTerrainCellDiffusion::DiffuseAlongCells(
				Graph, ChosenSeedIndices, /*HeightMin=*/-50.0, /*HeightMax=*/-30.0, /*LinePower=*/0.78, Stream);
			AllCarvedPathsThisIsland.Add(MoveTemp(ChosenPositions));
		}
	}

	// Cove-scale daughter troughs biased toward a primary trough's path - the compound
	// nested-inlet extension verified this session (TerrainCellGraph.NestedInletShape:
	// broadBaySpans=2 bestSpanSubInlets=4). One per primary (was two) - fewer carves into a
	// still-thin landmass.
	//
	// Plan Addendum 12: depth was -25/-15 - shallower than even the test's OWN sub-inlet layer
	// (-50/-30), let alone its guaranteed-nesting layer (-55/-35) - dialed back during the
	// Addendum 2 severing panic, before the IH-DEC-020 islet-budget filter existed as a
	// fragmentation safety net. That constraint no longer fully applies; ported the test's proven
	// guaranteed-nesting depth and its 60%-chance probability (was always-applied) for natural
	// per-island variation instead of every island reading equally nested.
	constexpr float NestedInletBiasChance = 0.6f;
	int32 DaughterTroughCount = 0;
	for (const TArray<FVector2D>& ParentPath : PrimaryTroughPaths)
	{
		if (Stream.FRand() > NestedInletBiasChance)
		{
			continue;
		}
		// 2026-09-04 (round 4, Ask C): lighter-weight crossing check than primary/sub-inlet troughs -
		// daughter troughs are already short/cove-scale and probabilistic (60% chance), so a couple of
		// redraws is enough; still falls through to carving on the last attempt regardless (never
		// blocks generation).
		double AlongFrac = 0.0, LateralCm = 0.0;
		int32 StartIdx = INDEX_NONE, EndIdx = INDEX_NONE;
		constexpr int32 MaxDaughterCrossingRetries = 3;
		for (int32 DaughterRetry = 0; DaughterRetry < MaxDaughterCrossingRetries; ++DaughterRetry)
		{
			AlongFrac = Stream.FRandRange(0.25, 0.75);
			LateralCm = (Stream.FRand() < 0.5 ? -1.0 : 1.0) * Stream.FRandRange(600.0, 2200.0);
			StartIdx = FIHTerrainCellDiffusion::PickCellNearPath(Graph, ParentPath, AlongFrac, LateralCm, Stream);
			EndIdx = FIHTerrainCellDiffusion::PickCellNearPath(
				Graph, ParentPath, FMath::Clamp(AlongFrac + Stream.FRandRange(0.08, 0.18), 0.0, 1.0),
				LateralCm * 1.6, Stream);
			if (StartIdx == INDEX_NONE || EndIdx == INDEX_NONE || StartIdx == EndIdx)
			{
				continue;
			}
			const TArray<FVector2D> CandidateSegment = { Graph.Cells[StartIdx].SitePos, Graph.Cells[EndIdx].SitePos };
			if (PathCrossesExisting(CandidateSegment) && DaughterRetry < MaxDaughterCrossingRetries - 1)
			{
				continue;
			}
			break;
		}
		if (StartIdx == INDEX_NONE || EndIdx == INDEX_NONE || StartIdx == EndIdx)
		{
			continue;
		}
		{
			TArray<FVector2D> DaughterPath;
			DaughterPath.Add(Graph.Cells[StartIdx].SitePos);
			DaughterPath.Add(Graph.Cells[EndIdx].SitePos);
			AllCarvedPathsThisIsland.Add(MoveTemp(DaughterPath));
		}
		FIHTerrainCellDiffusion::AddRangeBetweenCells(
			Graph, StartIdx, EndIdx, /*HeightMin=*/-55.0, /*HeightMax=*/-35.0,
			/*LinePower=*/0.78, /*PathRandomness=*/0.5, Stream);
		++DaughterTroughCount;
	}

	// Multiple passes at a stronger factor than the small-test's single Factor=0.15 - needed to
	// average out the per-hop jitter noise (Bug 3) before thresholding, without erasing the
	// large-scale hill/trough shape Smooth is meant to preserve.
	// Plan Addendum 3: narrowing the accent-hill window did NOT consolidate the loop-size
	// distribution (still no dominant loop, e.g. island 0 stayed [36,30,30,24,24] after narrowing
	// from [50,38,35,27,26]) - refuting that hypothesis. Real cause is more likely the MAIN hill's
	// own per-hop jitter compounding along whichever specific BFS-tree path first reaches each
	// cell at real hop-radius (~125-200) - branches can diverge enough in accumulated height to
	// dip below LandThreshold and back up, producing genuinely separate high-ground "islands" from
	// ONE seed, not just noisy edges. Pushing further on the one lever already proven to reduce
	// fragmentation (Smooth cut loop count ~50% in the first attempt) rather than touching the
	// core diffusion algorithm itself.
	// LOW-island stabilization (2026-09-02): reverted to the fixed LandThreshold=20.0 + plain
	// Smooth (matches the IH_WB_Demo003 fork point, commit 96273db) — this session first kept
	// IH-DEC-062's adaptive per-island threshold (its own decision-record text called it
	// PIE-confirmed-good: real compound nested inlets/trough curvature, fixing chronic
	// 0.04-0.21 under-fill), reasoning the actual WWF-weld regression traced to FindSharedEdge's
	// residual failure rate (IH-DEC-063) instead. That reasoning was sound against the paper
	// record, but real PIE evidence at the 512,000-acre scale it actually matters at contradicts
	// it: islands read as a harsh, hard-edged radiating "starburst," not the clean compound-inlet
	// shapes IH_WB_Demo003 produces. Real visual evidence overrides the paper trail. KNOWN RISK,
	// flagged for the next PIE pass: this fixed threshold was never tuned/tested at 512,000 acres
	// (only at Demo003's original 128,000) — it's exactly what produced the chronic under-fill
	// IH-DEC-062 was built to fix, at this same larger scale. If under-fill reappears rather than
	// the starburst clearing up, the next diagnostic step is testing this same algorithm at
	// Demo003's original 128,000-acre scale to isolate algorithm from scale, before reaching for
	// a size-aware (not fully adaptive) threshold instead of either extreme.
	constexpr double LandThreshold = 20.0;
	for (int32 SmoothPass = 0; SmoothPass < 8; ++SmoothPass)
	{
		FIHTerrainCellDiffusion::Smooth(Graph, 0.45);
	}

	FIHTerrainCellDiffusion::ClassifyLandWater(Graph, LandThreshold);

	// Plan Addendum 12: subtle, shared-vertex-consistent boundary jitter for coastline texture.
	// Perturbing MainCoastPolylineLocalCm alone (after the trace, further below) would desync from
	// the actual rendered mesh, which triangulates directly from Graph.Cells[*].Boundary - not
	// from that polyline. Perturbing the SOURCE boundary vertices here instead means the mesh, the
	// coastline trace, the shelf trace, and every ribbon/minimap consumer all inherit the same
	// detail automatically and stay consistent - the same principle as the actor-recenter shift
	// below. Deliberately conservative amplitude: per InvisibleHand_CameraOptimization_Notes.md's
	// bird's-eye/GIS-scale-layered canon, "Game Optimal Altitude" (not an extreme close-up) is the
	// PRIMARY gameplay viewing distance, so detail only legible from an unrealistic close zoom is
	// wasted effort - this stays at texture scale, well below what could compete with or wash out
	// a real bay/headland/half-moon coastal feature. Every cell's copy of a shared boundary corner
	// must move by the SAME amount (keyed by quantized position) or adjacent cells would crack
	// apart at their shared edge - smooth (multi-cell-wavelength) noise also keeps a single cell's
	// own corners moving in a correlated way, so cells stay simple (non-self-intersecting) polygons.
	{
		const double NoiseScaleCm = BuildParams.TargetCellWidthCm * 2.5; // wobble wavelength - a few cell-widths, not per-vertex jitter
		const double AmplitudeCm = BuildParams.TargetCellWidthCm * 0.10; // ~7.5m at the 75m cell width - texture, not a new landform
		const FVector2D SeedOffsetA(Stream.FRandRange(0.0, 10000.0), Stream.FRandRange(0.0, 10000.0));
		const FVector2D SeedOffsetB(Stream.FRandRange(0.0, 10000.0), Stream.FRandRange(0.0, 10000.0));
		TMap<FIntPoint, FVector2D> BoundaryDisplacement;
		auto QuantizeBoundaryKey = [](const FVector2D& P) -> FIntPoint
		{
			constexpr double QuantCm = 1.0;
			return FIntPoint(FMath::RoundToInt32(P.X / QuantCm), FMath::RoundToInt32(P.Y / QuantCm));
		};
		for (FIHTerrainCell& Cell : Graph.Cells)
		{
			for (FVector2D& BoundaryPt : Cell.Boundary)
			{
				const FIntPoint Key = QuantizeBoundaryKey(BoundaryPt);
				FVector2D* Existing = BoundaryDisplacement.Find(Key);
				if (!Existing)
				{
					const FVector2D NoiseInput = BoundaryPt / NoiseScaleCm;
					const double DX = FMath::PerlinNoise2D(NoiseInput + SeedOffsetA);
					const double DY = FMath::PerlinNoise2D(NoiseInput + SeedOffsetB);
					Existing = &BoundaryDisplacement.Add(Key, FVector2D(DX, DY) * AmplitudeCm);
				}
				BoundaryPt += *Existing;
			}
		}
	}

	// Hoisted out of the islet-budget/pond-fill blocks below (both scoped in their own { }) so the
	// ring-classification pass further down (IH-DEC-035 follow-up: area-matching instead of a
	// concave-vulnerable ring centroid) can compare each traced ring's own area against the KNOWN
	// cell counts of every surviving land islet / kept inland-sea component, without re-deriving
	// connectivity a second time.
	TArray<int32> KeptLandComponentCellCounts;
	int32 MainComponentCellCountForRingClassification = 0;
	TArray<int32> KeptWaterComponentCellCounts;

	// Plan Addendum 4: IH-DEC-020 islet-area budget enforcement (IH_Canonical_Decisions.md:246-251
	// - combined islet area <=5% of the parent island, no individual islet >2%; accidental
	// detached fragments are not valid islets). Three rounds of Smooth/hill-recipe parameter
	// tuning produced real but inconsistent, seed-dependent cohesion - connected components over
	// Land cells (exact, via Cell.Neighbors BFS) gives a direct, unambiguous landmass count/size
	// instead of inferring it indirectly from coastline loop counts, and lets the already-agreed
	// canon rule do the fragmentation work instead of more guessing.
	{
		TArray<int32> ComponentId;
		ComponentId.Init(INDEX_NONE, Graph.Num());
		TArray<TArray<int32>> Components;
		for (int32 SeedIdx = 0; SeedIdx < Graph.Num(); ++SeedIdx)
		{
			if (Graph.Cells[SeedIdx].Feature != EIHCellFeature::Land || ComponentId[SeedIdx] != INDEX_NONE)
			{
				continue;
			}
			const int32 NewComponentIdx = Components.AddDefaulted();
			TArray<int32>& Comp = Components[NewComponentIdx];
			TArray<int32> Stack;
			Stack.Add(SeedIdx);
			ComponentId[SeedIdx] = NewComponentIdx;
			while (Stack.Num() > 0)
			{
				const int32 Cur = Stack.Pop();
				Comp.Add(Cur);
				for (const int32 NeighborIdx : Graph.Cells[Cur].Neighbors)
				{
					if (Graph.Cells.IsValidIndex(NeighborIdx)
						&& Graph.Cells[NeighborIdx].Feature == EIHCellFeature::Land
						&& ComponentId[NeighborIdx] == INDEX_NONE)
					{
						ComponentId[NeighborIdx] = NewComponentIdx;
						Stack.Add(NeighborIdx);
					}
				}
			}
		}

		if (Components.Num() > 0)
		{
			int32 MainComponentIdx = 0;
			for (int32 i = 1; i < Components.Num(); ++i)
			{
				if (Components[i].Num() > Components[MainComponentIdx].Num())
				{
					MainComponentIdx = i;
				}
			}
			const int32 MainComponentCellCount = Components[MainComponentIdx].Num();
			MainComponentCellCountForRingClassification = MainComponentCellCount;

			// Plan Addendum 7: Grab 7 (GIZMO7) showed a component that passed the IH-DEC-020 area
			// budget but sat far enough from its parent landmass to not read as "subordinate" for
			// gameplay. Cap islet distance too, scaled to the PARENT's own footprint (user-
			// confirmed direction) rather than a fixed meters value, so the leash adapts to island
			// size instead of feeling too tight on huge islands or too loose on small ones.
			//
			// Plan Addendum 10: this cell-averaged centroid (directly reflects where the actual
			// land IS, immune to the concave-coastline pathologies a boundary-polygon centroid can
			// hit) is also the right anchor for SelectionReticle/the island caption - hoisted out of
			// the "islets exist" guard (was Components.Num() > 1) so it's always available, and
			// promoted to a persistent actor member below.
			FVector2D MainCentroid = FVector2D::ZeroVector;
			for (const int32 CellIdx : Components[MainComponentIdx])
			{
				MainCentroid += Graph.Cells[CellIdx].SitePos;
			}
			MainCentroid /= static_cast<double>(MainComponentCellCount);
			double MainFootprintRadiusCm = 0.0;
			for (const int32 CellIdx : Components[MainComponentIdx])
			{
				MainFootprintRadiusCm = FMath::Max(
					MainFootprintRadiusCm, FVector2D::Distance(MainCentroid, Graph.Cells[CellIdx].SitePos));
			}
			// Plan Addendum 11: recenter the actor's OWN origin on the true landmass center, not
			// just the reticle's query point - per user diagnosis, this makes the reticle, the
			// camera's click-to-focus target, AND the island move/rotate gizmo pivot all correct
			// by construction, with nothing left to keep re-deriving. Every live local-cm array
			// (coastline, shelf, ribbon rings) and every active mesh's vertex data is derived from
			// Graph.Cells[*].SitePos/.Boundary at some point after this, so shifting the SOURCE
			// once here is sufficient - everything downstream inherits the new origin for free.
			// Compensating SetActorLocation keeps every world-space consumer unaffected (a pure
			// re-origin) - confirmed safe via a dedicated research pass (Minimap's cached coastline
			// only refreshes AFTER BuildMeshesFromCellGraph completes, per ApplyTankLayout's
			// existing call order, so it picks up the final shifted state automatically).
			for (FIHTerrainCell& Cell : Graph.Cells)
			{
				Cell.SitePos -= MainCentroid;
				for (FVector2D& BoundaryPt : Cell.Boundary)
				{
					BoundaryPt -= MainCentroid;
				}
			}
			SetActorLocation(GetActorLocation() + GetActorRotation().RotateVector(FVector(MainCentroid.X, MainCentroid.Y, 0.0)));
			MainLandCentroidLocalCm = FVector2D::ZeroVector;
			MainLandFootprintRadiusCm = static_cast<float>(MainFootprintRadiusCm);
			// MainCentroid itself must track the shift too - the islet-distance filter below still
			// reads it against the now-shifted Graph.Cells positions.
			MainCentroid = FVector2D::ZeroVector;
			// Plan Addendum 12 / IH-DEC-020 amendment: relaxed per user direction to keep more of
			// the naturally-forming islets that were being discarded, as a lighter-weight path
			// toward a barrier-islet look than a dedicated new seeding system.
			constexpr double MaxIsletDistanceFactor = 2.5;
			const double MaxIsletDistanceCm = MainFootprintRadiusCm * MaxIsletDistanceFactor;

			TArray<int32> OtherIndices;
			for (int32 i = 0; i < Components.Num(); ++i)
			{
				if (i != MainComponentIdx)
				{
					OtherIndices.Add(i);
				}
			}
			OtherIndices.Sort([&Components](const int32 A, const int32 B)
			{
				return Components[A].Num() > Components[B].Num();
			});

			const double IndividualBudget = MainComponentCellCount * 0.04;
			const double CombinedBudget = MainComponentCellCount * 0.10;
			double CombinedKeptCells = 0.0;
			int32 KeptIsletCount = 0;
			int32 DiscardedCount = 0;
			int64 DiscardedCells = 0;
			int32 DiscardedByDistanceCount = 0;
			for (const int32 CompIdx : OtherIndices)
			{
				const int32 CompSize = Components[CompIdx].Num();
				const bool bWithinIndividual = CompSize <= IndividualBudget;
				const bool bWithinCombined = (CombinedKeptCells + CompSize) <= CombinedBudget;
				FVector2D CompCentroid = FVector2D::ZeroVector;
				for (const int32 CellIdx : Components[CompIdx])
				{
					CompCentroid += Graph.Cells[CellIdx].SitePos;
				}
				CompCentroid /= static_cast<double>(CompSize);
				const double DistanceToMainCm = FVector2D::Distance(MainCentroid, CompCentroid);
				const bool bWithinDistance = DistanceToMainCm <= MaxIsletDistanceCm;
				if (bWithinIndividual && bWithinCombined && bWithinDistance)
				{
					CombinedKeptCells += CompSize;
					++KeptIsletCount;
					KeptLandComponentCellCounts.Add(CompSize);
					// IH-DEC-039: real mainland-islet distance, previously computed but not logged -
					// measures whether the existing 2.5x-footprint-radius leash (IH-DEC-020) produces
					// islets close enough to feel like a walkable "channel" once the WWF shelf ramp
					// covers them, or whether a future WalkableChannelCapCm clamp on MaxIsletDistanceCm
					// is warranted.
					UE_LOG(LogTemp, Log,
						TEXT("IH_WB_IslandActor: island=%d isletKept cells=%d distanceToMainM=%.1f maxIsletDistanceM=%.1f"),
						TankIslandIndex, CompSize, DistanceToMainCm * 0.01, MaxIsletDistanceCm * 0.01);
				}
				else
				{
					for (const int32 CellIdx : Components[CompIdx])
					{
						Graph.Cells[CellIdx].Feature = EIHCellFeature::Ocean;
					}
					++DiscardedCount;
					DiscardedCells += CompSize;
					if (!bWithinDistance && bWithinIndividual && bWithinCombined)
					{
						++DiscardedByDistanceCount;
					}
				}
			}

			UE_LOG(LogTemp, Log,
				TEXT("IH_WB_IslandActor: island=%d islet-budget mainCells=%d totalComponents=%d keptIslets=%d discardedComponents=%d discardedCells=%lld discardedByDistance=%d maxIsletDistanceCm=%.0f"),
				TankIslandIndex, MainComponentCellCount, Components.Num(), KeptIsletCount, DiscardedCount, DiscardedCells,
				DiscardedByDistanceCount, MaxIsletDistanceCm);
		}
	}

	// Fill degenerate single/few-cell interior water "ponds" - live-log-confirmed root cause of
	// the minimap's "scattered gold fragments": islet-budget above reported e.g. keptIslets=2
	// (3 land blobs total) for an island whose TraceCoastlineLoops then produced loops=154 - the
	// ~150 extra loops were all tiny (rawVerts 3-13, area ~100-2500 world m^2, i.e. roughly one
	// Voronoi cell), matching isolated Ocean cells fully surrounded by Land, each traced as its
	// own small interior-hole boundary loop and pushed into ContourGoldRingsLocalCm alongside the
	// real coastline/islets. Same per-hop diffusion jitter that produced land noise-speck islets
	// (Bug 3) flips isolated cells the other way too. Inland seas/lakes stay canon
	// (IH-DEC-032/033) - this only reclassifies water components too small to be an intentional
	// lake back to Land, mirroring the land-side islet-budget filter in reverse.
	{
		TArray<int32> WaterComponentId;
		WaterComponentId.Init(INDEX_NONE, Graph.Num());
		TArray<TArray<int32>> WaterComponents;
		for (int32 SeedIdx = 0; SeedIdx < Graph.Num(); ++SeedIdx)
		{
			if (Graph.Cells[SeedIdx].Feature == EIHCellFeature::Land || WaterComponentId[SeedIdx] != INDEX_NONE)
			{
				continue;
			}
			const int32 NewComponentIdx = WaterComponents.AddDefaulted();
			TArray<int32>& Comp = WaterComponents[NewComponentIdx];
			TArray<int32> Stack;
			Stack.Add(SeedIdx);
			WaterComponentId[SeedIdx] = NewComponentIdx;
			while (Stack.Num() > 0)
			{
				const int32 Cur = Stack.Pop();
				Comp.Add(Cur);
				for (const int32 NeighborIdx : Graph.Cells[Cur].Neighbors)
				{
					if (Graph.Cells.IsValidIndex(NeighborIdx)
						&& Graph.Cells[NeighborIdx].Feature != EIHCellFeature::Land
						&& WaterComponentId[NeighborIdx] == INDEX_NONE)
					{
						WaterComponentId[NeighborIdx] = NewComponentIdx;
						Stack.Add(NeighborIdx);
					}
				}
			}
		}

		if (WaterComponents.Num() > 1)
		{
			int32 MainWaterComponentIdx = 0;
			for (int32 i = 1; i < WaterComponents.Num(); ++i)
			{
				if (WaterComponents[i].Num() > WaterComponents[MainWaterComponentIdx].Num())
				{
					MainWaterComponentIdx = i;
				}
			}

			constexpr int32 MinLakeCellCount = 5;
			int32 FilledPondCount = 0;
			int64 FilledPondCells = 0;

			// 2026-09-04 (IH-DEC-074, GIZMO7 fix, this round): a water component that touches this
			// island's own rectangular clip-box edge cannot be a genuine enclosed inland sea by
			// definition - the box boundary is an artifact of this island's own local generation
			// extent, not a real shoreline; real open water continues past it into the wider realm.
			// Without this check, the BFS above can (and, per live GIZMO7 self-test data, does) split
			// a truly-connected ocean margin into a second "component" purely because the box wall cut
			// it off from the main component, which then gets kept as a spurious "inland sea" and
			// rendered on the realm minimap sitting in what is actually open water near another
			// island. Same 50cm epsilon idiom as this function's own later IsBoxEdgeClipped (shelf-mesh
			// section) for consistency - tiny relative to real cell footprint (~7500cm), large enough
			// to reliably catch a clip-produced boundary vertex. Only affects RING CLASSIFICATION
			// (KeptWaterComponentCellCounts) - the cells themselves are NOT filled to land (unlike the
			// small-pond case below), since they may be real, large water bodies; they simply never
			// area-match as an inland-sea ring candidate.
			constexpr double BoxEdgeTouchEpsilonCm = 50.0;
			const FVector2D WaterBfsBoundsMin = Graph.BoundsMinLocalCm;
			const FVector2D WaterBfsBoundsMax = Graph.BoundsMaxLocalCm;
			auto ComponentTouchesBoxEdge = [&Graph, WaterBfsBoundsMin, WaterBfsBoundsMax](const TArray<int32>& Component) -> bool
			{
				for (const int32 CellIdx : Component)
				{
					for (const FVector2D& P : Graph.Cells[CellIdx].Boundary)
					{
						if (FMath::Abs(P.X - WaterBfsBoundsMin.X) < BoxEdgeTouchEpsilonCm
							|| FMath::Abs(P.X - WaterBfsBoundsMax.X) < BoxEdgeTouchEpsilonCm
							|| FMath::Abs(P.Y - WaterBfsBoundsMin.Y) < BoxEdgeTouchEpsilonCm
							|| FMath::Abs(P.Y - WaterBfsBoundsMax.Y) < BoxEdgeTouchEpsilonCm)
						{
							return true;
						}
					}
				}
				return false;
			};
			// 2026-09-04 (round 3, Ask 3): a fully-enclosed (non-box-edge-touching, i.e. "entirely
			// inland" per the user's own phrasing - box-edge-touching already means "connects to open
			// water beyond this island," reusing that existing signal rather than inventing a new
			// "ocean connected" test) water component can still be an elongated TROUGH shape rather
			// than a rounded lake - e.g. a primary/side trough carve that never reached open water.
			// That reads as an unnatural linear "inland sea," not a lake. Orientation-independent
			// elongation test (troughs are randomly rotated per-island this session - a naive
			// axis-aligned bounding-box ratio would under-detect a diagonally-oriented trough, whose
			// axis-aligned bbox can look nearly square despite being visually long and thin): compute
			// the component's SitePos centroid and 2x2 covariance matrix, solve its eigenvalues in
			// closed form, and take the ratio of principal-axis standard deviations - a true elongation
			// measure regardless of rotation. Only affects components that would otherwise be KEPT as
			// inland-sea candidates (>=MinLakeCellCount, not box-edge-touching); elongated ones are
			// filled to Land instead (same mechanism as the small-pond fill below, gated on shape
			// rather than size). Rounder components (real lakes) are unaffected.
			constexpr double ElongationRatioThreshold = 2.5; // self-test-calibrated starting point
			auto ComponentElongationRatio = [&Graph](const TArray<int32>& Component) -> double
			{
				if (Component.Num() < 3)
				{
					return 1.0; // too few cells for a meaningful shape measure - treat as round/keep
				}
				FVector2D Centroid(0.0, 0.0);
				for (const int32 CellIdx : Component)
				{
					Centroid += Graph.Cells[CellIdx].SitePos;
				}
				Centroid /= static_cast<double>(Component.Num());

				double Cxx = 0.0, Cyy = 0.0, Cxy = 0.0;
				for (const int32 CellIdx : Component)
				{
					const FVector2D D = Graph.Cells[CellIdx].SitePos - Centroid;
					Cxx += D.X * D.X;
					Cyy += D.Y * D.Y;
					Cxy += D.X * D.Y;
				}
				const double N = static_cast<double>(Component.Num());
				Cxx /= N; Cyy /= N; Cxy /= N;

				const double Mid = (Cxx + Cyy) * 0.5;
				const double Rad = FMath::Sqrt(FMath::Max(0.0, FMath::Square((Cxx - Cyy) * 0.5) + Cxy * Cxy));
				const double LambdaMax = Mid + Rad;
				const double LambdaMin = FMath::Max(1.0, Mid - Rad); // guard against near-zero/degenerate minor axis
				return FMath::Sqrt(LambdaMax / LambdaMin);
			};

			int32 BoxEdgeExcludedComponentCount = 0;
			int32 ElongatedTroughFilledCount = 0;
			int64 ElongatedTroughFilledCells = 0;

			for (int32 i = 0; i < WaterComponents.Num(); ++i)
			{
				if (i == MainWaterComponentIdx)
				{
					continue;
				}
				if (WaterComponents[i].Num() >= MinLakeCellCount)
				{
					if (ComponentTouchesBoxEdge(WaterComponents[i]))
					{
						++BoxEdgeExcludedComponentCount;
						continue; // not filled to land (may be real, large water) - just excluded as an inland-sea ring candidate
					}
					if (ComponentElongationRatio(WaterComponents[i]) > ElongationRatioThreshold)
					{
						for (const int32 CellIdx : WaterComponents[i])
						{
							Graph.Cells[CellIdx].Feature = EIHCellFeature::Land;
						}
						++ElongatedTroughFilledCount;
						ElongatedTroughFilledCells += WaterComponents[i].Num();
						continue;
					}
					// Kept as a genuine (rounded) inland sea - record for the ring-classification pass below.
					KeptWaterComponentCellCounts.Add(WaterComponents[i].Num());
					continue;
				}
				for (const int32 CellIdx : WaterComponents[i])
				{
					Graph.Cells[CellIdx].Feature = EIHCellFeature::Land;
				}
				++FilledPondCount;
				FilledPondCells += WaterComponents[i].Num();
			}

			if (FilledPondCount > 0)
			{
				UE_LOG(LogTemp, Log,
					TEXT("IH_WB_IslandActor: island=%d filled %d interior water-noise pond(s) (%lld cells) below MinLakeCellCount=%d"),
					TankIslandIndex, FilledPondCount, FilledPondCells, MinLakeCellCount);
			}
			if (BoxEdgeExcludedComponentCount > 0)
			{
				UE_LOG(LogTemp, Log,
					TEXT("IH_WB_IslandActor: island=%d excluded %d water component(s) from inland-sea classification (touches this island's own box edge - not a real enclosure)"),
					TankIslandIndex, BoxEdgeExcludedComponentCount);
			}
			if (ElongatedTroughFilledCount > 0)
			{
				UE_LOG(LogTemp, Log,
					TEXT("IH_WB_IslandActor: island=%d filled %d elongated trough-shaped water component(s) (%lld cells, elongationRatio>%.1f) - not a rounded inland sea"),
					TankIslandIndex, ElongatedTroughFilledCount, ElongatedTroughFilledCells, ElongationRatioThreshold);
			}
		}
	}

	FIHTerrainCellDiffusion::ComputeCoastalMetadata(Graph);

	TArray<TArray<FVector2D>> CoastLoops;
	FIHTerrainCellDiffusion::TraceCoastlineLoops(Graph, CoastLoops);
	if (CoastLoops.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("IH_WB_IslandActor: cell graph produced no coastline island=%d"), TankIslandIndex);
		return;
	}

	// Plan Addendum 3 diagnostic: top 5 loop sizes (not just the single largest) - confirms or
	// refutes the accent-hill hypothesis (a real second/third landmass reads as a size-comparable
	// loop, not just more small noise) before touching the seed window blind.
	{
		TArray<int32> LoopSizes;
		LoopSizes.Reserve(CoastLoops.Num());
		for (const TArray<FVector2D>& Loop : CoastLoops)
		{
			LoopSizes.Add(Loop.Num());
		}
		LoopSizes.Sort([](const int32 A, const int32 B) { return A > B; });
		const int32 TopN = FMath::Min(5, LoopSizes.Num());
		FString TopSizesStr;
		for (int32 i = 0; i < TopN; ++i)
		{
			TopSizesStr += (i > 0 ? TEXT(",") : TEXT("")) + FString::FromInt(LoopSizes[i]);
		}
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: island=%d topLoopSizes=[%s] totalLoops=%d"),
			TankIslandIndex, *TopSizesStr, CoastLoops.Num());
	}

	// Main loop = largest by ENCLOSED AREA (plan Addendum 8) - was largest by vertex count, which
	// is not the same thing: a small, kept islet with a jagged/convoluted outline can out-count a
	// smoother, far-bigger true landmass's outer loop. Area matches the criterion the IH-DEC-020
	// islet-budget filter above already uses (component cell count), so there is one consistent
	// definition of "the main landmass" instead of two that can quietly disagree.
	auto LoopShoelaceAreaAbsCm2 = [](const TArray<FVector2D>& RingLocalCm) -> double
	{
		const int32 N = RingLocalCm.Num();
		if (N < 3)
		{
			return 0.0;
		}
		double Sum = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = RingLocalCm[i];
			const FVector2D& B = RingLocalCm[(i + 1) % N];
			Sum += (A.X * B.Y - B.X * A.Y);
		}
		return FMath::Abs(Sum) * 0.5;
	};
	int32 MainLoopIdx = 0;
	double MainLoopAreaAbsCm2 = LoopShoelaceAreaAbsCm2(CoastLoops[0]);
	for (int32 i = 1; i < CoastLoops.Num(); ++i)
	{
		const double AreaAbsCm2 = LoopShoelaceAreaAbsCm2(CoastLoops[i]);
		if (AreaAbsCm2 > MainLoopAreaAbsCm2)
		{
			MainLoopIdx = i;
			MainLoopAreaAbsCm2 = AreaAbsCm2;
		}
	}
	MainCoastPolylineLocalCm = CoastLoops[MainLoopIdx];

	// Diagnostic: a topologically self-intersecting closed loop (possible if TraceBoundaryLoops'
	// edge-chaining picks the wrong branch at a vertex where 3+ boundary segments meet) can have a
	// shoelace area that partially cancels between its overlapping wound regions - undershooting
	// its true visual size, sometimes drastically. Confirmed live: one island's calibration
	// (below) came out ~109x smaller than every sibling island's, meaning MainLoopAreaAbsCm2 was
	// far too small for a ring supposedly covering thousands of cells. Logged unconditionally
	// (not gated) so a real occurrence is never missed.
	{
		bool bMainRingSelfIntersects = false;
		const int32 N = MainCoastPolylineLocalCm.Num();
		if (N >= 4)
		{
			auto Cross2D = [](const FVector2D& O, const FVector2D& P1, const FVector2D& P2) -> double
			{
				return (P1.X - O.X) * (P2.Y - O.Y) - (P1.Y - O.Y) * (P2.X - O.X);
			};
			for (int32 i = 0; i < N && !bMainRingSelfIntersects; ++i)
			{
				const FVector2D& A = MainCoastPolylineLocalCm[i];
				const FVector2D& B = MainCoastPolylineLocalCm[(i + 1) % N];
				for (int32 j = i + 1; j < N; ++j)
				{
					if (j == i || (j + 1) % N == i || (i + 1) % N == j)
					{
						continue; // adjacent segments share a vertex - not a real crossing
					}
					const FVector2D& C = MainCoastPolylineLocalCm[j];
					const FVector2D& D = MainCoastPolylineLocalCm[(j + 1) % N];
					const double D1 = Cross2D(C, D, A);
					const double D2 = Cross2D(C, D, B);
					const double D3 = Cross2D(A, B, C);
					const double D4 = Cross2D(A, B, D);
					if (((D1 > 0.0) != (D2 > 0.0)) && ((D3 > 0.0) != (D4 > 0.0)))
					{
						bMainRingSelfIntersects = true;
						break;
					}
				}
			}
		}
		// Unconditional (not gated on the check firing) so the main ring's own numbers are always
		// visible for comparison against MainComponentCellCountForRingClassification - a self-
		// intersecting AND a merely-wrong-ring-picked case both show up as an implausibly small
		// area for the reported cell count, even if this specific pairwise-crossing test misses it.
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: island=%d mainRing verts=%d areaCm2=%.0f selfIntersects=%d"),
			TankIslandIndex, N, MainLoopAreaAbsCm2, bMainRingSelfIntersects ? 1 : 0);
	}

	// Classify each secondary ring's interior as Land (islet - same gold treatment as the main
	// coastline) or Water (an enclosed inland-sea/lake hole, IH-DEC-032 - distinct minimap color/
	// thickness downstream) by matching the ring's OWN shoelace area against the known cell counts
	// of every surviving land islet / kept inland-sea water component (hoisted above). Deliberately
	// NOT a point-location test (nearest-cell to a ring centroid, or any point-in-polygon variant) -
	// this codebase's own prior lesson (Plan Addendum 10, SelectionReticle) is that a naive
	// vertex-averaged centroid can land outside a concave ring entirely, misclassifying it.
	// Area-matching against already-known, exact (topology-derived) component sizes sidesteps ring
	// geometry/concavity altogether - only the TYPE (land vs water) match matters, not which
	// specific same-type component it is, so a same-type near-tie is harmless.
	//
	// Calibration is deliberately NOT derived from MainLoopAreaAbsCm2 / MainComponentCellCount (a
	// prior version was - live log evidence showed it collapse ~109x on an island whose main ring
	// was suspected self-intersecting, corrupting classification for every OTHER ring on that
	// island too, since every candidate prediction shrank by the same broken factor). Per-cell area
	// is a property of the generation PARAMETERS (TargetCellWidthCm), not of any single ring's
	// geometry - computing it directly from the input is immune to any one ring's own defects.
	const double PerCellAreaCm2 = BuildParams.TargetCellWidthCm * BuildParams.TargetCellWidthCm;
	if (MainComponentCellCountForRingClassification > 0)
	{
		const double RingDerivedPerCellAreaCm2 =
			MainLoopAreaAbsCm2 / static_cast<double>(MainComponentCellCountForRingClassification);
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: island=%d perCellAreaCm2 paramDerived=%.0f ringDerived=%.0f ratio=%.2f"),
			TankIslandIndex, PerCellAreaCm2, RingDerivedPerCellAreaCm2,
			RingDerivedPerCellAreaCm2 > 0.0 ? PerCellAreaCm2 / RingDerivedPerCellAreaCm2 : 0.0);
	}
	auto ClassifyRingIsInlandSea = [&](const TArray<FVector2D>& RingLocalCm, double& OutRingAreaCm2) -> bool
	{
		OutRingAreaCm2 = LoopShoelaceAreaAbsCm2(RingLocalCm);
		if (PerCellAreaCm2 <= 0.0)
		{
			return false; // no calibration available - default to islet treatment, never silently drop the ring
		}
		double BestDeltaCm2 = TNumericLimits<double>::Max();
		bool bBestIsWater = false;
		for (const int32 CellCount : KeptLandComponentCellCounts)
		{
			const double DeltaCm2 = FMath::Abs(OutRingAreaCm2 - CellCount * PerCellAreaCm2);
			if (DeltaCm2 < BestDeltaCm2)
			{
				BestDeltaCm2 = DeltaCm2;
				bBestIsWater = false;
			}
		}
		for (const int32 CellCount : KeptWaterComponentCellCounts)
		{
			const double DeltaCm2 = FMath::Abs(OutRingAreaCm2 - CellCount * PerCellAreaCm2);
			if (DeltaCm2 < BestDeltaCm2)
			{
				BestDeltaCm2 = DeltaCm2;
				bBestIsWater = true;
			}
		}
		return bBestIsWater;
	};

	ContourGoldRingsLocalCm.Reset();
	ContourGoldRingsIsInlandSea.Reset();
	ContourGoldRingsLocalCm.Add(MainCoastPolylineLocalCm);
	ContourGoldRingsIsInlandSea.Add(false);
	for (int32 i = 0; i < CoastLoops.Num(); ++i)
	{
		if (i != MainLoopIdx)
		{
			double RingAreaCm2 = 0.0;
			const bool bIsInlandSea = ClassifyRingIsInlandSea(CoastLoops[i], RingAreaCm2);
			ContourGoldRingsLocalCm.Add(CoastLoops[i]);
			ContourGoldRingsIsInlandSea.Add(bIsInlandSea);
			UE_LOG(LogTemp, Log,
				TEXT("IH_WB_IslandActor: island=%d ring[%d] verts=%d areaCm2=%.0f perCellAreaCm2=%.0f classifiedInlandSea=%d"),
				TankIslandIndex, i, CoastLoops[i].Num(), RingAreaCm2, PerCellAreaCm2, bIsInlandSea ? 1 : 0);
		}
	}

	// WWF shelf-equivalent: offshore ring a few hops out from the coast (direct cell-boundary
	// trace - immune to the self-crossing failure the legacy polyline-offset approach had).
	//
	// Plan Addendum 9: was a naive vertex average, which is not the same thing as the polygon's
	// true center of mass - a jagged, densely-vertexed stretch of these deliberately-irregular
	// coastlines pulls a plain average toward itself, away from the actual centroid. Use the
	// standard area-weighted polygon centroid (shoelace-derived Cx/Cy) instead; falls back to the
	// vertex average only for degenerate near-zero-area rings, where the area formula is unstable.
	auto RingCentroid = [](const TArray<FVector2D>& RingLocalCm) -> FVector2D
	{
		const int32 N = RingLocalCm.Num();
		if (N == 0)
		{
			return FVector2D::ZeroVector;
		}
		double AreaAcc = 0.0;
		double CxAcc = 0.0;
		double CyAcc = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = RingLocalCm[i];
			const FVector2D& B = RingLocalCm[(i + 1) % N];
			const double Cross = A.X * B.Y - B.X * A.Y;
			AreaAcc += Cross;
			CxAcc += (A.X + B.X) * Cross;
			CyAcc += (A.Y + B.Y) * Cross;
		}
		const double Area = AreaAcc * 0.5;
		if (FMath::Abs(Area) > KINDA_SMALL_NUMBER)
		{
			return FVector2D(CxAcc / (6.0 * Area), CyAcc / (6.0 * Area));
		}
		FVector2D VertexAvg = FVector2D::ZeroVector;
		for (const FVector2D& P : RingLocalCm)
		{
			VertexAvg += P;
		}
		return VertexAvg / static_cast<double>(N);
	};
	const FVector2D MainCoastCentroidCm = RingCentroid(MainCoastPolylineLocalCm);

	// IH-DEC-039: tried swapping this from the topological 5-BFS-hop trace to a real elevation
	// isoline at a "-25m ASL" raw-Height threshold (derived via the same Height<->Zcm scale the
	// land mesh uses for its own visual Z). Reverted after a headless self-check + PIE screenshot
	// showed that scale is land-visual-calibrated, not physically meaningful once projected onto
	// Ocean cells: for several test islands it produced a threshold so permissive that "Height >=
	// threshold" was true almost everywhere in the graph except the deepest troughs, so the traced
	// isoline ballooned to nearly the size of the whole island-local bounding box instead of a
	// coastal band - confirmed unmistakably via a temporary magenta-debug-color PIE grab. The
	// topological BFS-hop metric below has no such failure mode (it's bounded by construction,
	// independent of any Height/Zcm scale) and is the same mechanism already proven safe for the
	// coastline trace itself - kept as the outer-ring source for that reason.
	TArray<TArray<FVector2D>> ShelfLoops;
	FIHTerrainCellDiffusion::TraceCoastDistanceBoundaryLoops(Graph, /*MinCoastDistance=*/-5, ShelfLoops);

	// Bug 2 (plan Addendum 1): with hundreds of loops in the graph (Bug 3), independently picking
	// "largest loop" for a coastline and its shelf ring can and did select two unrelated fragments
	// 10+ km apart (confirmed via the ring-centroid diagnostic logged below) - the loft between
	// them then stretched a degenerate spike across empty ocean. Pick the shelf loop whose
	// centroid is closest to the GIVEN coastline ring's own centroid, among loops big enough to
	// plausibly be a real ring (>=6 verts) rather than a noise speck; fall back to largest if
	// nothing qualifies. Shared here (was main-ring-only inline logic) so it can be reused once
	// per surviving land ring below - main island AND every islet - not just the main island.
	auto PickNearestShelfLoop = [&ShelfLoops, &RingCentroid](const FVector2D& CoastCentroidCm) -> TArray<FVector2D>
	{
		if (ShelfLoops.Num() == 0)
		{
			return TArray<FVector2D>();
		}
		int32 BestIdx = INDEX_NONE;
		double BestDistSqCm = TNumericLimits<double>::Max();
		for (int32 i = 0; i < ShelfLoops.Num(); ++i)
		{
			if (ShelfLoops[i].Num() < 6)
			{
				continue;
			}
			const double DistSqCm = FVector2D::DistSquared(RingCentroid(ShelfLoops[i]), CoastCentroidCm);
			if (DistSqCm < BestDistSqCm)
			{
				BestDistSqCm = DistSqCm;
				BestIdx = i;
			}
		}
		if (BestIdx == INDEX_NONE)
		{
			BestIdx = 0;
			for (int32 i = 1; i < ShelfLoops.Num(); ++i)
			{
				if (ShelfLoops[i].Num() > ShelfLoops[BestIdx].Num())
				{
					BestIdx = i;
				}
			}
		}
		return ShelfLoops[BestIdx];
	};

	ShelfPolylineLocalCm = PickNearestShelfLoop(MainCoastCentroidCm);
	// ShelfPolylineLocalCm/ShelfLoops/PickNearestShelfLoop above are no longer consumed by mesh
	// generation (IH-DEC-039 per-cell revision — BuildWwfShelfSection now iterates Graph.Cells
	// directly instead of pairing traced Inner/Outer polylines) but are kept: ShelfPolylineLocalCm
	// still feeds the diagnostic ring-bounds log and the WWF footprint-acreage estimate below.

	// HeightsMeters/SamplesPerSide intentionally left empty this pass - the raster-grid ribbon/
	// slope-sampling code this file also owns (BakeAslContourRibbons, BuildSeaShelfExtentFrom-
	// ShelfSegments' slope sampler) all guard on HeightsMeters.Num()==0 and degrade gracefully
	// (flat/zero height) rather than crash; ribbon fidelity is a follow-up pass, not required to
	// see real cell-graph island shape.
	HeightsMeters.Reset();
	SamplesPerSide = 0;
	HalfExtentMeters = static_cast<double>(BuildParams.HalfExtentXCm) * 0.01;
	SampleSpacingMeters = static_cast<double>(BuildParams.TargetCellWidthCm) * 0.01;
	RiverTerminusSockets.Reset(); // River receiving stays disabled until Hydrology (IH-DEC-025).
	ContourShelfRingsLocalCm.Reset();
	ContourPlus25RingsLocalCm.Reset();
	CoastCharacterRing.Reset();
	GovernedWwfOuterLocalCm.Reset();
	ContourGovernedWwfRingsLocalCm.Reset();
	SeaRootsExtent = FIHSeaRootsExtent();
	bHasSeaRootsExtent = false;
	bAslContourRibbonsBaked = false;
	bFeatureRibbonsBaked = false;
	if (ContourRibbonMesh)
	{
		ContourRibbonMesh->ClearAllMeshSections();
		ContourRibbonMesh->SetVisibility(false);
		ContourRibbonMesh->SetHiddenInGame(true);
	}
	if (FeatureRibbonMesh)
	{
		FeatureRibbonMesh->ClearAllMeshSections();
		FeatureRibbonMesh->SetVisibility(false);
		FeatureRibbonMesh->SetHiddenInGame(true);
	}

	// Land cell fan-triangulation (Voronoi cells are always convex - winding chosen per-triangle
	// so the normal faces +Z, ported from the verified preview-actor pattern that fixed this
	// exact backface-culling defect earlier this session).
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;

	double MaxLandHeight = LandThreshold;
	int64 LandCellCount = 0;
	for (const FIHTerrainCell& Cell : Graph.Cells)
	{
		if (Cell.Feature == EIHCellFeature::Land)
		{
			++LandCellCount;
			MaxLandHeight = FMath::Max(MaxLandHeight, Cell.Height);
		}
	}
	const double HeightSpan = FMath::Max(MaxLandHeight - LandThreshold, 1.0);

	// Frame-artifact investigation: general degenerate-cell scan across EVERY cell (Land and Ocean
	// alike), independent of any shelf-mesh logic - box-edge-clip exclusion and jitter-fallback
	// counts both measured zero on real seeds that still show the "frame" artifact (IH-DEC-039
	// follow-up), and the artifact persisted identically under the old ring-loft AND the new
	// per-cell shelf code, so the true source must be a property of Cell.Boundary itself, not of
	// either mesh-building approach. A normal jittered cell's boundary bounding box should be on
	// the order of TargetCellWidthCm across; log any cell whose boundary bounding box exceeds
	// DegenerateAspectFactor times that in either axis as a candidate sliver, with a few sample
	// positions so they can be checked against where the artifact actually appears.
	{
		constexpr double DegenerateAspectFactor = 4.0;
		const double DegenerateThresholdCm = BuildParams.TargetCellWidthCm * DegenerateAspectFactor;
		int32 DegenerateCellCount = 0;
		constexpr int32 MaxSamplesToLog = 8;
		for (const FIHTerrainCell& Cell : Graph.Cells)
		{
			if (Cell.Boundary.Num() < 3)
			{
				continue;
			}
			FVector2D BoundsMin(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
			FVector2D BoundsMax(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
			for (const FVector2D& P : Cell.Boundary)
			{
				BoundsMin = FVector2D::Min(BoundsMin, P);
				BoundsMax = FVector2D::Max(BoundsMax, P);
			}
			const double WidthCm = BoundsMax.X - BoundsMin.X;
			const double HeightCm = BoundsMax.Y - BoundsMin.Y;
			if (WidthCm > DegenerateThresholdCm || HeightCm > DegenerateThresholdCm)
			{
				++DegenerateCellCount;
				if (DegenerateCellCount <= MaxSamplesToLog)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("IH_WB_IslandActor: island=%d degenerateCell feature=%d widthCm=%.0f heightCm=%.0f ")
						TEXT("centroidLocalCm=(%.0f,%.0f) coastDistance=%d boundaryVerts=%d"),
						TankIslandIndex, static_cast<int32>(Cell.Feature), WidthCm, HeightCm,
						(BoundsMin.X + BoundsMax.X) * 0.5, (BoundsMin.Y + BoundsMax.Y) * 0.5,
						Cell.CoastDistance, Cell.Boundary.Num());
				}
			}
		}
		if (DegenerateCellCount > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("IH_WB_IslandActor: island=%d degenerateCellScan count=%d thresholdCm=%.0f totalCells=%d"),
				TankIslandIndex, DegenerateCellCount, DegenerateThresholdCm, Graph.Num());
		}
	}

	// ShelfFloorHeightRaw is already computed above (before the WWF shelf ring-picking code, which
	// needs it earlier than this point) - reused here as-is for the Sector Fabric prototype below;
	// it's numerically identical to what a MaxLandHeight/HeightSpan computed at this later point
	// would give, since Cell.Feature/.Height are unchanged in between.

	// Contour-Guided Sector Fabric prototype touchpoint (IH-DEC-023/027/029/031). Read-only pass -
	// Graph.Cells is never mutated here (only Mesh-building state below is). Gameplay-Sector data
	// layer prototype only - NOT the WWF shelf ramp mesh above, which reverted to a topological
	// (CoastDistance) shelf-floor metric after this same land-visual Height<->Zcm scale was found to
	// produce a pathologically permissive threshold on some islands (IH-DEC-039). Kept here only
	// because this prototype is dev-flag-gated off by default and already independently verified at
	// this scale in IH-DEC-038 - not proof the scale is safe in general.
	if (IHInvisibleHandSpec::IsDevDemoSectorFabricPrototypeEnabled())
	{
		constexpr double ShelfFloorZcm = -2500.0; // -25m ASL
		const double HeightToZcmScale = SummitTopZCm / HeightSpan; // Zcm per raw Height unit
		const double ShelfFloorHeightRaw = LandThreshold + ShelfFloorZcm / HeightToZcmScale;

		TArray<FIHSectorFabricCell> Sectors;
		FIHSectorFabricParams SectorParams; // defaults for now; tune once self-tested
		const double SectorFabricStartS = FPlatformTime::Seconds();
		FIHSectorFabric::BuildSectorFabricForRegion(
			Graph, ShelfFloorHeightRaw, MaxLandHeight, LandThreshold, HeightSpan, SummitTopZCm,
			SectorParams, Graph.BoundsMinLocalCm, Graph.BoundsMaxLocalCm, Stream, Sectors);

		int32 SteepCount = 0, ModerateCount = 0, FlatCount = 0, LandCount = 0, ShelfCount = 0;
		double SumAreaCm2 = 0.0, SumRadialRunCm = 0.0, SumAlongWidthCm = 0.0;
		for (const FIHSectorFabricCell& Sector : Sectors)
		{
			SumAreaCm2 += Sector.AreaCm2;
			SumRadialRunCm += Sector.RadialRunCm;
			SumAlongWidthCm += Sector.AlongContourWidthCm;
			switch (Sector.SlopeClass)
			{
			case EIHSectorSlopeClass::Steep: ++SteepCount; break;
			case EIHSectorSlopeClass::Moderate: ++ModerateCount; break;
			default: ++FlatCount; break;
			}
			(Sector.FeatureType == EIHCellFeature::Land ? LandCount : ShelfCount)++;
		}
		const int32 SectorCountForAvg = FMath::Max(1, Sectors.Num());
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: island=%d sectorFabricPrototype sectors=%d avgAreaAcres=%.2f ")
			TEXT("avgRadialRunCm=%.0f avgAlongContourWidthCm=%.0f steep=%d moderate=%d flat=%d ")
			TEXT("land=%d shelf=%d elapsedS=%.3f"),
			TankIslandIndex, Sectors.Num(), (SumAreaCm2 / SectorCountForAvg) / 40468564.224,
			SumRadialRunCm / SectorCountForAvg, SumAlongWidthCm / SectorCountForAvg,
			SteepCount, ModerateCount, FlatCount, LandCount, ShelfCount,
			FPlatformTime::Seconds() - SectorFabricStartS);

		// Bare-bones DrawDebugLine viz per polygon edge, colored by SlopeClass - NOT decal/PCG/
		// zoom-tier infra (neither exists in this codebase today); a dev gut-check only, matching
		// the existing debug-draw precedent used elsewhere (e.g. IH_TownGridOverlayComponent).
		if (UWorld* World = GetWorld())
		{
			for (const FIHSectorFabricCell& Sector : Sectors)
			{
				const FColor EdgeColor = Sector.SlopeClass == EIHSectorSlopeClass::Steep ? FColor::Red
					: Sector.SlopeClass == EIHSectorSlopeClass::Moderate ? FColor::Yellow : FColor::Green;
				for (int32 VertIdx = 0; VertIdx < Sector.PolygonVerts.Num(); ++VertIdx)
				{
					const FVector2D& A = Sector.PolygonVerts[VertIdx];
					const FVector2D& B = Sector.PolygonVerts[(VertIdx + 1) % Sector.PolygonVerts.Num()];
					DrawDebugLine(World,
						GetActorTransform().TransformPosition(FVector(A.X, A.Y, SummitTopZCm + 200.f)),
						GetActorTransform().TransformPosition(FVector(B.X, B.Y, SummitTopZCm + 200.f)),
						EdgeColor, /*bPersistent=*/true, /*LifeTime=*/-1.f, /*DepthPriority=*/0, /*Thickness=*/40.f);
				}
			}
		}
	}

	// Plan Addendum 12: per-cell-flat height/normals produced a hard-edged "stepped plateau" look
	// (each Voronoi cell rendering as one flat tile, confirmed directly from this exact loop before
	// this change). Smooth across cell boundaries instead: average the height of every LAND cell
	// sharing a boundary corner (Voronoi corners are shared by ~3 Delaunay-adjacent cells'
	// generating points) before normalizing to Z, and recompute normals from the now-smoothed
	// surface via standard face-normal accumulation. Quantized-position keying (not real shared-
	// vertex indices - each cell still emits its own copy of each boundary point) welds duplicate
	// corners from adjacent cells together for both passes. Local averaging only (one corner's ~3
	// neighbors), so small per-cell steps smooth out while broader slopes/cliffs (spanning many
	// cells) are preserved - doesn't touch the underlying cell graph, classification, or coastline.
	TMap<FIntPoint, TPair<double, int32>> VertexHeightAccum;
	auto QuantizeVertexKey = [](const FVector2D& P) -> FIntPoint
	{
		constexpr double QuantCm = 1.0; // 1cm grid - true shared corners coincide well within this
		return FIntPoint(FMath::RoundToInt32(P.X / QuantCm), FMath::RoundToInt32(P.Y / QuantCm));
	};
	for (const FIHTerrainCell& Cell : Graph.Cells)
	{
		if (Cell.Feature != EIHCellFeature::Land)
		{
			continue;
		}
		for (const FVector2D& P : Cell.Boundary)
		{
			TPair<double, int32>& Accum = VertexHeightAccum.FindOrAdd(QuantizeVertexKey(P));
			Accum.Key += Cell.Height;
			Accum.Value += 1;
		}
	}
	auto SmoothedHeightAt = [&VertexHeightAccum, &QuantizeVertexKey](const FVector2D& P, double Fallback) -> double
	{
		if (const TPair<double, int32>* Accum = VertexHeightAccum.Find(QuantizeVertexKey(P)))
		{
			return Accum->Value > 0 ? Accum->Key / Accum->Value : Fallback;
		}
		return Fallback;
	};

	for (const FIHTerrainCell& Cell : Graph.Cells)
	{
		if (Cell.Feature != EIHCellFeature::Land || Cell.Boundary.Num() < 3)
		{
			continue;
		}

		const int32 BaseVertId = Vertices.Num();
		for (const FVector2D& P : Cell.Boundary)
		{
			const double SmoothedRaw = SmoothedHeightAt(P, Cell.Height);
			const double LinearNormalizedHeight = FMath::Clamp((SmoothedRaw - LandThreshold) / HeightSpan, 0.0, 1.0);
			// IH-DEC-058: DiffuseFromSeeds' power-law falloff is peaked (most land area sits near
			// LandThreshold, a small summit reaches the top) - scale-invariant by construction, so
			// this isn't a per-island-size bug like IH-DEC-055a/057. A uniform multiplicative boost
			// of raw land height would be a no-op here: HeightSpan is itself derived from the same
			// land cells' max, so scaling every land cell by the same factor cancels out exactly
			// after normalization. Reshaping the already-computed 0-1 NormalizedHeight with a gamma
			// curve instead (Gamma<1 lifts low/mid elevations, Gamma=1 = prior behavior) fixes the
			// distribution's SHAPE directly, strictly after ClassifyLandWater has already locked in
			// Land vs. Ocean per cell (~line 3213) - this can never move the coastline, change dry
			// acreage, or add coastline loops (unlike the reverted IH-DEC-057 approach), and the
			// single highest cell still hits SummitTopZCm exactly (Pow(1.0, Gamma) == 1.0).
			// Shared constant (IH_WB_IslandActorPrivate::IslandHeightReshapeGamma) - BuildWwfShelfSection's
			// coastal seam-matching formula must use the exact same value, see its own comment.
			const double NormalizedHeight = FMath::Pow(
				LinearNormalizedHeight, IH_WB_IslandActorPrivate::IslandHeightReshapeGamma);
			const float ZCm = FMath::Max(1.f, static_cast<float>(NormalizedHeight) * SummitTopZCm);
			Vertices.Add(FVector(P.X, P.Y, ZCm));
			Normals.Add(FVector(0.0, 0.0, 1.0)); // placeholder - recomputed below from the smoothed surface
			UV0.Add(FVector2D(P.X / 100.0, P.Y / 100.0));
		}

		const FVector2D& P0 = Cell.Boundary[0];
		for (int32 i = 1; i + 1 < Cell.Boundary.Num(); ++i)
		{
			const FVector2D& Pi = Cell.Boundary[i];
			const FVector2D& Pi1 = Cell.Boundary[i + 1];
			// Winding flipped from the dev-preview actor's Addendum-10 logic: that logic was
			// never actually confirmed visible in PIE (only headless bounds/tri-count checked -
			// see plan Addendum 1), and the first real PIE grab of this pipeline showed terrain
			// lit only from BELOW (-696m ASL), i.e. backface-culled from above with the old sign.
			const double Cross = (Pi.X - P0.X) * (Pi1.Y - P0.Y) - (Pi.Y - P0.Y) * (Pi1.X - P0.X);
			if (Cross >= 0.0)
			{
				Triangles.Add(BaseVertId); Triangles.Add(BaseVertId + i + 1); Triangles.Add(BaseVertId + i);
			}
			else
			{
				Triangles.Add(BaseVertId); Triangles.Add(BaseVertId + i); Triangles.Add(BaseVertId + i + 1);
			}
		}
	}

	// Smooth normals across cell boundaries too, using the same quantized-position grouping so
	// duplicate-position vertices from adjacent cells end up with matching normals - seamless
	// shading across the smoothed surface instead of a flat-shaded facet look per cell.
	{
		TMap<FIntPoint, FVector> NormalAccum;
		for (int32 TriIdx = 0; TriIdx + 2 < Triangles.Num(); TriIdx += 3)
		{
			const FVector& A = Vertices[Triangles[TriIdx]];
			const FVector& B = Vertices[Triangles[TriIdx + 1]];
			const FVector& C = Vertices[Triangles[TriIdx + 2]];
			// Plan Addendum 13: CrossProduct(B-A, C-A) computed the Z-component as -Cross relative
			// to the winding-selection formula above (Cross >= 0.0 branch) - the OPPOSITE sign of
			// the "+Z-facing" convention that winding was chosen to produce (confirmed algebraically
			// via cross-product anti-commutativity, not guessed). Every smoothed normal pointed into
			// the ground instead of up, starving the terrain of direct light - the "murky haze" this
			// round. Swapped operand order to match the established convention.
			const FVector FaceNormal = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
			for (int32 k = 0; k < 3; ++k)
			{
				const FVector& V = Vertices[Triangles[TriIdx + k]];
				NormalAccum.FindOrAdd(QuantizeVertexKey(FVector2D(V.X, V.Y)), FVector::ZeroVector) += FaceNormal;
			}
		}
		for (int32 VertIdx = 0; VertIdx < Vertices.Num(); ++VertIdx)
		{
			if (const FVector* Accum = NormalAccum.Find(QuantizeVertexKey(FVector2D(Vertices[VertIdx].X, Vertices[VertIdx].Y))))
			{
				const FVector Smoothed = Accum->GetSafeNormal();
				Normals[VertIdx] = Smoothed.IsNearlyZero() ? FVector(0.0, 0.0, 1.0) : Smoothed;
			}
		}
	}

	Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), Vertices.Num());

	int32 ClassifiedBiomeTris = 0;
	int32 DistinctBiomeCount = 0;
	int32 MixedClampTris = 0;
	IH_WB_IslandActorPrivate::ApplyDtBiomeColorBands(
		IslandMesh, this, Vertices, Triangles, Normals, UV0, Tangents,
		ClassifiedBiomeTris, DistinctBiomeCount, MixedClampTris);

	// Diagnostic (plan Addendum 1, Bug 2): MainCoastPolylineLocalCm and ShelfPolylineLocalCm are
	// each picked independently as "largest loop" from graphs that can have hundreds of loops
	// (Bug 3) - if they don't spatially correspond to the same landmass, the loft below stretches
	// a band between two unrelated locations. Log both rings' centroid/bounds so the next grab's
	// sky-spike artifacts (if still present) can be checked against real numbers instead of guessed.
	auto LogRingCentroidBounds = [](const TCHAR* Label, const TArray<FVector2D>& RingLocalCm)
	{
		if (RingLocalCm.Num() == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("IH_WB_IslandActor: ring %s EMPTY"), Label);
			return;
		}
		FVector2D Centroid = FVector2D::ZeroVector;
		FVector2D BoundsMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
		FVector2D BoundsMax(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
		for (const FVector2D& P : RingLocalCm)
		{
			Centroid += P;
			BoundsMin = FVector2D::Min(BoundsMin, P);
			BoundsMax = FVector2D::Max(BoundsMax, P);
		}
		Centroid /= static_cast<double>(RingLocalCm.Num());
		UE_LOG(LogTemp, Log,
			TEXT("IH_WB_IslandActor: ring %s verts=%d centroidCm=(%.0f,%.0f) boundsMinCm=(%.0f,%.0f) boundsMaxCm=(%.0f,%.0f)"),
			Label, RingLocalCm.Num(), Centroid.X, Centroid.Y, BoundsMin.X, BoundsMin.Y, BoundsMax.X, BoundsMax.Y);
	};
	LogRingCentroidBounds(TEXT("MainCoast"), MainCoastPolylineLocalCm);
	LogRingCentroidBounds(TEXT("Shelf"), ShelfPolylineLocalCm);

	int32 ShelfTriCount = 0;
	int32 ShelfSlopedBandCellCount = 0;
	int32 ShelfBottomPlaneCellCount = 0;
	int32 ShelfBoxEdgeExcludedCellCount = 0;
	IH_WB_IslandActorPrivate::BuildWwfShelfSection(
		ShelfMesh, this, Graph,
		/*BottomPlaneMinCoastDistance=*/-5, /*BottomPlaneMaxDepthCoastDistance=*/-15,
		ShelfTriCount, ShelfSlopedBandCellCount, ShelfBottomPlaneCellCount, ShelfBoxEdgeExcludedCellCount,
		VertexHeightAccum, LandThreshold, HeightSpan, SummitTopZCm);

	const double TotalAreaM2 =
		(2.0 * BuildParams.HalfExtentXCm * 0.01) * (2.0 * BuildParams.HalfExtentYCm * 0.01);
	const double LandFraction = Graph.Num() > 0 ? static_cast<double>(LandCellCount) / Graph.Num() : 0.0;
	const double DryAcres = TotalAreaM2 * LandFraction / IHInvisibleHandSpec::InternationalAcreSquareMeters;

	auto ShoelaceAreaM2 = [](const TArray<FVector2D>& RingLocalCm) -> double
	{
		const int32 N = RingLocalCm.Num();
		if (N < 3)
		{
			return 0.0;
		}
		double Sum = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = RingLocalCm[i];
			const FVector2D& B = RingLocalCm[(i + 1) % N];
			Sum += (A.X * B.Y - B.X * A.Y);
		}
		return FMath::Abs(Sum) * 0.5 * 1.0e-4; // cm^2 -> m^2
	};
	const double WwfFootprintAcres =
		ShoelaceAreaM2(ShelfPolylineLocalCm) / IHInvisibleHandSpec::InternationalAcreSquareMeters;

	if (SelectionReticle)
	{
		// Plan Addendum 10: the coastline-polygon centroid (MainCoastCentroidCm, area-weighted as
		// of Addendum 9) is mathematically correct but not robust to these deliberately concave,
		// inlet-carved coastlines - it can land right at a bay/notch rather than "in" the visible
		// landmass. MainLandCentroidLocalCm (cell-averaged over the main connected component,
		// already computed above for the islet-distance cap) directly reflects where the land IS,
		// immune to that pathology - use it instead. Vertical (+Z) orientation keeps the marker
		// readable from directly overhead (Top-Down), not just oblique views.
		SelectionReticle->SetRelativeLocation(
			FVector(MainLandCentroidLocalCm.X, MainLandCentroidLocalCm.Y, FMath::Max(SummitTopZCm, 500.f)));
		SelectionReticle->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	}

	UE_LOG(LogTemp, Log,
		TEXT("IH_WB_IslandActor: island=%d cellGraph cells=%d landFraction=%.3f dryAcres=%.0f wwfAcres=%.0f ")
		TEXT("wwfSlopedBandCells=%d wwfBottomPlaneCells=%d primaryTroughs=%d daughterTroughs=%d loops=%d ")
		TEXT("biomeTris=%d distinctBiomes=%d shelfTris=%d elapsedS=%.3f"),
		TankIslandIndex, Graph.Num(), LandFraction, DryAcres, WwfFootprintAcres,
		ShelfSlopedBandCellCount, ShelfBottomPlaneCellCount,
		PrimaryTroughPaths.Num(), DaughterTroughCount, CoastLoops.Num(),
		ClassifiedBiomeTris, DistinctBiomeCount,
		ShelfTriCount, FPlatformTime::Seconds() - StartSeconds);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			Nav->SetIslandWwfAcres(TankIslandIndex, FMath::Max(0, FMath::RoundToInt(static_cast<float>(WwfFootprintAcres))));
			// Plan Addendum 9: replace the frozen pre-bake phi-budget target with the realized
			// figure, same value already feeding the dryAcres= log line above.
			Nav->SetIslandDryAcres(TankIslandIndex, FMath::Max(0, FMath::RoundToInt(static_cast<float>(DryAcres))));
		}
	}

	// Contours / Features checkboxes can stay ON across rebuild - re-bake/show. Ribbons will be
	// flat (HeightsMeters empty this pass) but will not crash - see guard comment above.
	if (IHInvisibleHandSpec::IsDevDemoAslContourLinesEnabled())
	{
		EnsureAslContourRibbonsBaked();
		ApplyDevContoursVisibility(true);
	}
#if !UE_BUILD_SHIPPING
	if (IHDevViewRuntime::AreFeaturesVisible())
	{
		ApplyDevFeaturesVisibility(true);
	}
#endif
}

void AIH_WB_IslandActor::RebuildRiverTerminusSocketMarkers()
{
	for (UArrowComponent* Arrow : RiverTerminusSocketMarkers)
	{
		if (Arrow)
		{
			Arrow->DestroyComponent();
		}
	}
	RiverTerminusSocketMarkers.Reset();

#if !UE_BUILD_SHIPPING
	RiverTerminusSocketMarkers.Reserve(RiverTerminusSockets.Num());
	for (const FIHRiverTerminusSocket& Socket : RiverTerminusSockets)
	{
		UArrowComponent* Arrow = NewObject<UArrowComponent>(this,
			*FString::Printf(TEXT("RiverTerminusSocket_%d"), Socket.SocketId));
		if (!Arrow)
		{
			continue;
		}
		Arrow->SetupAttachment(SceneRoot);
		Arrow->RegisterComponent();
		Arrow->SetRelativeLocation(FVector(Socket.LocationLocalCm.X, Socket.LocationLocalCm.Y, 200.f));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Socket.OutwardTangentXY.Y, Socket.OutwardTangentXY.X));
		Arrow->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		Arrow->ArrowSize = 4.f;
		Arrow->ArrowLength = 400.f;
		Arrow->ArrowColor = FColor(40, 180, 255);
		Arrow->SetHiddenInGame(false);
		RiverTerminusSocketMarkers.Add(Arrow);
	}
#endif
}

void AIH_WB_IslandActor::SegmentsToClosedPolyline(
	const TArray<TPair<FVector2D, FVector2D>>& SegmentsMeters,
	TArray<FVector2D>& OutLargestLocalCm,
	TArray<TArray<FVector2D>>* OutAllRingsLocalCm) const
{
	OutLargestLocalCm.Reset();
	if (OutAllRingsLocalCm)
	{
		OutAllRingsLocalCm->Reset();
	}
	if (SegmentsMeters.Num() == 0)
	{
		return;
	}

	// Chain marching-squares segments into closed rings; MainCoast = longest perimeter.
	const auto QuantKey = [](const FVector2D& P) -> int64
	{
		const int32 Xi = FMath::RoundToInt(P.X * 20.0); // 5 cm in meters space
		const int32 Yi = FMath::RoundToInt(P.Y * 20.0);
		return (static_cast<int64>(Xi) << 32) ^ static_cast<uint32>(Yi);
	};

	TMap<int64, TArray<int32>> Adjacency;
	TArray<uint8> Used;
	Used.Init(0, SegmentsMeters.Num());
	for (int32 I = 0; I < SegmentsMeters.Num(); ++I)
	{
		Adjacency.FindOrAdd(QuantKey(SegmentsMeters[I].Key)).Add(I);
		Adjacency.FindOrAdd(QuantKey(SegmentsMeters[I].Value)).Add(I);
	}

	auto OtherEnd = [&SegmentsMeters](const int32 Seg, const FVector2D& At) -> FVector2D
	{
		const double D0 = FVector2D::DistSquared(SegmentsMeters[Seg].Key, At);
		const double D1 = FVector2D::DistSquared(SegmentsMeters[Seg].Value, At);
		return D0 <= D1 ? SegmentsMeters[Seg].Value : SegmentsMeters[Seg].Key;
	};

	auto PerimeterCm = [](const TArray<FVector2D>& Ring) -> float
	{
		float Sum = 0.f;
		for (int32 I = 0; I + 1 < Ring.Num(); ++I)
		{
			Sum += FVector2D::Distance(Ring[I], Ring[I + 1]);
		}
		if (Ring.Num() >= 3)
		{
			Sum += FVector2D::Distance(Ring.Last(), Ring[0]);
		}
		return Sum;
	};

	auto WalkRingFrom = [&](const int32 StartSeg) -> TArray<FVector2D>
	{
		TArray<FVector2D> Ring;
		FVector2D Cursor = SegmentsMeters[StartSeg].Key;
		const int32 DegStart = Adjacency.FindRef(QuantKey(SegmentsMeters[StartSeg].Key)).Num();
		if (DegStart != 1 && Adjacency.FindRef(QuantKey(SegmentsMeters[StartSeg].Value)).Num() == 1)
		{
			Cursor = SegmentsMeters[StartSeg].Value;
		}

		Ring.Reserve(SegmentsMeters.Num() + 1);
		Ring.Add(Cursor * 100.f);
		Used[StartSeg] = 1;
		Cursor = OtherEnd(StartSeg, Cursor);
		Ring.Add(Cursor * 100.f);

		for (int32 Guard = 0; Guard < SegmentsMeters.Num(); ++Guard)
		{
			const TArray<int32>* Candidates = Adjacency.Find(QuantKey(Cursor));
			if (!Candidates)
			{
				break;
			}
			int32 Next = INDEX_NONE;
			for (const int32 Seg : *Candidates)
			{
				if (!Used[Seg])
				{
					Next = Seg;
					break;
				}
			}
			if (Next == INDEX_NONE)
			{
				break;
			}
			Used[Next] = 1;
			Cursor = OtherEnd(Next, Cursor);
			Ring.Add(Cursor * 100.f);
		}
		return Ring;
	};

	TArray<TArray<FVector2D>> Rings;
	Rings.Reserve(8);

	// Prefer open endpoints as seeds (open arcs); then any unused segment (closed loops).
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 I = 0; I < SegmentsMeters.Num(); ++I)
		{
			if (Used[I])
			{
				continue;
			}
			if (Pass == 0)
			{
				const int32 DegA = Adjacency.FindRef(QuantKey(SegmentsMeters[I].Key)).Num();
				const int32 DegB = Adjacency.FindRef(QuantKey(SegmentsMeters[I].Value)).Num();
				if (DegA != 1 && DegB != 1)
				{
					continue;
				}
			}
			TArray<FVector2D> Ring = WalkRingFrom(I);
			if (Ring.Num() >= 4)
			{
				Rings.Add(MoveTemp(Ring));
			}
		}
	}

	if (Rings.Num() == 0)
	{
		OutLargestLocalCm.Reserve(SegmentsMeters.Num() + 1);
		for (const TPair<FVector2D, FVector2D>& Seg : SegmentsMeters)
		{
			OutLargestLocalCm.Add(Seg.Key * 100.f);
		}
		OutLargestLocalCm.Add(SegmentsMeters.Last().Value * 100.f);
		return;
	}

	Rings.Sort([&PerimeterCm](const TArray<FVector2D>& A, const TArray<FVector2D>& B)
	{
		return PerimeterCm(A) > PerimeterCm(B);
	});

	// Drop tiny noise rings (< ~50 m perimeter) but always keep the largest.
	constexpr float MinRingPerimeterCm = 5000.f;
	OutLargestLocalCm = Rings[0];
	if (OutAllRingsLocalCm)
	{
		OutAllRingsLocalCm->Reserve(Rings.Num());
		for (int32 R = 0; R < Rings.Num(); ++R)
		{
			if (R == 0 || PerimeterCm(Rings[R]) >= MinRingPerimeterCm)
			{
				OutAllRingsLocalCm->Add(Rings[R]);
			}
		}
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("IH_WB_IslandActor: SegmentsToClosedPolyline rings=%d largestPts=%d largestPerimCm=%.0f"),
		Rings.Num(), OutLargestLocalCm.Num(), PerimeterCm(OutLargestLocalCm));
}

void AIH_WB_IslandActor::BuildSeaShelfExtentFromShelfSegments()
{
	SeaRootsExtent = FIHSeaRootsExtent();
	bHasSeaRootsExtent = false;
	if (MainCoastPolylineLocalCm.Num() < 8)
	{
		return;
	}

	auto SampleHeightMetersAtLocalCm = [this](const FVector2D& LocalCm) -> float
	{
		if (HeightsMeters.Num() == 0 || SamplesPerSide < 2 || SampleSpacingMeters <= 0.0)
		{
			return 0.f;
		}
		const double Xm = LocalCm.X * 0.01;
		const double Ym = LocalCm.Y * 0.01;
		const double Fx = (Xm + HalfExtentMeters) / SampleSpacingMeters;
		const double Fy = (Ym + HalfExtentMeters) / SampleSpacingMeters;
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fx)), 0, SamplesPerSide - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Fy)), 0, SamplesPerSide - 1);
		const int32 X1 = FMath::Min(X0 + 1, SamplesPerSide - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, SamplesPerSide - 1);
		const float Tx = static_cast<float>(Fx - X0);
		const float Ty = static_cast<float>(Fy - Y0);
		const float H00 = HeightsMeters[Y0 * SamplesPerSide + X0];
		const float H10 = HeightsMeters[Y0 * SamplesPerSide + X1];
		const float H01 = HeightsMeters[Y1 * SamplesPerSide + X0];
		const float H11 = HeightsMeters[Y1 * SamplesPerSide + X1];
		const float Hx0 = FMath::Lerp(H00, H10, Tx);
		const float Hx1 = FMath::Lerp(H01, H11, Tx);
		return FMath::Lerp(Hx0, Hx1, Ty);
	};

	auto ClassifyTierFromSlope = [](const float SlopeDeg, const float AbsDeltaZ) -> EIHSeaRootsTier
	{
		using namespace IHInvisibleHandSpec;
		if (AbsDeltaZ < SeaRootsSlopeFlatDeltaMeters)
		{
			return EIHSeaRootsTier::Beach;
		}
		if (SlopeDeg <= SeaRootsTierBeachMaxSlopeDeg)
		{
			return EIHSeaRootsTier::Beach;
		}
		if (SlopeDeg < SeaRootsTierGentleMaxSlopeDeg)
		{
			return EIHSeaRootsTier::Gentle;
		}
		if (SlopeDeg < SeaRootsTierSteepMaxSlopeDeg)
		{
			return EIHSeaRootsTier::Steep;
		}
		return EIHSeaRootsTier::Sheer;
	};

	auto DisplacementForTier = [](const EIHSeaRootsTier Tier) -> float
	{
		using namespace IHInvisibleHandSpec;
		switch (Tier)
		{
		case EIHSeaRootsTier::Beach: return SeaRootsDispBeachMeters;
		case EIHSeaRootsTier::Gentle: return SeaRootsDispGentleMeters;
		case EIHSeaRootsTier::Steep: return SeaRootsDispSteepMeters;
		case EIHSeaRootsTier::Sheer: return SeaRootsDispSheerMeters;
		default: return SeaRootsDispGentleMeters;
		}
	};

	const float InlandSampleM = IHInvisibleHandSpec::SeaRootsSlopeSampleInlandMeters;
	const float TanFrac = IHInvisibleHandSpec::SeaRootsTanOuterFraction;
	const float CyanFrac = IHInvisibleHandSpec::SeaRootsCyanOuterFraction;
	const bool bUseSlopeLut = IHInvisibleHandSpec::IsCoastPhaseB2SlopeTierShelfActive();
	constexpr int32 GovernedCoastTargetPts = 640;
	const int32 FullCoastPts = MainCoastPolylineLocalCm.Num();

	// Full MainCoast → km, ensure CCW. Gold Contours keep full ContourGoldRings; only reverse-sync Features.
	TArray<FVector2D> CoastKmFull;
	CoastKmFull.Reserve(FullCoastPts);
	for (const FVector2D& P : MainCoastPolylineLocalCm)
	{
		CoastKmFull.Add(FVector2D(P.X / 100000.f, P.Y / 100000.f));
	}
	float SignedAreaKm2 = 0.f;
	for (int32 i = 0; i < CoastKmFull.Num(); ++i)
	{
		const FVector2D& A = CoastKmFull[i];
		const FVector2D& B = CoastKmFull[(i + 1) % CoastKmFull.Num()];
		SignedAreaKm2 += (A.X * B.Y - B.X * A.Y);
	}
	SignedAreaKm2 *= 0.5f;
	const bool bReversedToCcw = SignedAreaKm2 < 0.f;
	FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(CoastKmFull);
	if (bReversedToCcw)
	{
		for (int32 i = 0; i < CoastKmFull.Num(); ++i)
		{
			MainCoastPolylineLocalCm[i] = FVector2D(CoastKmFull[i].X * 100000.f, CoastKmFull[i].Y * 100000.f);
		}
		if (ContourGoldRingsLocalCm.Num() > 0)
		{
			ContourGoldRingsLocalCm[0] = MainCoastPolylineLocalCm;
		}
		if (CoastCharacterRing.Num() == MainCoastPolylineLocalCm.Num())
		{
			Algo::Reverse(CoastCharacterRing);
		}
	}

	// Decimate for governed WWF offset (avoids self-crossing on dense coasts).
	TArray<FVector2D> CoastKm;
	const int32 DecimateN = FMath::Clamp(GovernedCoastTargetPts, 64, CoastKmFull.Num());
	if (CoastKmFull.Num() > DecimateN)
	{
		FIHCoastPolylineSmoothing::ResampleClosedPolylineUniformCount(CoastKmFull, DecimateN, CoastKm);
	}
	else
	{
		CoastKm = CoastKmFull;
	}
	FIHCoastPolylineSmoothing::EnsureCounterClockwiseClosedPolylineKm(CoastKm);

	const int32 SampleCount = CoastKm.Num();
	SeaRootsExtent.CoastSamplesLocalCm.SetNum(SampleCount);
	SeaRootsExtent.SharedRingSampleCount = SampleCount;
	SeaRootsExtent.OutwardNormals.SetNum(SampleCount);
	SeaRootsExtent.Tiers.SetNum(SampleCount);
	SeaRootsExtent.OutwardDisplacementsMeters.SetNum(SampleCount);
	SeaRootsExtent.TanOuterSamplesLocalCm.SetNum(SampleCount);
	SeaRootsExtent.CyanOuterSamplesLocalCm.SetNum(SampleCount);
	SeaRootsExtent.DeepOuterSamplesLocalCm.SetNum(SampleCount);
	FMemory::Memzero(SeaRootsExtent.TierCounts, sizeof(SeaRootsExtent.TierCounts));
	// Baked MainCoast-offset rings — do NOT fill CoastRadiiCm (azimuth rebuild causes star voids).
	SeaRootsExtent.CoastRadiiCm.Reset();
	SeaRootsExtent.TanOuterRadiiCm.Reset();
	SeaRootsExtent.CyanOuterRadiiCm.Reset();
	SeaRootsExtent.DeepOuterRadiiCm.Reset();

	for (int32 i = 0; i < SampleCount; ++i)
	{
		SeaRootsExtent.CoastSamplesLocalCm[i] = FVector2D(CoastKm[i].X * 100000.f, CoastKm[i].Y * 100000.f);
	}

	TArray<float> VertexOffsetKm;
	VertexOffsetKm.SetNum(SampleCount);

	for (int32 i = 0; i < SampleCount; ++i)
	{
		const FVector2D Coast = SeaRootsExtent.CoastSamplesLocalCm[i];
		const FVector2D Prev = SeaRootsExtent.CoastSamplesLocalCm[(i + SampleCount - 1) % SampleCount];
		const FVector2D Next = SeaRootsExtent.CoastSamplesLocalCm[(i + 1) % SampleCount];
		FVector2D Tangent = (Next - Prev).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = Coast.GetSafeNormal();
		}
		// CCW poly: outward = right of tangent (land inside).
		FVector2D Outward(Tangent.Y, -Tangent.X);
		SeaRootsExtent.OutwardNormals[i] = Outward;

		EIHSeaRootsTier Tier = EIHSeaRootsTier::Gentle;
		float DispM = IHInvisibleHandSpec::SeaRootsDispGentleMeters;
		if (bUseSlopeLut && HeightsMeters.Num() > 0)
		{
			const FVector2D InlandLocalCm = Coast - Outward * (InlandSampleM * 100.f);
			const float ZCoast = SampleHeightMetersAtLocalCm(Coast);
			const float ZInland = SampleHeightMetersAtLocalCm(InlandLocalCm);
			const float DeltaZ = FMath::Max(ZInland - ZCoast, 0.f);
			const float SlopeDeg = FMath::RadiansToDegrees(FMath::Atan2(DeltaZ, InlandSampleM));
			Tier = ClassifyTierFromSlope(SlopeDeg, DeltaZ);
			DispM = DisplacementForTier(Tier);
		}

		SeaRootsExtent.Tiers[i] = Tier;
		SeaRootsExtent.TierCounts[static_cast<uint8>(Tier)]++;
		VertexOffsetKm[i] = DispM / 1000.f; // m → km
	}

	// Closed 1D box blur on Disp (softens 100↔450 m tier jumps before normal-push).
	{
		constexpr int32 BlurHalf = 2; // 5-neighbor
		constexpr int32 BlurPasses = 3;
		TArray<float> Blurred = VertexOffsetKm;
		for (int32 Pass = 0; Pass < BlurPasses; ++Pass)
		{
			TArray<float> NextBlur;
			NextBlur.SetNum(SampleCount);
			for (int32 i = 0; i < SampleCount; ++i)
			{
				float Sum = 0.f;
				for (int32 K = -BlurHalf; K <= BlurHalf; ++K)
				{
					Sum += Blurred[(i + K + SampleCount * 8) % SampleCount];
				}
				NextBlur[i] = Sum / static_cast<float>(BlurHalf * 2 + 1);
			}
			Blurred = MoveTemp(NextBlur);
		}
		VertexOffsetKm = MoveTemp(Blurred);
		for (int32 i = 0; i < SampleCount; ++i)
		{
			VertexOffsetKm[i] = FMath::Clamp(
				VertexOffsetKm[i],
				IHInvisibleHandSpec::SeaRootsDispSheerMeters / 1000.f,
				IHInvisibleHandSpec::SeaRootsDispBeachMeters / 1000.f);
			// Gate 0 LUT Disp (pre-cleanup; expect ~100–450 m).
			SeaRootsExtent.OutwardDisplacementsMeters[i] = VertexOffsetKm[i] * 1000.f;
		}
	}

	float LutDispSum = 0.f;
	float LutDispMax = 0.f;
	for (const float D : SeaRootsExtent.OutwardDisplacementsMeters)
	{
		LutDispSum += D;
		LutDispMax = FMath::Max(LutDispMax, D);
	}
	const float LutDispAvg = SampleCount > 0 ? LutDispSum / static_cast<float>(SampleCount) : 0.f;

	// Index-locked normal-push DeepOuter (no variable miter / no Chaikin) — keeps 1:1 Coast↔Deep.
	TArray<FVector2D> OutwardNormalsUnit;
	OutwardNormalsUnit.SetNum(SampleCount);
	for (int32 i = 0; i < SampleCount; ++i)
	{
		OutwardNormalsUnit[i] = SeaRootsExtent.OutwardNormals[i].GetSafeNormal();
		if (OutwardNormalsUnit[i].IsNearlyZero())
		{
			OutwardNormalsUnit[i] = FVector2D(1.f, 0.f);
		}
	}
	TArray<FVector2D> DeepKm;
	int32 SelfCrossAfter = 0;
	IH_WB_IslandActorPrivate::BuildNormalPushDeepOuterKm(
		CoastKm, OutwardNormalsUnit, VertexOffsetKm, DeepKm, SelfCrossAfter);
	if (DeepKm.Num() != SampleCount)
	{
		DeepKm.SetNum(SampleCount);
		const float GentleKm = IHInvisibleHandSpec::SeaRootsDispGentleMeters / 1000.f;
		for (int32 i = 0; i < SampleCount; ++i)
		{
			DeepKm[i] = CoastKm[i] + OutwardNormalsUnit[i] * GentleKm;
			VertexOffsetKm[i] = GentleKm;
		}
		SelfCrossAfter = IH_WB_IslandActorPrivate::CountClosedPolylineSelfCrossings(DeepKm);
	}

	// Presentation Disp follows post-cleanup chords (may go below Sheer in narrow firths).
	for (int32 i = 0; i < SampleCount; ++i)
	{
		SeaRootsExtent.OutwardDisplacementsMeters[i] = VertexOffsetKm[i] * 1000.f;
	}

	float ChordDispSum = 0.f;
	float ChordDispMax = 0.f;
	for (int32 i = 0; i < SampleCount; ++i)
	{
		const FVector2D Coast = SeaRootsExtent.CoastSamplesLocalCm[i];
		const FVector2D Outward = OutwardNormalsUnit[i];
		FVector2D DeepCm = Coast + Outward * (VertexOffsetKm[i] * 100000.f);
		if (DeepKm.IsValidIndex(i))
		{
			DeepCm = FVector2D(DeepKm[i].X * 100000.f, DeepKm[i].Y * 100000.f);
		}
		SeaRootsExtent.DeepOuterSamplesLocalCm[i] = DeepCm;
		const float DeepDispCm = FVector2D::Distance(Coast, DeepCm);
		const float DeepDispM = DeepDispCm * 0.01f;
		ChordDispSum += DeepDispM;
		ChordDispMax = FMath::Max(ChordDispMax, DeepDispM);
		SeaRootsExtent.TanOuterSamplesLocalCm[i] = Coast + Outward * (DeepDispCm * TanFrac);
		SeaRootsExtent.CyanOuterSamplesLocalCm[i] = Coast + Outward * (DeepDispCm * CyanFrac);
	}
	const float ChordDispAvg = SampleCount > 0 ? ChordDispSum / static_cast<float>(SampleCount) : 0.f;
	const float TaperDegAvg = LutDispAvg > KINDA_SMALL_NUMBER
		? FMath::RadiansToDegrees(FMath::Atan2(-IHInvisibleHandSpec::ShelfFloorMeters, LutDispAvg))
		: 0.f;
	UE_LOG(LogTemp, Log,
		TEXT("IH_WB_IslandActor: GovernedWWF island=%d samples=%d (fromFull=%d) dispAvgM=%.0f dispMaxM=%.0f chordDispAvgM=%.0f chordDispMaxM=%.0f selfCross=%d taperDeg~%.1f tiers B/G/St/Sh=%d/%d/%d/%d (HF-25 override off normalPush=1 miterOffset=0 decimate=1 noChaikin=1)"),
		TankIslandIndex, SampleCount, FullCoastPts, LutDispAvg, LutDispMax, ChordDispAvg, ChordDispMax, SelfCrossAfter, TaperDegAvg,
		SeaRootsExtent.TierCounts[static_cast<uint8>(EIHSeaRootsTier::Beach)],
		SeaRootsExtent.TierCounts[static_cast<uint8>(EIHSeaRootsTier::Gentle)],
		SeaRootsExtent.TierCounts[static_cast<uint8>(EIHSeaRootsTier::Steep)],
		SeaRootsExtent.TierCounts[static_cast<uint8>(EIHSeaRootsTier::Sheer)]);

	bHasSeaRootsExtent = HasValidSeaRootsExtentForPresentation(SeaRootsExtent);
}

void AIH_WB_IslandActor::RegisterCollision()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C07_IslandCollisionSubsystem* Collision = GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
		{
			Collision->UnregisterIslandCollision(this);
			Collision->RegisterIslandCollision(this, IslandMesh);
			if (IHInvisibleHandSpec::IsWwfShelfCollisionEnabled() && ShelfMesh)
			{
				Collision->RegisterIslandCollision(this, ShelfMesh);
			}
		}
	}
}

void AIH_WB_IslandActor::UnregisterCollision()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C07_IslandCollisionSubsystem* Collision = GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
		{
			Collision->UnregisterIslandCollision(this);
		}
	}
}

void AIH_WB_IslandActor::RefreshMinimapCoastline()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI || TankIslandIndex < 0)
	{
		return;
	}
	UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>();
	if (!Minimap)
	{
		return;
	}

	// Drop stale rings for this island, then publish all ContourGold rings (MainCoast = [0]).
	Minimap->UnregisterCoastlineForIsland(TankIslandIndex);

	auto RegisterRingLocalCm = [&](const TArray<FVector2D>& LocalRing, const int32 FeatureId, const bool bIsInlandSea)
	{
		if (LocalRing.Num() < 3)
		{
			return;
		}
		TArray<FVector2D> WorldCoast;
		WorldCoast.Reserve(LocalRing.Num());
		for (const FVector2D& Local : LocalRing)
		{
			WorldCoast.Add(LocalCmToWorldCm(Local));
		}
		Minimap->RegisterCoastlinePolylineWorld(TankIslandIndex, WorldCoast, FeatureId, bIsInlandSea);
	};

	if (ContourGoldRingsLocalCm.Num() > 0)
	{
		for (int32 RingIdx = 0; RingIdx < ContourGoldRingsLocalCm.Num(); ++RingIdx)
		{
			const bool bIsInlandSea =
				ContourGoldRingsIsInlandSea.IsValidIndex(RingIdx) && ContourGoldRingsIsInlandSea[RingIdx];
			RegisterRingLocalCm(ContourGoldRingsLocalCm[RingIdx], RingIdx, bIsInlandSea);
		}
	}
	else
	{
		TArray<FVector2D> WorldCoast;
		GetShorelinePolygonWorldCm(WorldCoast);
		Minimap->RegisterCoastlinePolylineWorld(TankIslandIndex, WorldCoast, 0);
	}

	if (bHasSeaRootsExtent)
	{
		const FVector Loc = GetActorLocation();
		Minimap->RegisterSeaRootsExtentWorld(
			TankIslandIndex,
			FVector2D(Loc.X, Loc.Y),
			GetActorRotation().Yaw,
			SeaRootsExtent);
	}
}

void AIH_WB_IslandActor::UpdateMinimapCoastlineTransformOnly()
{
	RefreshMinimapCoastline();
}
