// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_IslandNavWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IH_P1C08_IslandNavSubsystem.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IHMapSeedFrameworkLibrary.h"
#include "IH_Cube2FlyPlayerController.h"
#include "IHUIColorSchemeLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboBox.h"

namespace IHIslandNavComboStyle
{
	static FSlateColor MakeComboFieldBackground()
	{
		return FSlateColor(FLinearColor(0.04f, 0.05f, 0.06f, 1.f));
	}
}

void UIH_P1C08_OriginComboBox::ConfigureForIslandNav()
{
	SetContentPadding(FMargin(4.f, 1.f, 2.f, 1.f));

	FComboBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	Style.SetContentPadding(FMargin(4.f, 1.f, 2.f, 1.f));
	Style.SetMenuRowPadding(FMargin(6.f, 2.f, 8.f, 2.f));

	FComboButtonStyle ButtonStyle = Style.ComboButtonStyle;
	ButtonStyle.SetContentPadding(FMargin(0.f));
	ButtonStyle.SetDownArrowPadding(FMargin(2.f, 0.f, 0.f, 0.f));
	FButtonStyle InnerButton = ButtonStyle.ButtonStyle;
	InnerButton.Normal.TintColor = IHIslandNavComboStyle::MakeComboFieldBackground();
	InnerButton.Hovered.TintColor = FSlateColor(FLinearColor(0.06f, 0.07f, 0.08f, 1.f));
	InnerButton.Pressed.TintColor = InnerButton.Hovered.TintColor;
	ButtonStyle.SetButtonStyle(InnerButton);
	Style.SetComboButtonStyle(ButtonStyle);

	SetWidgetStyle(Style);

	OnGenerateWidgetEvent.BindUFunction(this, FName("HandleGenerateComboWidget"));
}

void UIH_P1C08_OriginComboBox::SetNavRowContext(int32 InRowIndex, UIH_P1C08_IslandNavWidget* InOwner)
{
	NavRowIndex = InRowIndex;
	NavWidgetOwner = InOwner;
	RefreshOriginComboColors();
}

void UIH_P1C08_OriginComboBox::RefreshOriginComboColors()
{
	if (const FString Selected = GetSelectedOption(); !Selected.IsEmpty())
	{
		UpdateOrGenerateWidget(MakeShareable(new FString(Selected)));
	}
	InvalidateLayoutAndVolatility();
}

UWidget* UIH_P1C08_OriginComboBox::MakeOriginComboLabelWidget(const FString& Item, bool bHighlighted) const
{
	UTextBlock* Text = NewObject<UTextBlock>(const_cast<UIH_P1C08_OriginComboBox*>(this));
	Text->SetText(FText::FromString(Item));
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", IH_P1C08_DevPanelStyle::OriginComboFontSize));
	Text->SetJustification(ETextJustify::Left); // Plan Addendum 20: user asked for left-justified dropdown text
	Text->SetColorAndOpacity(FSlateColor(
		bHighlighted
			? IH_P1C08_DevPanelStyle::IslandNavOriginActiveTextColor
			: IH_P1C08_DevPanelStyle::IslandNavOriginInactiveTextColor));
	return Text;
}

UWidget* UIH_P1C08_OriginComboBox::HandleGenerateComboWidget(FString Item)
{
	bool bHighlighted = false;
	if (IsOpen())
	{
		bHighlighted = Item == GetSelectedOption();
	}
	else if (const UIH_P1C08_IslandNavWidget* Owner = NavWidgetOwner.Get())
	{
		bHighlighted = NavRowIndex == Owner->GetSelectedRowIndex();
	}
	return MakeOriginComboLabelWidget(Item, bHighlighted);
}

void UIH_P1C08_OriginComboBox::OpenDropdown()
{
	if (MyComboBox.IsValid() && !MyComboBox->IsOpen())
	{
		MyComboBox->SetIsOpen(true, true);
	}
}

void UIH_P1C08_OriginComboBox::CloseDropdown()
{
	if (MyComboBox.IsValid() && MyComboBox->IsOpen())
	{
		MyComboBox->SetIsOpen(false, false);
	}
}

