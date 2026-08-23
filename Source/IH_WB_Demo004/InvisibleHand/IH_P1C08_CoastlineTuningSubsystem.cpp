// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_CoastlineTuningSubsystem.h"

#include "IH_P1C08_IslandNavSubsystem.h"
#include "IH_WB_Demo004GameInstance.h"

void UIH_P1C08_CoastlineTuningSubsystem::LoadActiveIslandFromSelection()
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			SyncActiveFromIsland(Nav->GetSelectedIslandIndex());
		}
	}
}

void UIH_P1C08_CoastlineTuningSubsystem::SyncActiveFromIsland(int32 IslandIndex)
{
	ActiveIslandIndex = IslandIndex;
	ActiveTuning = FIHIslandCoastlineTuning::SeedBaseline();
	ActiveManualTransform = FIHIslandManualTransform();

	if (IslandIndex != INDEX_NONE)
	{
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
			{
				ActiveTuning = Nav->GetCommittedCoastlineTuning(IslandIndex);
				ActiveManualTransform = Nav->GetCommittedManualTransform(IslandIndex);
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningSubsystem::ApplyActiveDraft()
{
	if (ActiveIslandIndex == INDEX_NONE)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			Nav->SetCommittedCoastlineTuning(ActiveIslandIndex, ActiveTuning);
			Nav->SetCommittedManualTransform(ActiveIslandIndex, ActiveManualTransform);
		}
	}

	// Apply committed transform before mesh rebuild so minimap coastline registers once at final pose.
	NotifyManualTransformChanged(ActiveIslandIndex);
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::RevertActiveDraft()
{
	if (ActiveIslandIndex == INDEX_NONE)
	{
		return;
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			ActiveTuning = Nav->GetCommittedCoastlineTuning(ActiveIslandIndex);
			ActiveManualTransform = Nav->GetCommittedManualTransform(ActiveIslandIndex);
		}
	}

	NotifyManualTransformChanged(ActiveIslandIndex);
	NotifyTuningChanged(ActiveIslandIndex);
}

bool UIH_P1C08_CoastlineTuningSubsystem::HasUncommittedTuningDraft() const
{
	if (ActiveIslandIndex == INDEX_NONE)
	{
		return false;
	}
	return ActiveTuning != GetCommittedTuningForActive();
}

bool UIH_P1C08_CoastlineTuningSubsystem::HasUncommittedTransformDraft() const
{
	if (ActiveIslandIndex == INDEX_NONE)
	{
		return false;
	}
	return ActiveManualTransform != GetCommittedManualTransformForActive();
}

bool UIH_P1C08_CoastlineTuningSubsystem::HasUncommittedDraft() const
{
	return HasUncommittedTuningDraft() || HasUncommittedTransformDraft();
}

FIHIslandCoastlineTuning UIH_P1C08_CoastlineTuningSubsystem::GetCommittedTuningForActive() const
{
	if (ActiveIslandIndex == INDEX_NONE)
	{
		return FIHIslandCoastlineTuning::SeedBaseline();
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			return Nav->GetCommittedCoastlineTuning(ActiveIslandIndex);
		}
	}
	return FIHIslandCoastlineTuning::SeedBaseline();
}

FIHIslandManualTransform UIH_P1C08_CoastlineTuningSubsystem::GetCommittedManualTransformForActive() const
{
	if (ActiveIslandIndex == INDEX_NONE)
	{
		return FIHIslandManualTransform();
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			return Nav->GetCommittedManualTransform(ActiveIslandIndex);
		}
	}
	return FIHIslandManualTransform();
}

TArray<FIHIslandCoastlineTuning> UIH_P1C08_CoastlineTuningSubsystem::GetAllIslandTuning(int32 IslandCount) const
{
	TArray<FIHIslandCoastlineTuning> Result;
	Result.SetNum(IslandCount);
	for (int32 i = 0; i < IslandCount; ++i)
	{
		Result[i] = FIHIslandCoastlineTuning::SeedBaseline();
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
			{
				Result[i] = Nav->GetCommittedCoastlineTuning(i);
			}
		}
	}
	return Result;
}

void UIH_P1C08_CoastlineTuningSubsystem::MarkDraftEdited()
{
	if (ActiveIslandIndex == INDEX_NONE)
	{
		return;
	}
	ActiveTuning.bUserEdited = true;
}

void UIH_P1C08_CoastlineTuningSubsystem::SetFbmAmplitudeScale(float V)
{
	ActiveTuning.FbmAmplitudeScale = FMath::Clamp(V, 0.f, 3.f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetFbmFrequencyScale(float V)
{
	ActiveTuning.FbmFrequencyScale = FMath::Clamp(V, 0.25f, 3.f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetDomainWarpStrengthScale(float V)
{
	ActiveTuning.DomainWarpStrengthScale = FMath::Clamp(V, 0.f, 3.f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetLobeStrengthScale(float V)
{
	ActiveTuning.LobeStrengthScale = FMath::Clamp(V, 0.f, 3.f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetRippleStrengthScale(float V)
{
	ActiveTuning.RippleStrengthScale = FMath::Clamp(V, 0.f, 3.f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetSummitAltitudeScale(float V)
{
	ActiveTuning.SummitAltitudeScale = FMath::Clamp(V, 0.25f, 2.5f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

float UIH_P1C08_CoastlineTuningSubsystem::GetSummitAltitudeScaleForIsland(int32 IslandIndex) const
{
	if (IslandIndex == ActiveIslandIndex)
	{
		return ActiveTuning.SummitAltitudeScale;
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			return Nav->GetCommittedCoastlineTuning(IslandIndex).SummitAltitudeScale;
		}
	}
	return 1.f;
}

void UIH_P1C08_CoastlineTuningSubsystem::SetPlacementScatter(float V)
{
	ActiveTuning.PlacementScatter = FMath::Clamp(V, 0.f, 1.f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetIslandSizeMultiplier(float V)
{
	ActiveTuning.IslandSizeMultiplier = FMath::Clamp(V, 1.f, 2.f);
	MarkDraftEdited();
	NotifyTuningChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetDraftManualTransform(const FIHIslandManualTransform& Transform)
{
	ActiveManualTransform = Transform;
	NotifyManualTransformChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::SetDraftManualTransformPreview(const FIHIslandManualTransform& Transform)
{
	ActiveManualTransform = Transform;
}

void UIH_P1C08_CoastlineTuningSubsystem::NudgeDraftManualOffsetCm(const FVector2D& DeltaCm)
{
	ActiveManualTransform.OffsetXYCm += DeltaCm;
	ActiveManualTransform.bUserMoved = true;
	NotifyManualTransformChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::AddDraftManualYawDeg(float DeltaDeg)
{
	ActiveManualTransform.YawDeg = FMath::UnwindDegrees(ActiveManualTransform.YawDeg + DeltaDeg);
	ActiveManualTransform.bUserMoved = true;
	NotifyManualTransformChanged(ActiveIslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::NotifyTuningChanged(int32 IslandIndex)
{
	OnCoastlineTuningChanged.Broadcast(IslandIndex);
}

void UIH_P1C08_CoastlineTuningSubsystem::NotifyManualTransformChanged(int32 IslandIndex)
{
	OnManualTransformChanged.Broadcast(IslandIndex);
}
