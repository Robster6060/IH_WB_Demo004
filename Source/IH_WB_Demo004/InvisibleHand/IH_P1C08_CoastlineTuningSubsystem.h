// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_IslandCoastlineTuning.h"
#include "IH_P1C08_IslandManualTransform.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C08_CoastlineTuningSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCoastlineTuningChanged, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnIslandManualTransformChanged, int32);

UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_CoastlineTuningSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FOnCoastlineTuningChanged OnCoastlineTuningChanged;
	FOnIslandManualTransformChanged OnManualTransformChanged;

	void LoadActiveIslandFromSelection();
	void SyncActiveFromIsland(int32 IslandIndex);

	/** Commit draft tuning + manual transform to per-island storage. */
	void ApplyActiveDraft();

	/** Restore draft from last committed state and refresh preview. */
	void RevertActiveDraft();

	bool HasUncommittedDraft() const;
	bool HasUncommittedTuningDraft() const;
	bool HasUncommittedTransformDraft() const;

	void SetFbmAmplitudeScale(float V);
	void SetFbmFrequencyScale(float V);
	void SetDomainWarpStrengthScale(float V);
	void SetLobeStrengthScale(float V);
	void SetRippleStrengthScale(float V);
	void SetSummitAltitudeScale(float V);
	void SetPlacementScatter(float V);
	void SetIslandSizeMultiplier(float V);

	void SetDraftManualTransform(const FIHIslandManualTransform& Transform);
	/** Update draft transform without broadcasting (used during live island drag). */
	void SetDraftManualTransformPreview(const FIHIslandManualTransform& Transform);
	void NudgeDraftManualOffsetCm(const FVector2D& DeltaCm);
	void AddDraftManualYawDeg(float DeltaDeg);

	float GetFbmAmplitudeScale() const { return ActiveTuning.FbmAmplitudeScale; }
	float GetFbmFrequencyScale() const { return ActiveTuning.FbmFrequencyScale; }
	float GetDomainWarpStrengthScale() const { return ActiveTuning.DomainWarpStrengthScale; }
	float GetLobeStrengthScale() const { return ActiveTuning.LobeStrengthScale; }
	float GetRippleStrengthScale() const { return ActiveTuning.RippleStrengthScale; }
	float GetSummitAltitudeScale() const { return ActiveTuning.SummitAltitudeScale; }
	float GetSummitAltitudeScaleForIsland(int32 IslandIndex) const;
	float GetPlacementScatter() const { return ActiveTuning.PlacementScatter; }
	float GetIslandSizeMultiplier() const { return ActiveTuning.IslandSizeMultiplier; }

	FIHIslandCoastlineTuning GetActiveTuning() const { return ActiveTuning; }
	FIHIslandCoastlineTuning GetCommittedTuningForActive() const;
	FIHIslandManualTransform GetActiveManualTransform() const { return ActiveManualTransform; }
	FIHIslandManualTransform GetCommittedManualTransformForActive() const;

	int32 GetActiveIslandIndex() const { return ActiveIslandIndex; }
	bool HasActiveIsland() const { return ActiveIslandIndex != INDEX_NONE; }

	TArray<FIHIslandCoastlineTuning> GetAllIslandTuning(int32 IslandCount) const;

private:
	void NotifyTuningChanged(int32 IslandIndex);
	void NotifyManualTransformChanged(int32 IslandIndex);
	void MarkDraftEdited();

	UPROPERTY(Transient)
	FIHIslandCoastlineTuning ActiveTuning;

	UPROPERTY(Transient)
	FIHIslandManualTransform ActiveManualTransform;

	UPROPERTY(Transient)
	int32 ActiveIslandIndex = INDEX_NONE;
};