namespace
{
	static UTextBlock* MakeCellLabel(
		UWidgetTree* Tree,
		const FName& Name,
		const FString& Text,
		float MinWidth,
		ETextJustify::Type Justification = ETextJustify::Left)
	{
		UTextBlock* Label = IH_P1C08_DevPanelStyle::MakeHUDLabel(
			Tree, Name, Text, FName(TEXT("SecondaryText")));
		Label->SetMinDesiredWidth(MinWidth);
		Label->SetJustification(Justification);
		return Label;
	}

	static UHorizontalBoxSlot* AddTableColumn(
		UHorizontalBox* Row,
		UWidget* Child,
		ESlateSizeRule::Type SizeRule,
		float MinWidth = 0.f,
		float RightPadding = 0.f)
	{
		if (!Row || !Child)
		{
			return nullptr;
		}

		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Child))
		{
			Slot->SetSize(FSlateChildSize(SizeRule));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, RightPadding, 0.f));
			if (MinWidth > 0.f)
			{
				if (UTextBlock* Label = Cast<UTextBlock>(Child))
				{
					Label->SetMinDesiredWidth(MinWidth);
				}
				else if (USizeBox* SizeBox = Cast<USizeBox>(Child))
				{
					SizeBox->SetMinDesiredWidth(MinWidth);
				}
			}
			return Slot;
		}

		return nullptr;
	}
}

UIH_P1C08_IslandNavSubsystem* UIH_P1C08_IslandNavWidget::GetNavSubsystem() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	}
	return nullptr;
}

void UIH_P1C08_IslandNavWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("IslandNavRoot"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("IslandNavPanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder);

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("IslandNavVBox"));
	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("IslandNavTitle"), TEXT("Island Nav"));
	if (UVerticalBoxSlot* TitleSlot = VB->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	// Plan Addendum 22: all headers center-aligned over their own column except Acres, which
	// stays right-justified to match its (also right-justified) numeric data below.
	HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("IslandNavHeader"));
	AddTableColumn(
		HeaderRow,
		MakeCellLabel(
			WidgetTree,
			TEXT("HdrIsland"),
			TEXT("Island"),
			IH_P1C08_DevPanelStyle::IslandNavColIslandWidth,
			ETextJustify::Center),
		ESlateSizeRule::Automatic,
		IH_P1C08_DevPanelStyle::IslandNavColIslandWidth,
		4.f);
	AddTableColumn(
		HeaderRow,
		MakeCellLabel(
			WidgetTree,
			TEXT("HdrName"),
			TEXT("Name"),
			IH_P1C08_DevPanelStyle::IslandNavColNameWidth,
			ETextJustify::Center),
		ESlateSizeRule::Automatic,
		IH_P1C08_DevPanelStyle::IslandNavColNameWidth,
		4.f);
	AddTableColumn(
		HeaderRow,
		MakeCellLabel(
			WidgetTree,
			TEXT("HdrOrigin"),
			TEXT("Origin"),
			IH_P1C08_DevPanelStyle::IslandNavColOriginWidth,
			ETextJustify::Center),
		ESlateSizeRule::Automatic,
		IH_P1C08_DevPanelStyle::IslandNavColOriginWidth,
		4.f);
	AddTableColumn(
		HeaderRow,
		MakeCellLabel(
			WidgetTree,
			TEXT("HdrType"),
			TEXT("Type"),
			IH_P1C08_DevPanelStyle::IslandNavColShapeWidth,
			ETextJustify::Center),
		ESlateSizeRule::Automatic,
		IH_P1C08_DevPanelStyle::IslandNavColShapeWidth,
		4.f);
	AddTableColumn(
		HeaderRow,
		MakeCellLabel(
			WidgetTree,
			TEXT("HdrAcres"),
			TEXT("Acres"),
			IH_P1C08_DevPanelStyle::IslandNavColSectorsWidth,
			ETextJustify::Right),
		ESlateSizeRule::Automatic,
		IH_P1C08_DevPanelStyle::IslandNavColSectorsWidth);
	if (UVerticalBoxSlot* HeaderSlot = VB->AddChildToVerticalBox(HeaderRow))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	TableBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("IslandNavTable"));
	VB->AddChildToVerticalBox(TableBox);

	PanelBorder->AddChild(VB);

	// Plan Addendum 21: force this panel to the same width as Realm Seed/Sun Position (user
	// reported the three top-left panels rendering at different widths despite sharing the same
	// CanvasSlot PanelWidth) - wrapping in a SizeBox with an explicit width override guarantees
	// it regardless of whether the Border's own content happens to be narrower.
	USizeBox* WidthLock = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IslandNavWidthLock"));
	WidthLock->SetWidthOverride(IH_P1C08_DevPanelStyle::PanelWidth);
	WidthLock->AddChild(PanelBorder);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(WidthLock))
	{
		int32 IslandCount = 3;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UIH_WB_Demo004GameInstance* StoryGI = Cast<UIH_WB_Demo004GameInstance>(GI))
			{
				IslandCount = StoryGI->GetProceduralIslandCount();
			}
		}

		IH_P1C08_DevPanelStyle::ConfigureTopLeftPanelSlot(
			CSlot,
			IH_P1C08_DevPanelStyle::EStackSlot::IslandNav,
			IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
				IH_P1C08_DevPanelStyle::EStackSlot::IslandNav, IslandCount),
			IslandCount);
	}
}

