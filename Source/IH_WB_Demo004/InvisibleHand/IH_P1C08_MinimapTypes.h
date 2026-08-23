// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"
#include "IHInvisibleHandDesignSpec.h"



/** Shared minimap constants and view math for the φ landscape tank prototype. */

namespace IH_P1C08_Minimap

{

	/** Default 26×~42 km φ realm half-extents (cm). Runtime values come from game instance. */

	static constexpr float DefaultRealmHalfExtentNSCm =
		IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm * 100000.f;

	static constexpr float DefaultRealmHalfExtentEWCm = static_cast<float>(
		IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm * IHInvisibleHandSpec::GoldenRatioPhi * 100000.0);

	static constexpr float MinZoomWindowHalfExtentCm = 20000.f;

	/** Blue φ-map content width (px). Panel chrome is added in FPanelLayout.
	 * Plan Addendum 19: halved 552->276 (pure NativePaint widget, no UMG RenderTransform
	 * applies here - unlike every other dev-HUD widget, this one's chrome constants must be
	 * hand-adjusted directly). Follow-up: user asked for the minimap back at 2x its halved
	 * size, which lands back at the original 552 - kept as its own named constant/comment
	 * rather than reverted silently, so the history stays legible. */
	static constexpr float MapContentWidthPx = 552.f;

	static constexpr float DefaultMarginPx = 20.f;

	/** Square red close icon; also drives uniform slate frame margins. */
	static constexpr float CloseButtonSizePx = 14.f;

	static constexpr float TitleBarHeightPx = CloseButtonSizePx;

	static constexpr float BorderWidthPx = 2.f;

	/** Grey inset around blue map on all sides; matches close button width/height. */
	static constexpr float UniformFrameMarginPx = CloseButtonSizePx;

	static constexpr float CloseButtonHitPaddingPx = 4.f;

	static constexpr int32 ZoomStepCount = 8;

	static constexpr float WaterlineSliceZCm = 0.f;

	/** M1/M2/B2: minimap coast stroke and decimation (screen px). MinPx/MaxPx are now clamp
	 * bounds around a world-scaled value (see CoastOutlineTargetWorldThicknessCm /
	 * ResolveWorldScaledStrokePx below), not the primary driver of thickness. */
	static constexpr float CoastOutlineThicknessMinPx = 1.f;
	static constexpr float CoastOutlineThicknessMaxPx = 6.f;
	/** Target real-world coastline stroke width (~3m) - converted to screen px each frame via
	 * the current zoom's world-to-map scale, then clamped to [Min/MaxPx] above. Anchoring to a
	 * world size (not a fixed px value) means the stroke naturally thins when zoomed out (so it
	 * stops overwhelming tiny islets/inland-sea holes) and thickens when zoomed in (so it never
	 * reads as "too thin") - the clamp only guards the two extremes. Starting value - tune via
	 * PIE grab like every other visual constant in this file. */
	static constexpr float CoastOutlineTargetWorldThicknessCm = 300.f;
	static constexpr float CoastMinimapMinVertexSpacingPx = 0.65f;
	static constexpr float CoastMinimapMinVertexSpacingZoomedPx = 0.35f;
	/** Minimap coast input cap (display only — not MainCoast @768 authority). */
	static constexpr int32 CoastMinimapMaxVerticesPerFeature = IHInvisibleHandSpec::SeaRootsAzimuthSampleCount;
	/** Hard cap after densify — prevents full-zoom minimap paint from exploding to 100k+ verts. */
	static constexpr int32 CoastMinimapMaxStrokeOutputVerts = 384;
	/** Max gap between drawn coast segments (px) — prevents dotted strokes after decimation. */
	static constexpr float CoastMinimapMaxDrawSegmentPx = 2.5f;
	/** PIE world coast overlay: cap screen verts per island (display only). */
	static constexpr int32 CoastWorldOverlayMaxScreenVertsPerIsland = CoastMinimapMaxStrokeOutputVerts;
	/** Rebuild overlay cache when camera moves more than this (cm). */
	static constexpr float CoastWorldOverlayCameraInvalidateDistCm = 250.f;
	/** Coastline rings (raw WORLD cm^2, not screen px - a screen-space threshold is zoom-
	 * dependent and would incorrectly filter real islands when zoomed out) below this enclosed
	 * area are skipped entirely (not filled, not stroked) - filters genuinely degenerate
	 * slivers (near-zero real area) without touching real islets/landmasses regardless of
	 * current minimap zoom. 100 m^2 is far below any real islet's realistic size. */
	static constexpr double CoastMinimapMinRingFillAreaWorldCm2 = 100.0 * 10000.0;

