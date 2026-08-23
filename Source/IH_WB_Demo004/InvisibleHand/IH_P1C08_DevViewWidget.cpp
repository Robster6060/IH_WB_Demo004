// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_DevViewWidget.h"

#include "IH_BuildPalettePanelStyle.h"
#include "IH_P1C08_DevPanelStyle.h"
#include "IHDevViewRuntime.h"
#include "IHUIColorSchemeLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	static constexpr float DevViewPanelMinWidthPx = IH_BuildPalettePanelStyle::TopRightHudDevViewW;

	static UCheckBox* MakeRow(
		UWidgetTree* Tree,
		UVerticalBox* VB,
		const TCHAR* RowName,
		const TCHAR* Label)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName);
		UCheckBox* Check = Tree->ConstructWidget<UCheckBox>(
			UCheckBox::StaticClass(), *FString::Printf(TEXT("%sCheck"), RowName));
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), RowName));
		Text->SetText(FText::FromString(Label));
		Text->SetColorAndOpacity(
			FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("SecondaryText")))));
		IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(Text);
		Text->SetAutoWrapText(false);
		if (UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(Check))
		{
			CheckSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			CheckSlot->SetVerticalAlignment(VAlign_Center);
			CheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text))
		{
			TextSlot->SetVerticalAlignment(VAlign_Center);
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		if (UVerticalBoxSlot* RowSlot = VB->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		return Check;
	}
}

void UIH_P1C08_DevViewWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DevViewRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DevViewSizeBox"));
	PanelSizeBox->SetWidthOverride(DevViewPanelMinWidthPx);
	PanelSizeBox->SetMinDesiredWidth(DevViewPanelMinWidthPx);

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DevViewPanel"));
	PanelBorder->SetPadding(FMargin(12.f, 10.f));
	{
		FSlateBrush PanelBrush;
		PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PanelBrush.TintColor = FSlateColor(
			UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(FName(TEXT("PanelBackground")), 0.82f));
		PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PanelBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
		PanelBrush.OutlineSettings.Color =
			UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("BorderFrameTint")));
		PanelBrush.OutlineSettings.Width = 1.f;
		PanelBorder->SetBrush(PanelBrush);
	}

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DevViewVBox"));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DevViewTitle"));
	Title->SetText(FText::FromString(TEXT("DEV View")));
	Title->SetColorAndOpacity(
		FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")))));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(Title);
	VB->AddChildToVerticalBox(Title);

	OceanCheck = MakeRow(WidgetTree, VB, TEXT("Ocean"), TEXT("Ocean"));
	ContoursCheck = MakeRow(WidgetTree, VB, TEXT("Contours"), TEXT("Contours"));
	FeaturesCheck = MakeRow(WidgetTree, VB, TEXT("Features"), TEXT("Features"));
	CloudsCheck = MakeRow(WidgetTree, VB, TEXT("Clouds"), TEXT("Clouds"));
	GrabContrastCheck = MakeRow(WidgetTree, VB, TEXT("GrabContrast"), TEXT("GrabContrast"));

	OceanCheck->OnCheckStateChanged.AddDynamic(this, &UIH_P1C08_DevViewWidget::HandleOceanChanged);
	ContoursCheck->OnCheckStateChanged.AddDynamic(this, &UIH_P1C08_DevViewWidget::HandleContoursChanged);
	FeaturesCheck->OnCheckStateChanged.AddDynamic(this, &UIH_P1C08_DevViewWidget::HandleFeaturesChanged);
	CloudsCheck->OnCheckStateChanged.AddDynamic(this, &UIH_P1C08_DevViewWidget::HandleCloudsChanged);
	GrabContrastCheck->OnCheckStateChanged.AddDynamic(this, &UIH_P1C08_DevViewWidget::HandleGrabContrastChanged);

	PanelBorder->AddChild(VB);
	PanelSizeBox->AddChild(PanelBorder);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelSizeBox))
	{
		CSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		CSlot->SetAlignment(FVector2D(1.f, 0.f));
		CSlot->SetPosition(FVector2D(
			-IH_BuildPalettePanelStyle::TopRightHudClusterRightClearPx,
			IH_BuildPalettePanelStyle::TopRightHudClusterTopY));
		CSlot->SetAutoSize(true);
	}
}

TSharedRef<SWidget> UIH_P1C08_DevViewWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_DevViewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(1.f, 0.f)); // Plan Addendum 19
	SyncChecksFromRuntime();
}

