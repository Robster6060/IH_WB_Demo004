// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_MinimapTypes.h"
#include "IHCellHeightmapTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C08_MinimapSubsystem.generated.h"

class AIH_Cube2FlyPlayerController;
class UIH_P1C08_MinimapWidget;
class UIH_P1C08_WorldCoastStrokeWidget;
class UIH_WB_Demo004GameInstance;

enum class EIHGate2bDevPOIMarkerKind : uint8
{
	LandingCove,
	Caldera,
	Summit
};

/** Gate 2b dev POI colors — shared by minimap and 3D/billboard markers. */
inline FLinearColor GetGate2bDevPOIMarkerDisplayColor(const EIHGate2bDevPOIMarkerKind Kind)
{
	switch (Kind)
	{
	case EIHGate2bDevPOIMarkerKind::LandingCove:
		return FLinearColor(1.f, 0.f, 1.f, 1.f); // pink
	case EIHGate2bDevPOIMarkerKind::Caldera:
		return FLinearColor(1.f, 0.5f, 0.f, 1.f); // orange
	case EIHGate2bDevPOIMarkerKind::Summit:
	default:
		return FLinearColor(1.f, 1.f, 0.f, 1.f); // yellow
	}
}

struct FIHGate2bDevPOIMarker
{
	int32 IslandIndex = INDEX_NONE;
	EIHGate2bDevPOIMarkerKind Kind = EIHGate2bDevPOIMarkerKind::LandingCove;
	FVector2D WorldXY = FVector2D::ZeroVector;
};

UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_MinimapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsMinimapOpen() const { return bMinimapOpen; }
	bool IsMouseOverMinimap() const { return bMouseOverMinimap; }
	bool IsScreenPointOverMinimap(const FVector2D& ScreenPos) const;
	/** PgUp/PgDn stay on fly camera; minimap uses mouse wheel only. */
	bool ShouldConsumeFlyPageKeys() const { return false; }

	float GetZoomFactor() const { return ZoomFactor; }
	int32 GetZoomStepIndex() const { return ZoomStepIndex; }
	bool UsesFloatingPosition() const { return bUseFloatingPosition; }
	FVector2D GetFloatingScreenPosition() const { return FloatingScreenPosition; }

	const TArray<TArray<FVector2D>>& GetCoastlinePolylinesWorld() const { return CoastlinePolylinesWorld; }
	/** Parallel to GetCoastlinePolylinesWorld() - true if that ring's interior is water (an
	 * enclosed inland sea/lake), false for the main coastline or a land islet. */
	const TArray<bool>& GetCoastlineIsInlandSeaWorld() const { return CoastlineIsInlandSea; }
	int32 GetCoastlineRevision() const { return CoastlineRevision; }
	/** Main coast only (FeatureId 0 / INDEX_NONE). */
	bool TryGetCoastlineWorldXYForIsland(int32 IslandIndex, TArray<FVector2D>& OutWorldXY) const;

	struct FCoastlineLayerEntry
	{
		int32 IslandIndex = INDEX_NONE;
		/** 0 = MainCoast; 1.. = ContourGold secondary rings (barrier islets). */
		int32 FeatureId = 0;
		TArray<FVector2D> PolylineWorld;
	};

	struct FSeaRootsLayerEntry
	{
		int32 IslandIndex = INDEX_NONE;
		/** INDEX_NONE = main feature; otherwise C5 islet FeatureId. */
		int32 FeatureId = INDEX_NONE;
		bool bIsMainFeature = true;
		FVector2D CenterWorldCm = FVector2D::ZeroVector;
		float YawDegrees = 0.f;
		FIHSeaRootsExtent Extent;
	};
	const TArray<FSeaRootsLayerEntry>& GetSeaRootsLayers() const { return SeaRootsLayers; }
	bool TryGetSeaRootsLayerForIsland(int32 IslandIndex, FSeaRootsLayerEntry& OutLayer) const;

	UIH_P1C08_MinimapWidget* GetMinimapWidget() const { return MinimapWidget; }
	UIH_P1C08_WorldCoastStrokeWidget* GetWorldCoastStrokeWidget() const { return WorldCoastStrokeWidget; }

	/** Create minimap widget and add to viewport (collapsed) so M toggle only flips visibility. */
	void PrepareMinimapWidget(AIH_Cube2FlyPlayerController* PC);

	void ToggleMinimap(AIH_Cube2FlyPlayerController* PC);
	void OpenMinimap(AIH_Cube2FlyPlayerController* PC);
	void CloseMinimap();

	void RegisterCoastlinePolylineWorld(
		int32 IslandIndex,
		const TArray<FVector2D>& PolylineWorld,
		int32 FeatureId = 0,
		bool bIsInlandSea = false);
	void RegisterSeaRootsExtentWorld(
		int32 IslandIndex,
		const FVector2D& CenterWorldCm,
		float YawDegrees,
		const FIHSeaRootsExtent& Extent,
		int32 FeatureId = INDEX_NONE,
		bool bIsMainFeature = true);
	void UnregisterCoastlineForOwner(const AActor* Owner);
	void UnregisterCoastlineForIsland(int32 IslandIndex);
	void UnregisterSeaRootsExtentForIsland(int32 IslandIndex);
	void SetGate2bDevPOIMarkersForIsland(int32 IslandIndex, const TArray<FIHGate2bDevPOIMarker>& Markers);
	void UnregisterGate2bDevPOIMarkersForIsland(int32 IslandIndex);
	const TArray<FIHGate2bDevPOIMarker>& GetGate2bDevPOIMarkers() const { return Gate2bDevPOIMarkers; }
	void ClearCoastlines();
	void RequestMinimapRepaint();
	/** Minimap widget only — does not invalidate PIE coast overlay. */
	void RequestMinimapWidgetRepaint();
	void SetMouseOverMinimap(bool bOver);
	void SetFloatingScreenPosition(const FVector2D& ViewportLocalPos);
	void UpdateFloatingPosition(const FVector2D& ViewportLocalPos);

	void HandleMouseWheelZoom(float WheelDelta, const FVector2D& ScreenPos);
	void StepZoom(int32 Direction);
	void FocusCameraOnWorldXY(AIH_Cube2FlyPlayerController* PC, const FVector2D& WorldXY);

	IH_P1C08_Minimap::FView BuildCurrentView(const AIH_Cube2FlyPlayerController* PC) const;

	float GetRealmHalfExtentEWCm() const;
	float GetRealmHalfExtentNSCm() const;

private:
	void EnsureWidget(AIH_Cube2FlyPlayerController* PC);
	void EnsureWorldCoastStrokeWidget(AIH_Cube2FlyPlayerController* PC);
	void ApplyZoomWithMapPivot(float NewZoomFactor, const FVector2D& MapPivotLocal, const FVector2D& MapSizePx);

	UPROPERTY()
	TObjectPtr<UIH_P1C08_MinimapWidget> MinimapWidget;

	UPROPERTY()
	TObjectPtr<UIH_P1C08_WorldCoastStrokeWidget> WorldCoastStrokeWidget;

	TArray<TArray<FVector2D>> CoastlinePolylinesWorld;
	TArray<int32> CoastlineIslandIndices;
	TArray<int32> CoastlineFeatureIds;
	TArray<bool> CoastlineIsInlandSea;
	int32 CoastlineRevision = 0;
	TArray<FSeaRootsLayerEntry> SeaRootsLayers;
	TArray<FIHGate2bDevPOIMarker> Gate2bDevPOIMarkers;

	bool bMinimapOpen = false;
	bool bMouseOverMinimap = false;
	bool bUseFloatingPosition = false;
	float ZoomFactor = 0.f;
	int32 ZoomStepIndex = 0;
	FVector2D ViewCenterWorld = FVector2D::ZeroVector;
	FVector2D FloatingScreenPosition = FVector2D::ZeroVector;
};