TSharedRef<SWidget> UIH_P1C08_IslandNavWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_IslandNavWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.f, 0.f)); // Plan Addendum 19
	BindSubsystem();
	RefreshFromSubsystem();
}

void UIH_P1C08_IslandNavWidget::NativeDestruct()
{
	UnbindSubsystem();
	Super::NativeDestruct();
}

void UIH_P1C08_IslandNavWidget::BindSubsystem()
{
	if (UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem())
	{
		if (!NavChangedHandle.IsValid())
		{
			NavChangedHandle = Nav->OnIslandNavChanged.AddUObject(this, &UIH_P1C08_IslandNavWidget::RefreshFromSubsystem);
		}
		if (!SelectionChangedHandle.IsValid())
		{
			SelectionChangedHandle = Nav->OnSelectionChanged.AddUObject(
				this, &UIH_P1C08_IslandNavWidget::SyncSelectionFromSubsystem);
		}
	}
}

void UIH_P1C08_IslandNavWidget::UnbindSubsystem()
{
	if (UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem())
	{
		if (NavChangedHandle.IsValid())
		{
			Nav->OnIslandNavChanged.Remove(NavChangedHandle);
			NavChangedHandle.Reset();
		}
		if (SelectionChangedHandle.IsValid())
		{
			Nav->OnSelectionChanged.Remove(SelectionChangedHandle);
			SelectionChangedHandle.Reset();
		}
	}
}

void UIH_P1C08_IslandNavWidget::ApplyDevPanelStackPosition(float TopY, float ContentHeight)
{
	if (!PanelBorder || !WidgetTree || !WidgetTree->RootWidget)
	{
		return;
	}

	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
	{
		if (Canvas->GetSlots().Num() > 0)
		{
			if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Canvas->GetSlots()[0]))
			{
				IH_P1C08_DevPanelStyle::ApplyTopLeftPanelSlotAtY(CSlot, TopY, ContentHeight);
			}
		}
	}
}

void UIH_P1C08_IslandNavWidget::RefreshTableFromSubsystem()
{
	RefreshFromSubsystem();
}

void UIH_P1C08_IslandNavWidget::UpdatePanelLayout()
{
	int32 IslandCount = 3;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		IslandCount = GI->GetProceduralIslandCount();
	}

	const float ContentHeight = IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
		IH_P1C08_DevPanelStyle::EStackSlot::IslandNav, IslandCount);

	ApplyDevPanelStackPosition(
		IH_P1C08_DevPanelStyle::GetStackPosition(
			IH_P1C08_DevPanelStyle::EStackSlot::IslandNav, IslandCount).Y,
		ContentHeight);
}