	inline float ResolveCoastMinVertexSpacingPx(float ZoomFactor)
	{
		return FMath::Lerp(
			CoastMinimapMinVertexSpacingPx,
			CoastMinimapMinVertexSpacingZoomedPx,
			FMath::Clamp(ZoomFactor, 0.f, 1.f));
	}



	inline float ZoomFactorFromStep(int32 StepIndex)

	{

		if (ZoomStepCount <= 1)

		{

			return 0.f;

		}

		return FMath::Clamp(static_cast<float>(StepIndex) / static_cast<float>(ZoomStepCount - 1), 0.f, 1.f);

	}



	inline int32 ZoomStepFromFactor(float ZoomFactor)

	{

		if (ZoomStepCount <= 1)

		{

			return 0;

		}

		return FMath::Clamp(

			FMath::RoundToInt(ZoomFactor * static_cast<float>(ZoomStepCount - 1)), 0, ZoomStepCount - 1);

	}



	inline FVector2D HalfExtentFromZoomFactor(float ZoomFactor, float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm,

		float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)

	{

		const FVector2D FullHalf(RealmHalfExtentEWCm, RealmHalfExtentNSCm);

		const float MaxDim = FMath::Max(RealmHalfExtentEWCm, RealmHalfExtentNSCm);

		const FVector2D MinHalf(

			MinZoomWindowHalfExtentCm * (RealmHalfExtentEWCm / MaxDim),

			MinZoomWindowHalfExtentCm * (RealmHalfExtentNSCm / MaxDim));

		return FMath::Lerp(

			FullHalf, MinHalf, FMath::Clamp(ZoomFactor, 0.f, 1.f));

	}



	/** Map content size (px) preserving tank E-W / N-S aspect at MapContentWidthPx. */
	inline FVector2D GetMapContentSizePx(
		float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm,
		float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)
	{
		const float Aspect = RealmHalfExtentEWCm / FMath::Max(RealmHalfExtentNSCm, 1.f);
		const float MapHeight = MapContentWidthPx / Aspect;
		return FVector2D(MapContentWidthPx, MapHeight);
	}

	/** Generic "world-width, pixel-clamped" stroke-thickness primitive - reusable by any future
	 * 2D overlay map, not just coastline. Converts a real WORLD-space target thickness (cm) to
	 * screen px using the current zoom's world-to-map scale (px-per-world-cm is a single uniform
	 * scalar here - HalfExtentFromZoomFactor/GetMapContentSizePx share the same EW/NS aspect, so
	 * X and Y scale identically), then clamps to [MinPx, MaxPx] so the result can neither vanish
	 * at extreme zoom-out nor balloon at extreme zoom-in. */
	inline float ResolveWorldScaledStrokePx(
		float WorldThicknessCm, float ZoomFactor, float MinPx, float MaxPx,
		float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm,
		float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)
	{
		const FVector2D HalfExtent = HalfExtentFromZoomFactor(ZoomFactor, RealmHalfExtentEWCm, RealmHalfExtentNSCm);
		const FVector2D MapSize = GetMapContentSizePx(RealmHalfExtentEWCm, RealmHalfExtentNSCm);
		const float PxPerWorldCm = (MapSize.X * 0.5f) / FMath::Max(HalfExtent.X, KINDA_SMALL_NUMBER);
		return FMath::Clamp(WorldThicknessCm * PxPerWorldCm, MinPx, MaxPx);
	}

	inline float ResolveCoastOutlineThicknessPx(
		float ZoomFactor,
		float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm,
		float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)
	{
		return ResolveWorldScaledStrokePx(
			CoastOutlineTargetWorldThicknessCm, ZoomFactor,
			CoastOutlineThicknessMinPx, CoastOutlineThicknessMaxPx,
			RealmHalfExtentEWCm, RealmHalfExtentNSCm);
	}

	/** Tight outer panel + map placement; single source of truth for paint and hit tests. */
	struct FPanelLayout
	{
		FVector2D PanelSize = FVector2D::ZeroVector;
		FVector2D MapSize = FVector2D::ZeroVector;
		FVector2D MapOrigin = FVector2D::ZeroVector;

		static FPanelLayout Compute(
			float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm,
			float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)
		{
			FPanelLayout Layout;
			Layout.MapSize = GetMapContentSizePx(RealmHalfExtentEWCm, RealmHalfExtentNSCm);
			const float FrameMargin = UniformFrameMarginPx;
			Layout.MapOrigin = FVector2D(FrameMargin, FrameMargin);
			Layout.PanelSize = FVector2D(
				Layout.MapSize.X + (FrameMargin * 2.f),
				Layout.MapSize.Y + (FrameMargin * 2.f));
			return Layout;
		}
	};

