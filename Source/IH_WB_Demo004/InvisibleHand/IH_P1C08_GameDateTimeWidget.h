// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHCalendarTypes.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_GameDateTimeWidget.generated.h"

class UBorder;
class UTextBlock;

/** Read-only status strip: Year | Season | Month | Day | Time. No player interaction beyond
 * show/hide — reports GameInstance's calendar snapshot, never lets the player edit it. Dialing
 * the calendar is Play Atmospherics' job now; this widget just reflects the result, refreshing
 * live while Play Atmospherics auto-advances. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_GameDateTimeWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	bool IsPanelVisible() const { return bPanelVisible; }
	void TogglePanelVisible();
	void SetPanelVisible(bool bVisible);
	void UpdatePanelLayout();
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const;
	/** Re-reads GameInstance's calendar fields and refreshes the displayed text. */
	void RefreshFromGameInstance();

private:
	void EnsureWidgetTree();
	static FString SeasonLabelFromMonth(int32 Month);
	static const TCHAR* MonthAbbrev(int32 Month);
	static FText TimeBracketLabel(EIHTimeBracket Bracket);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DateTimeText;

	bool bPanelVisible = true;
};