void UIH_P1C08_IslandNavWidget::RebuildTableRows()
{
	if (!TableBox || !WidgetTree)
	{
		return;
	}

	TableBox->ClearChildren();
	RowWidgets.Reset();

	UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem();
	if (!Nav)
	{
		return;
	}

	RowOriginOptions = Nav->GetAvailableOrigins();
	const TArray<FIHIslandNavRecord>& Records = Nav->GetIslandRecords();

	// Plan Addendum 15: display rows sorted by descending realized DryAcres (SetIslandDryAcres,
	// Addendum 9, made DryAcres reflect actual generated acreage rather than the frozen
	// pre-generation budget - which broke the old "Island 1 = largest" guarantee that used to
	// hold automatically from the budget table's own fixed descending ratios). Sort a COPY for
	// display only; Nav->GetIslandRecords()/GetSpawnedIsland() and everything else still key off
	// the stable original spawn IslandIndex, recovered per-row via FIslandNavRowWidgets::IslandIndex.
	TArray<FIHIslandNavRecord> SortedRecords = Records;
	SortedRecords.Sort([](const FIHIslandNavRecord& A, const FIHIslandNavRecord& B)
	{
		return A.DryAcres > B.DryAcres;
	});

	int32 DisplayRank = 0;
	for (const FIHIslandNavRecord& Record : SortedRecords)
	{
		++DisplayRank;
		FIslandNavRowWidgets Row;
		Row.IslandIndex = Record.IslandIndex;
		Row.RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("IslandRow_%d"), Record.IslandIndex));

		Row.IslandLabel = MakeCellLabel(
			WidgetTree,
			*FString::Printf(TEXT("IslandIdx_%d"), Record.IslandIndex),
			FString::Printf(TEXT("%d"), DisplayRank),
			IH_P1C08_DevPanelStyle::IslandNavColIslandWidth);
		AddTableColumn(
			Row.RowBox,
			Row.IslandLabel,
			ESlateSizeRule::Automatic,
			IH_P1C08_DevPanelStyle::IslandNavColIslandWidth,
			4.f);

		Row.NameLabel = MakeCellLabel(
			WidgetTree,
			*FString::Printf(TEXT("IslandName_%d"), Record.IslandIndex),
			Record.Name,
			IH_P1C08_DevPanelStyle::IslandNavColNameWidth);
		AddTableColumn(
			Row.RowBox,
			Row.NameLabel,
			ESlateSizeRule::Automatic,
			IH_P1C08_DevPanelStyle::IslandNavColNameWidth,
			4.f);

		Row.OriginCombo = WidgetTree->ConstructWidget<UIH_P1C08_OriginComboBox>(
			UIH_P1C08_OriginComboBox::StaticClass(),
			*FString::Printf(TEXT("OriginCombo_%d"), Record.IslandIndex));
		if (UIH_P1C08_OriginComboBox* OriginCombo = Cast<UIH_P1C08_OriginComboBox>(Row.OriginCombo))
		{
			OriginCombo->ConfigureForIslandNav();
			OriginCombo->SetNavRowContext(RowWidgets.Num(), this);
		}
		for (const FString& Origin : RowOriginOptions)
		{
			Row.OriginCombo->AddOption(Origin);
		}
		Row.OriginCombo->SetSelectedOption(Record.Origin);

		Row.OriginBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("OriginBox_%d"), Record.IslandIndex));
		Row.OriginBox->SetWidthOverride(IH_P1C08_DevPanelStyle::IslandNavColOriginWidth);
		Row.OriginBox->AddChild(Row.OriginCombo);
		AddTableColumn(
			Row.RowBox,
			Row.OriginBox,
			ESlateSizeRule::Automatic,
			IH_P1C08_DevPanelStyle::IslandNavColOriginWidth,
			4.f);

		Row.OriginCombo->OnSelectionChanged.AddDynamic(this, &UIH_P1C08_IslandNavWidget::HandleOriginSelectionChanged);

		Row.TypeLabel = MakeCellLabel(
			WidgetTree,
			*FString::Printf(TEXT("IslandType_%d"), Record.IslandIndex),
			UIHMapSeedFrameworkLibrary::IslandTemplateTypeToNavAbbrev(Record.TemplateType),
			IH_P1C08_DevPanelStyle::IslandNavColShapeWidth,
			ETextJustify::Center);
		AddTableColumn(
			Row.RowBox,
			Row.TypeLabel,
			ESlateSizeRule::Automatic,
			IH_P1C08_DevPanelStyle::IslandNavColShapeWidth,
			4.f);

		Row.AcresLabel = MakeCellLabel(
			WidgetTree,
			*FString::Printf(TEXT("IslandAcres_%d"), Record.IslandIndex),
			FString::Printf(TEXT("%d"), Record.DryAcres),
			IH_P1C08_DevPanelStyle::IslandNavColSectorsWidth,
			ETextJustify::Right);
		AddTableColumn(
			Row.RowBox,
			Row.AcresLabel,
			ESlateSizeRule::Automatic,
			IH_P1C08_DevPanelStyle::IslandNavColSectorsWidth);

		// Highlight is a sibling overlay slot on top of RowBox, inside a shared UOverlay - UMG's
		// own overlay layout stretches it to match RowBox's size/position automatically, so there
		// is no manual geometry math of any kind involved in showing it on the right row.
		Row.HighlightBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("IslandRowHighlight_%d"), Record.IslandIndex));
		{
			static const FSlateRoundedBoxBrush RowOutlineBrush(
				FLinearColor::Transparent,
				FVector4(3.f, 3.f, 3.f, 3.f),
				IH_P1C08_DevPanelStyle::RowSelectionOutlineColor,
				IH_P1C08_DevPanelStyle::RowSelectionOutlineThickness);
			Row.HighlightBorder->SetBrush(RowOutlineBrush);
		}
		// Collapsed (not just opacity 0) is the unambiguous "not shown" state in UMG - it removes
		// the border from painting entirely rather than relying on an opacity value.
		Row.HighlightBorder->SetVisibility(ESlateVisibility::Collapsed);

		UOverlay* RowOverlay = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(),
			*FString::Printf(TEXT("IslandRowOverlay_%d"), Record.IslandIndex));
		if (UOverlaySlot* RowBoxSlot = RowOverlay->AddChildToOverlay(Row.RowBox))
		{
			RowBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			RowBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}
		// HighlightBorder has no content of its own, so without an explicit Fill/Fill alignment
		// its slot shrinks to the border's own minimal (near-zero) desired size and sits at the
		// slot's default corner - it must be told to stretch to match RowBox's overlay bounds.
		if (UOverlaySlot* HighlightSlot = RowOverlay->AddChildToOverlay(Row.HighlightBorder))
		{
			HighlightSlot->SetHorizontalAlignment(HAlign_Fill);
			HighlightSlot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UVerticalBoxSlot* RowSlot = TableBox->AddChildToVerticalBox(RowOverlay))
		{
			RowSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
		}

		RowWidgets.Add(Row);
	}

	RefreshAllOriginComboColors();
}