	/** Outer slate panel size (px): map content + title bar + border padding. */
	inline FVector2D GetPanelSizePx(
		float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm,
		float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)
	{
		return FPanelLayout::Compute(RealmHalfExtentEWCm, RealmHalfExtentNSCm).PanelSize;
	}



	inline FVector2D ClampViewCenterToRealm(

		const FVector2D& CenterWorld, const FVector2D& HalfExtentWorld,

		float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm, float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)

	{

		if (HalfExtentWorld.X >= RealmHalfExtentEWCm - KINDA_SMALL_NUMBER

			&& HalfExtentWorld.Y >= RealmHalfExtentNSCm - KINDA_SMALL_NUMBER)

		{

			return FVector2D::ZeroVector;

		}



		const float LimitX = RealmHalfExtentEWCm - HalfExtentWorld.X;

		const float LimitY = RealmHalfExtentNSCm - HalfExtentWorld.Y;

		return FVector2D(

			FMath::Clamp(CenterWorld.X, -LimitX, LimitX),

			FMath::Clamp(CenterWorld.Y, -LimitY, LimitY));

	}



	struct FView

	{

		FVector2D CenterWorld = FVector2D::ZeroVector;

		FVector2D HalfExtentWorld = FVector2D(DefaultRealmHalfExtentEWCm, DefaultRealmHalfExtentNSCm);

		float RealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm;

		float RealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm;



		static FView Build(const FVector2D& ViewCenterWorld, float ZoomFactor,

			float InRealmHalfExtentEWCm = DefaultRealmHalfExtentEWCm, float InRealmHalfExtentNSCm = DefaultRealmHalfExtentNSCm)

		{

			FView View;

			View.RealmHalfExtentEWCm = InRealmHalfExtentEWCm;

			View.RealmHalfExtentNSCm = InRealmHalfExtentNSCm;

			View.HalfExtentWorld = HalfExtentFromZoomFactor(ZoomFactor, InRealmHalfExtentEWCm, InRealmHalfExtentNSCm);

			View.CenterWorld = (ZoomFactor <= KINDA_SMALL_NUMBER)

				? FVector2D::ZeroVector

				: ClampViewCenterToRealm(ViewCenterWorld, View.HalfExtentWorld, InRealmHalfExtentEWCm, InRealmHalfExtentNSCm);

			return View;

		}



		FVector2D WorldToMapLocal(FVector2D WorldXY, const FVector2D& MapSize) const

		{

			if (HalfExtentWorld.X <= KINDA_SMALL_NUMBER || HalfExtentWorld.Y <= KINDA_SMALL_NUMBER)

			{

				return MapSize * 0.5f;

			}



			const FVector2D Rel = WorldXY - CenterWorld;

			const FVector2D Scale(

				(MapSize.X * 0.5f) / HalfExtentWorld.X,

				(MapSize.Y * 0.5f) / HalfExtentWorld.Y);

			// Match default fly camera (south of tank, looking north): +Y north = top of panel.
			return FVector2D(
				MapSize.X * 0.5f - Rel.X * Scale.X,
				MapSize.Y * 0.5f - Rel.Y * Scale.Y);

		}



		bool MapLocalToWorld(FVector2D MapLocal, const FVector2D& MapSize, FVector2D& OutWorldXY) const

		{

			if (HalfExtentWorld.X <= KINDA_SMALL_NUMBER || HalfExtentWorld.Y <= KINDA_SMALL_NUMBER

				|| MapSize.X <= KINDA_SMALL_NUMBER || MapSize.Y <= KINDA_SMALL_NUMBER)

			{

				return false;

			}



			const FVector2D Scale(

				(MapSize.X * 0.5f) / HalfExtentWorld.X,

				(MapSize.Y * 0.5f) / HalfExtentWorld.Y);

			const FVector2D Rel(
				-(MapLocal.X - MapSize.X * 0.5f) / Scale.X,
				-(MapLocal.Y - MapSize.Y * 0.5f) / Scale.Y);

			OutWorldXY = CenterWorld + Rel;

			return true;

		}



		/** Convert a world XY travel direction into map-panel glyph degrees. */

		static float WorldDirectionToMapGlyphDegrees(FVector2D WorldDirXY)

		{

			if (WorldDirXY.IsNearlyZero())

			{

				return 0.f;

			}

			const FVector2D MapDir(-WorldDirXY.X, -WorldDirXY.Y);

			return FMath::UnwindDegrees(FMath::RadiansToDegrees(FMath::Atan2(MapDir.Y, MapDir.X)));

		}

	};

}


