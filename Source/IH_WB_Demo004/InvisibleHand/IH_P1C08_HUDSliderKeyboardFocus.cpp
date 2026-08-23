// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_HUDSliderKeyboardFocus.h"

void FIH_P1C08_HUDSliderKeyboardFocus::Reset()
{
	bActive = false;
	FocusedSliderIndex = 0;
	BufferedValues.Reset();
	CommittedValues.Reset();
	DebounceCountdown = 0.f;
	bPreviewPending = false;
}

bool FIH_P1C08_HUDSliderKeyboardFocus::IsDirty() const
{
	if (BufferedValues.Num() != CommittedValues.Num())
	{
		return true;
	}

	for (int32 Index = 0; Index < BufferedValues.Num(); ++Index)
	{
		if (!FMath::IsNearlyEqual(BufferedValues[Index], CommittedValues[Index], KINDA_SMALL_NUMBER))
		{
			return true;
		}
	}

	return false;
}

void FIH_P1C08_HUDSliderKeyboardFocus::BeginFocus(const TArray<float>& CurrentValues, int32 InitialIndex)
{
	bActive = true;
	BufferedValues = CurrentValues;
	CommittedValues = CurrentValues;
	FocusedSliderIndex = FMath::Clamp(InitialIndex, 0, FMath::Max(0, CurrentValues.Num() - 1));
	DebounceCountdown = 0.f;
	bPreviewPending = false;
}

void FIH_P1C08_HUDSliderKeyboardFocus::EndFocus()
{
	bActive = false;
	DebounceCountdown = 0.f;
	bPreviewPending = false;
}

void FIH_P1C08_HUDSliderKeyboardFocus::SetFocusedSliderIndex(int32 Index, int32 SliderCount)
{
	if (!bActive || SliderCount <= 0)
	{
		return;
	}

	FocusedSliderIndex = FMath::Clamp(Index, 0, SliderCount - 1);
}

void FIH_P1C08_HUDSliderKeyboardFocus::NudgeFocused(
	int32 SliderCount,
	float Delta,
	float MinValue,
	float MaxValue)
{
	if (!bActive || SliderCount <= 0 || !BufferedValues.IsValidIndex(FocusedSliderIndex))
	{
		return;
	}

	BufferedValues[FocusedSliderIndex] = FMath::Clamp(
		BufferedValues[FocusedSliderIndex] + Delta, MinValue, MaxValue);
	RequestPreview();
}

void FIH_P1C08_HUDSliderKeyboardFocus::CycleFocused(int32 SliderCount, int32 Delta)
{
	if (!bActive || SliderCount <= 1)
	{
		return;
	}

	FocusedSliderIndex = FMath::Clamp(FocusedSliderIndex + Delta, 0, SliderCount - 1);
}

void FIH_P1C08_HUDSliderKeyboardFocus::RequestPreview()
{
	bPreviewPending = true;
	DebounceCountdown = DebounceSeconds;
}

void FIH_P1C08_HUDSliderKeyboardFocus::TickDebouncedPreview(
	float DeltaTime,
	TFunctionRef<void(const TArray<float>&)> PreviewApply)
{
	if (!bActive || !bPreviewPending)
	{
		return;
	}

	DebounceCountdown -= DeltaTime;
	if (DebounceCountdown <= 0.f)
	{
		FlushPreview(PreviewApply);
	}
}

void FIH_P1C08_HUDSliderKeyboardFocus::FlushPreview(TFunctionRef<void(const TArray<float>&)> PreviewApply)
{
	if (!bActive || !bPreviewPending)
	{
		return;
	}

	bPreviewPending = false;
	DebounceCountdown = 0.f;
	PreviewApply(BufferedValues);
}

bool FIH_P1C08_HUDSliderKeyboardFocus::TryHandleNavigationKey(
	FKey Key,
	bool bIsRepeat,
	int32 SliderCount,
	float NudgeStep,
	float MinValue,
	float MaxValue)
{
	if (!bActive || SliderCount <= 0)
	{
		return false;
	}

	if (Key == EKeys::Left)
	{
		NudgeFocused(SliderCount, -NudgeStep, MinValue, MaxValue);
		return true;
	}

	if (Key == EKeys::Right)
	{
		NudgeFocused(SliderCount, +NudgeStep, MinValue, MaxValue);
		return true;
	}

	if (Key == EKeys::Up)
	{
		if (bIsRepeat)
		{
			return true;
		}
		CycleFocused(SliderCount, -1);
		return true;
	}

	if (Key == EKeys::Down)
	{
		if (bIsRepeat)
		{
			return true;
		}
		CycleFocused(SliderCount, +1);
		return true;
	}

	return false;
}

FIH_P1C08_HUDSliderKeyboardFocus::EActionKeyResult FIH_P1C08_HUDSliderKeyboardFocus::TryHandleActionKey(
	FKey Key,
	bool bIsRepeat)
{
	if (!bActive || bIsRepeat)
	{
		return EActionKeyResult::NotHandled;
	}

	if (Key == EKeys::Enter)
	{
		return EActionKeyResult::Commit;
	}

	if (Key == EKeys::Escape)
	{
		return EActionKeyResult::Cancel;
	}

	return EActionKeyResult::NotHandled;
}

void FIH_P1C08_HUDSliderKeyboardFocus::RevertToCommitted(TFunctionRef<void(const TArray<float>&)> ApplyValues)
{
	BufferedValues = CommittedValues;
	ApplyValues(CommittedValues);
}

void FIH_P1C08_HUDSliderKeyboardFocus::CommitToSubsystem(TFunctionRef<void(const TArray<float>&)> ApplyValues)
{
	FlushPreview(ApplyValues);
	CommittedValues = BufferedValues;
	ApplyValues(BufferedValues);
}
