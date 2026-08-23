// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "IH_P1C08_IslandNavWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UTextBlock;
class UIH_P1C08_IslandNavSubsystem;

UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_OriginComboBox : public UComboBoxString
{
	GENERATED_BODY()

public:
	void ConfigureForIslandNav();
	void SetNavRowContext(int32 InRowIndex, class UIH_P1C08_IslandNavWidget* InOwner);
	void RefreshOriginComboColors();
	void OpenDropdown();
	void CloseDropdown();

private:
	UFUNCTION()
	UWidget* HandleGenerateComboWidget(FString Item);

	UWidget* MakeOriginComboLabelWidget(const FString& Item, bool bHighlighted) const;

	int32 NavRowIndex = INDEX_NONE;
	TWeakObjectPtr<UIH_P1C08_IslandNavWidget> NavWidgetOwner;
};

UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_IslandNavWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	bool ProcessPanelPointerDown(const FVector2D& ScreenAbsolute);
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const { return IsPointOverPanel(ScreenAbsolute); }
	void UpdatePanelLayout();
	void RefreshTableFromSubsystem();
	void ApplyDevPanelStackPosition(float TopY, float ContentHeight);
	void SyncSelectionFromSubsystem(int32 IslandIndex);
	int32 GetSelectedRowIndex() const { return SelectedRowIndex; }

private:
	struct FIslandNavRowWidgets
	{
		TObjectPtr<UHorizontalBox> RowBox;
		// Sibling overlay slot to RowBox, inside a shared UOverlay - UMG's own overlay layout
		// automatically sizes/positions it to match RowBox exactly (no manual geometry code).
		// Visibility toggled per selection state instead of being drawn manually.
		TObjectPtr<UBorder> HighlightBorder;
		TObjectPtr<UComboBoxString> OriginCombo;
		TObjectPtr<class USizeBox> OriginBox;
		TObjectPtr<UTextBlock> IslandLabel;
		TObjectPtr<UTextBlock> NameLabel;
		TObjectPtr<UTextBlock> TypeLabel;
		TObjectPtr<UTextBlock> AcresLabel;
		// Plan Addendum 15: physical row slot (RowIndex, used by HitTest/SelectRow/click-handling)
		// no longer equals the island's true spawn index once rows are displayed sorted by
		// descending acreage - this is the stable lookup back to Nav->GetIslandRecords()/
		// GetSpawnedIsland(), set from the sorted record at RebuildTableRows time.
		int32 IslandIndex = INDEX_NONE;
	};

	UFUNCTION()
	void HandleOriginSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void EnsureWidgetTree();
	void BindSubsystem();
	void UnbindSubsystem();
	void RefreshFromSubsystem();
	void RebuildTableRows();
	/** Applies the current SelectedRowIndex to each row's own RowBox (drives its own highlight
	 * paint) - call whenever SelectedRowIndex changes, and after RebuildTableRows since that
	 * constructs fresh row widgets with no selection state of their own. */
	void UpdateRowHighlights();
	/** Blue outline + nav index only — no camera fly (origin dropdown clicks). */
	void HighlightRowForNavUI(int32 RowIndex);
	/** Full island selection: focus camera, coastline tuning, etc. */
	void SelectRow(int32 RowIndex);
	void ClearRowSelection();
	void CloseAllOriginDropdowns();
	bool IsAnyOriginDropdownOpen() const;
	void RefreshAllOriginComboColors();
	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;
	int32 HitTestRowIndex(const FVector2D& ScreenAbsolute) const;
	int32 HitTestOriginComboRowIndex(const FVector2D& ScreenAbsolute) const;
	UIH_P1C08_IslandNavSubsystem* GetNavSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HeaderRow;

	UPROPERTY(Transient)
	TObjectPtr<class UVerticalBox> TableBox;

	TArray<FIslandNavRowWidgets> RowWidgets;
	TArray<FString> RowOriginOptions;
	int32 SelectedRowIndex = INDEX_NONE;
	bool bSuppressOriginChanged = false;
	FDelegateHandle NavChangedHandle;
	FDelegateHandle SelectionChangedHandle;
};