void UIH_P1C08_DevViewWidget::SyncChecksFromRuntime()
{
	bSuppressCheckNotify = true;
	if (OceanCheck) OceanCheck->SetIsChecked(IHDevViewRuntime::IsOceanVisible());
	if (ContoursCheck) ContoursCheck->SetIsChecked(IHDevViewRuntime::AreContoursVisible());
	if (FeaturesCheck) FeaturesCheck->SetIsChecked(IHDevViewRuntime::AreFeaturesVisible());
	if (CloudsCheck) CloudsCheck->SetIsChecked(IHDevViewRuntime::AreCloudsVisible());
	if (GrabContrastCheck) GrabContrastCheck->SetIsChecked(IHDevViewRuntime::IsGrabContrastEnabled());
	bSuppressCheckNotify = false;
}

bool UIH_P1C08_DevViewWidget::IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	const UWidget* HitWidget = PanelSizeBox ? static_cast<const UWidget*>(PanelSizeBox) : PanelBorder;
	if (!HitWidget) return false;
	return HitWidget->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_DevViewWidget::TryToggleCheckAtScreen(
	UCheckBox* Check,
	void (UIH_P1C08_DevViewWidget::*Handler)(bool),
	const FVector2D& ScreenAbsolute)
{
	if (!Check || !Handler || !PanelBorder) return false;
	const FGeometry CheckGeo = Check->GetCachedGeometry();
	const FGeometry PanelGeo = PanelBorder->GetCachedGeometry();
	const FVector2D CheckTL = CheckGeo.GetAbsolutePosition();
	const FVector2D PanelTL = PanelGeo.GetAbsolutePosition();
	const float RowTop = CheckTL.Y - 2.f;
	const float RowBottom = CheckTL.Y + CheckGeo.GetAbsoluteSize().Y + 2.f;
	const float RowLeft = PanelTL.X;
	const float RowRight = PanelTL.X + PanelGeo.GetAbsoluteSize().X;
	if (ScreenAbsolute.X < RowLeft || ScreenAbsolute.X > RowRight
		|| ScreenAbsolute.Y < RowTop || ScreenAbsolute.Y > RowBottom)
	{
		return false;
	}
	const bool bNew = !Check->IsChecked();
	bSuppressCheckNotify = true;
	Check->SetIsChecked(bNew);
	bSuppressCheckNotify = false;
	(this->*Handler)(bNew);
	return true;
}

bool UIH_P1C08_DevViewWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!IsScreenPointOverPanel(ScreenAbsolute)) return false;
	if (TryToggleCheckAtScreen(OceanCheck, &UIH_P1C08_DevViewWidget::HandleOceanChanged, ScreenAbsolute)
		|| TryToggleCheckAtScreen(ContoursCheck, &UIH_P1C08_DevViewWidget::HandleContoursChanged, ScreenAbsolute)
		|| TryToggleCheckAtScreen(FeaturesCheck, &UIH_P1C08_DevViewWidget::HandleFeaturesChanged, ScreenAbsolute)
		|| TryToggleCheckAtScreen(CloudsCheck, &UIH_P1C08_DevViewWidget::HandleCloudsChanged, ScreenAbsolute)
		|| TryToggleCheckAtScreen(GrabContrastCheck, &UIH_P1C08_DevViewWidget::HandleGrabContrastChanged, ScreenAbsolute))
	{
		return true;
	}
	return true;
}

void UIH_P1C08_DevViewWidget::HandleOceanChanged(const bool bIsChecked)
{
	if (bSuppressCheckNotify) return;
	IHDevViewRuntime::SetOceanVisible(bIsChecked);
	IHDevViewRuntime::ApplyOceanVisibilityToWorld(GetWorld());
}

void UIH_P1C08_DevViewWidget::HandleContoursChanged(const bool bIsChecked)
{
	if (bSuppressCheckNotify) return;
	IHDevViewRuntime::SetContoursVisible(bIsChecked);
	IHDevViewRuntime::ApplyContoursVisibilityToWorld(GetWorld());
}

void UIH_P1C08_DevViewWidget::HandleFeaturesChanged(const bool bIsChecked)
{
	if (bSuppressCheckNotify) return;
	IHDevViewRuntime::SetFeaturesVisible(bIsChecked);
	IHDevViewRuntime::ApplyFeaturesVisibilityToWorld(GetWorld());
}

void UIH_P1C08_DevViewWidget::HandleCloudsChanged(const bool bIsChecked)
{
	if (bSuppressCheckNotify) return;
	IHDevViewRuntime::SetCloudsVisible(bIsChecked);
	IHDevViewRuntime::ApplyCloudsVisibilityToWorld(GetWorld());
}

void UIH_P1C08_DevViewWidget::HandleGrabContrastChanged(const bool bIsChecked)
{
	if (bSuppressCheckNotify) return;
	IHDevViewRuntime::SetGrabContrastEnabled(bIsChecked);
	IHDevViewRuntime::ApplyGrabContrastToWorld(GetWorld());
}