void UIH_P1C08_IslandNavWidget::RefreshAllOriginComboColors()
{
	for (const FIslandNavRowWidgets& Row : RowWidgets)
	{
		if (UIH_P1C08_OriginComboBox* OriginCombo = Cast<UIH_P1C08_OriginComboBox>(Row.OriginCombo.Get()))
		{
			OriginCombo->RefreshOriginComboColors();
		}
	}
}

void UIH_P1C08_IslandNavWidget::RefreshFromSubsystem()
{
	if (UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem())
	{
		SetVisibility(Nav->IsPreBakeMode() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	bSuppressOriginChanged = true;
	RebuildTableRows();
	bSuppressOriginChanged = false;
	if (!RowWidgets.IsValidIndex(SelectedRowIndex))
	{
		SelectedRowIndex = INDEX_NONE;
	}
	// RebuildTableRows constructs brand-new row widgets with no selection state of their own -
	// reapply whatever SelectedRowIndex survived the rebuild.
	UpdateRowHighlights();
	UpdatePanelLayout();
	Invalidate(EInvalidateWidget::LayoutAndVolatility | EInvalidateWidget::Paint);
}

void UIH_P1C08_IslandNavWidget::UpdateRowHighlights()
{
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (UBorder* HighlightBorder = RowWidgets[Index].HighlightBorder.Get())
		{
			HighlightBorder->SetVisibility(
				Index == SelectedRowIndex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UIH_P1C08_IslandNavWidget::HandleOriginSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressOriginChanged)
	{
		return;
	}

	UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem();
	if (!Nav || SelectedItem.IsEmpty())
	{
		return;
	}

	const TArray<FIHIslandNavRecord>& Records = Nav->GetIslandRecords();
	for (int32 RowIndex = 0; RowIndex < RowWidgets.Num() && Records.IsValidIndex(RowIndex); ++RowIndex)
	{
		const FIslandNavRowWidgets& Row = RowWidgets[RowIndex];
		if (!Row.OriginCombo || Row.OriginCombo->GetSelectedOption() != SelectedItem)
		{
			continue;
		}

		const int32 IslandIndex = RowWidgets[RowIndex].IslandIndex;
		if (Records.IsValidIndex(IslandIndex) && SelectedItem != Records[IslandIndex].Origin)
		{
			Nav->SetIslandOrigin(IslandIndex, SelectedItem);
			RefreshAllOriginComboColors();
		}
		break;
	}
}

bool UIH_P1C08_IslandNavWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	if (!PanelBorder)
	{
		return false;
	}

	const FGeometry& Geo = PanelBorder->GetCachedGeometry();
	return Geo.IsUnderLocation(ScreenAbsolute);
}

int32 UIH_P1C08_IslandNavWidget::HitTestRowIndex(const FVector2D& ScreenAbsolute) const
{
	for (int32 RowIndex = 0; RowIndex < RowWidgets.Num(); ++RowIndex)
	{
		const UHorizontalBox* RowBox = RowWidgets[RowIndex].RowBox.Get();
		if (!RowBox || !RowBox->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
		{
			continue;
		}

		const FIslandNavRowWidgets& Row = RowWidgets[RowIndex];
		if (Row.OriginBox && Row.OriginBox->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
		{
			continue;
		}

		return RowIndex;
	}
	return INDEX_NONE;
}

int32 UIH_P1C08_IslandNavWidget::HitTestOriginComboRowIndex(const FVector2D& ScreenAbsolute) const
{
	for (int32 RowIndex = 0; RowIndex < RowWidgets.Num(); ++RowIndex)
	{
		const FIslandNavRowWidgets& Row = RowWidgets[RowIndex];
		if (Row.OriginBox && Row.OriginBox->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
		{
			return RowIndex;
		}
		if (const UComboBoxString* OriginCombo = Row.OriginCombo.Get())
		{
			if (OriginCombo->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
			{
				return RowIndex;
			}
		}
	}
	return INDEX_NONE;
}

void UIH_P1C08_IslandNavWidget::CloseAllOriginDropdowns()
{
	for (const FIslandNavRowWidgets& Row : RowWidgets)
	{
		if (UIH_P1C08_OriginComboBox* OriginCombo = Cast<UIH_P1C08_OriginComboBox>(Row.OriginCombo.Get()))
		{
			OriginCombo->CloseDropdown();
		}
	}
}

bool UIH_P1C08_IslandNavWidget::IsAnyOriginDropdownOpen() const
{
	for (const FIslandNavRowWidgets& Row : RowWidgets)
	{
		if (const UComboBoxString* OriginCombo = Row.OriginCombo.Get())
		{
			if (OriginCombo->IsOpen())
			{
				return true;
			}
		}
	}
	return false;
}

void UIH_P1C08_IslandNavWidget::SyncSelectionFromSubsystem(int32 IslandIndex)
{
	(void)IslandIndex;

	int32 NewSelectedRowIndex = INDEX_NONE;
	if (const UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem())
	{
		// Rows display sorted by descending acreage (Plan Addendum 15), so the physical row
		// position is generally NOT the same as the island's stable spawn index that the
		// subsystem tracks - resolve it via each row's own stored IslandIndex instead of
		// assuming they match (that mismatch was clobbering the correct row selection with
		// the wrong row whenever this delegate fired after a click).
		const int32 SelectedIslandIndex = Nav->GetSelectedIslandIndex();
		for (int32 RowIndex = 0; RowIndex < RowWidgets.Num(); ++RowIndex)
		{
			if (RowWidgets[RowIndex].IslandIndex == SelectedIslandIndex)
			{
				NewSelectedRowIndex = RowIndex;
				break;
			}
		}
	}
	SelectedRowIndex = NewSelectedRowIndex;
	UpdateRowHighlights();
	RefreshAllOriginComboColors();
	Invalidate(EInvalidateWidget::Paint);
}

void UIH_P1C08_IslandNavWidget::HighlightRowForNavUI(int32 RowIndex)
{
	if (!RowWidgets.IsValidIndex(RowIndex))
	{
		return;
	}

	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (Index == RowIndex)
		{
			continue;
		}

		if (UIH_P1C08_OriginComboBox* OriginCombo = Cast<UIH_P1C08_OriginComboBox>(RowWidgets[Index].OriginCombo.Get()))
		{
			OriginCombo->CloseDropdown();
		}
	}

	if (SelectedRowIndex == RowIndex)
	{
		RefreshAllOriginComboColors();
		return;
	}

	SelectedRowIndex = RowIndex;
	UpdateRowHighlights();
	RefreshAllOriginComboColors();
	Invalidate(EInvalidateWidget::Paint);
}

void UIH_P1C08_IslandNavWidget::SelectRow(int32 RowIndex)
{
	if (!RowWidgets.IsValidIndex(RowIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("IslandNav: SelectRow called with OUT-OF-RANGE RowIndex=%d (RowWidgets.Num=%d)"),
			RowIndex, RowWidgets.Num());
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("IslandNav: SelectRow RowIndex=%d IslandIndex=%d prevSelectedRowIndex=%d"),
		RowIndex, RowWidgets[RowIndex].IslandIndex, SelectedRowIndex);

	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (Index == RowIndex)
		{
			continue;
		}

		if (UIH_P1C08_OriginComboBox* OriginCombo = Cast<UIH_P1C08_OriginComboBox>(RowWidgets[Index].OriginCombo.Get()))
		{
			OriginCombo->CloseDropdown();
		}
	}

	// Plan Addendum 15: RowIndex is the physical UI slot, no longer the island's true spawn
	// index now that rows display sorted by descending acreage - resolve via the row's own
	// stored IslandIndex before touching camera-fly/selection state.
	const int32 IslandIndex = RowWidgets[RowIndex].IslandIndex;

	if (SelectedRowIndex == RowIndex)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC))
			{
				FlyPC->RequestFocusIsland(IslandIndex);
				// Plan Addendum 8: recenter the camera on Nav-row click (single click is fine per
				// user feedback - no double-click gesture needed). Scoped to the Nav panel only;
				// RequestFocusIsland's other caller (viewport click-to-select) is untouched.
				FlyPC->BeginCameraFlyToIsland(IslandIndex);
			}
		}
		RefreshAllOriginComboColors();
		Invalidate(EInvalidateWidget::Paint);
		return;
	}

	SelectedRowIndex = RowIndex;
	UpdateRowHighlights();

	RefreshAllOriginComboColors();
	Invalidate(EInvalidateWidget::Paint);

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC))
		{
			FlyPC->RequestFocusIsland(IslandIndex);
			FlyPC->BeginCameraFlyToIsland(IslandIndex);
			return;
		}
	}

	if (UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem())
	{
		Nav->SetSelectedIslandIndex(IslandIndex);
	}
}

