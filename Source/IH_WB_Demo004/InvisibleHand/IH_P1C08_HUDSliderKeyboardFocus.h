// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "InputCoreTypes.h"

/** Shared keyboard-edit state for HUD slider panels (coastline tuning, water choppiness). */
struct IH_WB_DEMO004_API FIH_P1C08_HUDSliderKeyboardFocus
{
	static constexpr float DefaultNudgeStep = 0.01f;
	static constexpr float DefaultDebounceSeconds = 0.2f;

	enum class EActionKeyResult : uint8
	{
		NotHandled,
		Commit,
		Cancel,
	};

	bool bActive = false;
	int32 FocusedSliderIndex = 0;
	TArray<float> BufferedValues;
	TArray<float> CommittedValues;

	float DebounceSeconds = DefaultDebounceSeconds;
	float DebounceCountdown = 0.f;
	bool bPreviewPending = false;

	void Reset();
	bool IsActive() const { return bActive; }
	bool IsDirty() const;

	void BeginFocus(const TArray<float>& CurrentValues, int32 InitialIndex = 0);
	void EndFocus();
	int32 GetFocusedSliderIndex() const { return FocusedSliderIndex; }
	void SetFocusedSliderIndex(int32 Index, int32 SliderCount);

	/** Left/Right nudge (clamp at min/max). Up/Down cycle slider index (clamp, no wrap). */
	bool TryHandleNavigationKey(
		FKey Key,
		bool bIsRepeat,
		int32 SliderCount,
		float NudgeStep = DefaultNudgeStep,
		float MinValue = 0.f,
		float MaxValue = 1.f);

	EActionKeyResult TryHandleActionKey(FKey Key, bool bIsRepeat);

	void TickDebouncedPreview(float DeltaTime, TFunctionRef<void(const TArray<float>&)> PreviewApply);
	void FlushPreview(TFunctionRef<void(const TArray<float>&)> PreviewApply);

	void RevertToCommitted(TFunctionRef<void(const TArray<float>&)> ApplyValues);
	void CommitToSubsystem(TFunctionRef<void(const TArray<float>&)> ApplyValues);

private:
	void NudgeFocused(int32 SliderCount, float Delta, float MinValue = 0.f, float MaxValue = 1.f);
	void CycleFocused(int32 SliderCount, int32 Delta);
	void RequestPreview();
};