void UIH_P1C08_IslandNavWidget::ClearRowSelection()
{
	if (SelectedRowIndex == INDEX_NONE)
	{
		return;
	}

	SelectedRowIndex = INDEX_NONE;
	UpdateRowHighlights();
	if (UIH_P1C08_IslandNavSubsystem* Nav = GetNavSubsystem())
	{
		Nav->SetSelectedIslandIndex(INDEX_NONE);
	}
	RefreshAllOriginComboColors();
	Invalidate(EInvalidateWidget::Paint);
}

bool UIH_P1C08_IslandNavWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!IsPointOverPanel(ScreenAbsolute))
	{
		return false;
	}
	UE_LOG(LogTemp, Warning,
		TEXT("IslandNav: HandleScreenPointerDown over panel at (%.0f,%.0f) RowWidgets.Num=%d HitTestRowIndex=%d"),
		ScreenAbsolute.X, ScreenAbsolute.Y, RowWidgets.Num(), HitTestRowIndex(ScreenAbsolute));

	if (IsAnyOriginDropdownOpen())
	{
		if (HitTestOriginComboRowIndex(ScreenAbsolute) != INDEX_NONE)
		{
			return false;
		}

		const int32 HitRow = HitTestRowIndex(ScreenAbsolute);
		if (HitRow != INDEX_NONE)
		{
			CloseAllOriginDropdowns();
			SelectRow(HitRow);
			return true;
		}

		return false;
	}

	const int32 ComboRow = HitTestOriginComboRowIndex(ScreenAbsolute);
	if (ComboRow != INDEX_NONE)
	{
		HighlightRowForNavUI(ComboRow);
		if (UIH_P1C08_OriginComboBox* OriginCombo = Cast<UIH_P1C08_OriginComboBox>(RowWidgets[ComboRow].OriginCombo.Get()))
		{
			OriginCombo->OpenDropdown();
		}
		return true;
	}

	const int32 HitRow = HitTestRowIndex(ScreenAbsolute);
	if (HitRow != INDEX_NONE)
	{
		CloseAllOriginDropdowns();
		SelectRow(HitRow);
	}
	return true;
}

bool UIH_P1C08_IslandNavWidget::ProcessPanelPointerDown(const FVector2D& ScreenAbsolute)
{
	return HandleScreenPointerDown(ScreenAbsolute);
}

