// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_BuildPaletteHostWidget.h"



#include "IH_BuildPaletteItemRow.h"

#include "IH_BuildPaletteSubsystem.h"

#include "IH_BuildPaletteTypes.h"

#include "IH_Cube2FlyPlayerController.h"

#include "IH_BuildPalettePanelStyle.h"
#include "IH_P1C08_DevPanelStyle.h"

#include "IH_TownGridDataSubsystem.h"

#include "IHUIColorSchemeLibrary.h"



#include "IHInvisibleHandDesignSpec.h"
#include "FIHTerrainStampTypes.h"

#include "Blueprint/WidgetTree.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

#include "Components/CanvasPanel.h"

#include "Components/CanvasPanelSlot.h"

#include "Components/HorizontalBox.h"

#include "Components/SizeBox.h"

#include "Components/HorizontalBoxSlot.h"

#include "Components/TextBlock.h"

#include "Components/VerticalBox.h"

#include "Components/VerticalBoxSlot.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

#include "Engine/GameViewportClient.h"

#include "Engine/LocalPlayer.h"

#include "Framework/Application/SlateApplication.h"

#include "Widgets/SViewport.h"

#include "Styling/CoreStyle.h"

#include "UObject/EnumProperty.h"



namespace

{

	struct FBuildPaletteTabDef

	{

		const TCHAR* KeyLetter;

		const TCHAR* Label;

		bool bEnabled;

		const TCHAR* Tooltip;

	};



	static constexpr FBuildPaletteTabDef TabDefs[] = {

		{TEXT("G"), TEXT("Grid"), true, nullptr},

		{TEXT("W"), TEXT("World"), true, TEXT("World Builder level only")},

		{TEXT("B"), TEXT("Build"), true, TEXT("Phase 2+")},

		{TEXT("C"), TEXT("Convey"), true, TEXT("Unlock required")},

		{TEXT("D"), TEXT("Defense"), true, TEXT("Civic / Combat unlock")},

	};

}



UIH_BuildPaletteHostWidget::UIH_BuildPaletteHostWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedPaint = true;
}



UTextBlock* UIH_BuildPaletteHostWidget::MakeSectionHeader(const FString& Label, bool bStubSection)

{

	UTextBlock* Header = IH_P1C08_DevPanelStyle::MakeHUDLabel(

		WidgetTree,

		*FString::Printf(TEXT("Section_%s"), *Label),

		Label,

		bStubSection ? FName(TEXT("SecondaryText")) : FName(TEXT("HeadingText")));

	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(

		Header, IH_P1C08_DevPanelStyle::CompactLabelFontSize);

	if (bStubSection)

	{

		Header->SetColorAndOpacity(FSlateColor(IH_BuildPalettePanelStyle::DisabledTabText));

	}

	else

	{

		Header->SetColorAndOpacity(FSlateColor(IH_BuildPalettePanelStyle::FocusBlue));

	}

	return Header;

}



UTextBlock* UIH_BuildPaletteHostWidget::MakeStubLine(const FString& Label)

{

	UTextBlock* Stub = IH_P1C08_DevPanelStyle::MakeHUDLabel(

		WidgetTree,

		*FString::Printf(TEXT("Stub_%s"), *Label),

		Label,

		FName(TEXT("SecondaryText")));

	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(

		Stub, IH_P1C08_DevPanelStyle::CompactLabelFontSize);

	Stub->SetColorAndOpacity(FSlateColor(IH_BuildPalettePanelStyle::DisabledTabText));

	return Stub;

}



void UIH_BuildPaletteHostWidget::EnsureWidgetTree()

{

	if (!WidgetTree)

	{

		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));

		if (!WidgetTree)

		{

			UE_LOG(LogTemp, Error, TEXT("BuildPaletteHost: failed to create WidgetTree"));

			return;

		}

	}

	if (WidgetTree->RootWidget)

	{

		return;

	}



	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildPaletteRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildPaletteRootSize"));
	RootSizeBox->SetWidthOverride(IH_BuildPalettePanelStyle::TabStripWidth);
	RootSizeBox->SetHeightOverride(FMath::Max(ComputeHostPanelHeight(), 1.f));



	FlyOutBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GridFlyOutBorder"));

	IH_BuildPalettePanelStyle::ApplyRightFlyOutBorderStyle(FlyOutBorder);
	FlyOutBorder->SetPadding(FMargin(6.f, 4.f));

	UVerticalBox* FlyOutContentRoot = WidgetTree->ConstructWidget<UVerticalBox>(

		UVerticalBox::StaticClass(), TEXT("FlyOutContentRoot"));



	GridFlyOutVBox = WidgetTree->ConstructWidget<UVerticalBox>(

		UVerticalBox::StaticClass(), TEXT("GridFlyOutVBox"));



	if (UTextBlock* TownGridsHeader = MakeSectionHeader(TEXT("Town Grids")))

	{

		if (UVerticalBoxSlot* HeaderSlot = GridFlyOutVBox->AddChildToVerticalBox(TownGridsHeader))

		{

			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		}

	}



	TemplateListVBox = WidgetTree->ConstructWidget<UVerticalBox>(

		UVerticalBox::StaticClass(), TEXT("GridTemplateListVBox"));

	if (UVerticalBoxSlot* ListSlot = GridFlyOutVBox->AddChildToVerticalBox(TemplateListVBox))

	{

		ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	}



	if (UTextBlock* ZonesHeader = MakeSectionHeader(TEXT("Zones"), true))

	{

		if (UVerticalBoxSlot* HeaderSlot = GridFlyOutVBox->AddChildToVerticalBox(ZonesHeader))

		{

			HeaderSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 2.f));

		}

	}

	if (UTextBlock* ZonesStub = MakeStubLine(TEXT("P1")))

	{

		if (UVerticalBoxSlot* StubSlot = GridFlyOutVBox->AddChildToVerticalBox(ZonesStub))

		{

			StubSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 8.f));

		}

	}



	if (UTextBlock* SplinesHeader = MakeSectionHeader(TEXT("Open / Closed splines"), true))

	{

		if (UVerticalBoxSlot* HeaderSlot = GridFlyOutVBox->AddChildToVerticalBox(SplinesHeader))

		{

			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

		}

	}

	if (UTextBlock* SplinesStub = MakeStubLine(TEXT("P1")))

	{

		if (UVerticalBoxSlot* StubSlot = GridFlyOutVBox->AddChildToVerticalBox(SplinesStub))

		{

			StubSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));

		}

	}



	WorldFlyOutVBox = WidgetTree->ConstructWidget<UVerticalBox>(

		UVerticalBox::StaticClass(), TEXT("WorldFlyOutVBox"));

	if (UTextBlock* WorldHeader = MakeSectionHeader(TEXT("World"), false))

	{

		if (UVerticalBoxSlot* HeaderSlot = WorldFlyOutVBox->AddChildToVerticalBox(WorldHeader))

		{

			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		}

	}



	BuildFlyOutVBox = WidgetTree->ConstructWidget<UVerticalBox>(

		UVerticalBox::StaticClass(), TEXT("BuildFlyOutVBox"));

	if (UTextBlock* BuildHeader = MakeSectionHeader(TEXT("Build"), true))

	{

		if (UVerticalBoxSlot* HeaderSlot = BuildFlyOutVBox->AddChildToVerticalBox(BuildHeader))

		{

			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		}

	}

	if (UTextBlock* BuildStub = MakeStubLine(TEXT("Phase 2+")))

	{

		BuildFlyOutVBox->AddChildToVerticalBox(BuildStub);

	}



	ConveyFlyOutVBox = WidgetTree->ConstructWidget<UVerticalBox>(

		UVerticalBox::StaticClass(), TEXT("ConveyFlyOutVBox"));

	if (UTextBlock* ConveyHeader = MakeSectionHeader(TEXT("Convey"), true))

	{

		if (UVerticalBoxSlot* HeaderSlot = ConveyFlyOutVBox->AddChildToVerticalBox(ConveyHeader))

		{

			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		}

	}

	if (UTextBlock* ConveyStub = MakeStubLine(TEXT("Unlock required")))

	{

		ConveyFlyOutVBox->AddChildToVerticalBox(ConveyStub);

	}



	DefenseFlyOutVBox = WidgetTree->ConstructWidget<UVerticalBox>(

		UVerticalBox::StaticClass(), TEXT("DefenseFlyOutVBox"));

	if (UTextBlock* DefenseHeader = MakeSectionHeader(TEXT("Defense"), true))

	{

		if (UVerticalBoxSlot* HeaderSlot = DefenseFlyOutVBox->AddChildToVerticalBox(DefenseHeader))

		{

			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		}

	}

	if (UTextBlock* DefenseStub = MakeStubLine(TEXT("Civic / Combat unlock")))

	{

		DefenseFlyOutVBox->AddChildToVerticalBox(DefenseStub);

	}



	if (UVerticalBoxSlot* GridSlot = FlyOutContentRoot->AddChildToVerticalBox(GridFlyOutVBox))

	{

		GridSlot->SetPadding(FMargin(0.f));

	}

	if (UVerticalBoxSlot* WorldSlot = FlyOutContentRoot->AddChildToVerticalBox(WorldFlyOutVBox))

	{

		WorldSlot->SetPadding(FMargin(0.f));

	}

	if (UVerticalBoxSlot* BuildSlot = FlyOutContentRoot->AddChildToVerticalBox(BuildFlyOutVBox))

	{

		BuildSlot->SetPadding(FMargin(0.f));

	}

	if (UVerticalBoxSlot* ConveySlot = FlyOutContentRoot->AddChildToVerticalBox(ConveyFlyOutVBox))

	{

		ConveySlot->SetPadding(FMargin(0.f));

	}

	if (UVerticalBoxSlot* DefenseSlot = FlyOutContentRoot->AddChildToVerticalBox(DefenseFlyOutVBox))

	{

		DefenseSlot->SetPadding(FMargin(0.f));

	}



	FlyOutBorder->AddChild(FlyOutContentRoot);

	FlyOutSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GridFlyOutSizeBox"));
	FlyOutSizeBox->SetWidthOverride(IH_BuildPalettePanelStyle::FlyOutWidth);
	FlyOutSizeBox->AddChild(FlyOutBorder);

	HostHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildPaletteHostHBox"));
	if (UHorizontalBoxSlot* FlyOutSlot = HostHBox->AddChildToHorizontalBox(FlyOutSizeBox))
	{
		FlyOutSlot->SetPadding(FMargin(0.f));
		FlyOutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	static constexpr float TabRowHeight = 18.f;
	const float TabStripH = static_cast<float>(UE_ARRAY_COUNT(TabDefs)) * TabRowHeight + 12.f;
	TabStripSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildPaletteTabStripSize"));
	TabStripSizeBox->SetWidthOverride(IH_BuildPalettePanelStyle::TabStripWidth);
	TabStripSizeBox->SetHeightOverride(TabStripH);
	TabStripBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BuildPaletteTabStripBorder"));
	IH_BuildPalettePanelStyle::ApplyRightTabStripBorderStyle(TabStripBorder);
	TabStripVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuildPaletteTabStripVBox"));
	TabKeyLabels.Reset();
	for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(TabDefs); ++TabIndex)
	{
		UTextBlock* KeyLabel = IH_P1C08_DevPanelStyle::MakeHUDLabel(
			WidgetTree,
			*FString::Printf(TEXT("TabKey_%d"), TabIndex),
			TabDefs[TabIndex].KeyLetter,
			FName(TEXT("HeadingText")));
		IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(KeyLabel, IH_P1C08_DevPanelStyle::CompactLabelFontSize);
		if (UVerticalBoxSlot* KeySlot = TabStripVBox->AddChildToVerticalBox(KeyLabel))
		{
			KeySlot->SetPadding(FMargin(10.f, 1.f, 0.f, 0.f));
		}
		TabKeyLabels.Add(KeyLabel);
	}
	TabStripBorder->AddChild(TabStripVBox);
	TabStripSizeBox->AddChild(TabStripBorder);
	if (UHorizontalBoxSlot* TabSlot = HostHBox->AddChildToHorizontalBox(TabStripSizeBox))
	{
		TabSlot->SetPadding(FMargin(0.f));
		TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	RootSizeBox->AddChild(HostHBox);

	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(RootSizeBox))
	{
		PanelSlot->SetZOrder(1);
	}
}

TSharedRef<SWidget> UIH_BuildPaletteHostWidget::RebuildWidget()

{

	EnsureWidgetTree();

	return Super::RebuildWidget();

}



void UIH_BuildPaletteHostWidget::NativeConstruct()

{

	Super::NativeConstruct();

	EnsureWidgetTree();

	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(1.f, 0.f)); // Plan Addendum 19

	bool bShouldShow = bTabStripVisible;

	if (const UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())

	{

		bShouldShow = Subsystem->HasTabStripEnabled();

		bTabStripVisible = bShouldShow;

		if (Subsystem->IsFlyOutOpen())

		{

			ActiveFlyOutTab = Subsystem->GetActiveTab();

		}

		else

		{

			ActiveFlyOutTab.Reset();

		}

	}

	// Paint-only overlay root; PlayerController routes pointer hits via HandleScreenPointerDown.
	SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	bLoggedPaintGeometryOnce = false;
	SyncHostLayout();

}



void UIH_BuildPaletteHostWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)

{

	Super::NativeTick(MyGeometry, InDeltaTime);

	if (const UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())
	{
		const TOptional<EIHBuildPaletteTab> ExpectedFlyOutTab = Subsystem->IsFlyOutOpen()
			? TOptional<EIHBuildPaletteTab>(Subsystem->GetActiveTab())
			: TOptional<EIHBuildPaletteTab>();
		if (ActiveFlyOutTab != ExpectedFlyOutTab)
		{
			ActiveFlyOutTab = ExpectedFlyOutTab;
			SyncHostLayout();
		}

		if (Subsystem->IsDragActive()
			&& Subsystem->GetDragPayload().paletteTab == EIHBuildPaletteTab::Build)
		{
			Invalidate(EInvalidateWidget::Paint);
		}
	}

}



FVector2D UIH_BuildPaletteHostWidget::ResolveViewportSize() const

{

	if (const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())

	{

		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())

		{

			if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)

			{

				if (const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget())

				{

					const FVector2D Size = ViewportWidget->GetTickSpaceGeometry().GetLocalSize();

					if (Size.X >= 1.f && Size.Y >= 1.f)

					{

						return Size;

					}

				}

			}

		}



		int32 ViewX = 0;

		int32 ViewY = 0;

		PC->GetViewportSize(ViewX, ViewY);

		if (ViewX > 0 && ViewY > 0)

		{

			return FVector2D(static_cast<float>(ViewX), static_cast<float>(ViewY));

		}

	}



	return FVector2D(1280.f, 720.f);

}



void UIH_BuildPaletteHostWidget::ResolveViewportMetrics(
	FVector2D& OutViewportSize, FVector2D& OutViewportAbs) const
{
	OutViewportSize = ResolveViewportSize();
	OutViewportAbs = FVector2D::ZeroVector;

	const FGeometry BaseGeometry = GetBaseGeometry();
	if (BaseGeometry.GetLocalSize().X >= 1.f && BaseGeometry.GetLocalSize().Y >= 1.f)
	{
		OutViewportSize = BaseGeometry.GetLocalSize();
		OutViewportAbs = BaseGeometry.GetAbsolutePosition();
	}
}

FGeometry UIH_BuildPaletteHostWidget::GetBaseGeometry() const
{
	if (const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)
			{
				if (const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget())
				{
					return ViewportWidget->GetTickSpaceGeometry();
				}
			}
		}
	}

	if (const TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		return Cached->GetTickSpaceGeometry();
	}

	return FGeometry();
}

FGeometry UIH_BuildPaletteHostWidget::ResolveBaseGeometry(const FGeometry& AllottedGeometry) const
{
	const FGeometry ViewportGeometry = GetBaseGeometry();
	if (ViewportGeometry.GetLocalSize().X >= 1.f && ViewportGeometry.GetLocalSize().Y >= 1.f)
	{
		return ViewportGeometry;
	}

	if (AllottedGeometry.GetLocalSize().X >= 1.f && AllottedGeometry.GetLocalSize().Y >= 1.f)
	{
		return AllottedGeometry;
	}

	return ViewportGeometry;
}

FVector2D UIH_BuildPaletteHostWidget::ResolveOverlaySize(const FGeometry& Geometry) const
{
	const FGeometry ViewportGeometry = GetBaseGeometry();
	const FVector2D ViewportSize = ViewportGeometry.GetLocalSize();
	if (ViewportSize.X >= 1.f && ViewportSize.Y >= 1.f)
	{
		return ViewportSize;
	}

	FVector2D Size = Geometry.GetLocalSize();
	if (Size.X >= 1.f && Size.Y >= 1.f)
	{
		return Size;
	}

	return ResolveViewportSize();
}

FGeometry UIH_BuildPaletteHostWidget::MakeOverlayGeometry(const FGeometry& AllottedGeometry) const
{
	const FGeometry BaseGeometry = ResolveBaseGeometry(AllottedGeometry);
	const FVector2D OverlaySize = ResolveOverlaySize(BaseGeometry);
	return BaseGeometry.MakeChild(
		FVector2f(OverlaySize),
		FSlateLayoutTransform(FVector2f::ZeroVector));
}

FVector2D UIH_BuildPaletteHostWidget::GetTabStripTopLeftLocal(const FGeometry& Geometry) const
{
	const FVector2D ViewSize = ResolveOverlaySize(Geometry);
	const float StripH = GetTabStripHeight();
	return FVector2D(
		FMath::Max(0.f, ViewSize.X - IH_BuildPalettePanelStyle::TabStripWidth - IH_BuildPalettePanelStyle::RightHUDInset),
		IH_BuildPalettePanelStyle::TabStripTopMargin);
}

FGeometry UIH_BuildPaletteHostWidget::MakeTabStripGeometry(const FGeometry& AllottedGeometry) const
{
	const FVector2D StripOrigin = GetTabStripTopLeftLocal(MakeOverlayGeometry(AllottedGeometry));
	const float StripH = GetTabStripHeight();
	return MakeOverlayGeometry(AllottedGeometry).MakeChild(
		FVector2f(IH_BuildPalettePanelStyle::TabStripWidth, StripH),
		FSlateLayoutTransform(FVector2f(StripOrigin)));
}

FGeometry UIH_BuildPaletteHostWidget::MakeFlyOutPaintGeometry(const FGeometry& AllottedGeometry) const
{
	const float FlyOutW = GetActiveFlyOutWidth();
	const float FlyOutH = ComputeHostPanelHeight();
	const FVector2D StripOrigin = GetTabStripTopLeftLocal(MakeOverlayGeometry(AllottedGeometry));
	const FVector2D FlyOutOrigin(
		FMath::Max(0.f, StripOrigin.X - FlyOutW - IH_BuildPalettePanelStyle::FlyOutTabStripGap),
		StripOrigin.Y);
	return MakeOverlayGeometry(AllottedGeometry).MakeChild(
		FVector2f(FlyOutW, FlyOutH),
		FSlateLayoutTransform(FVector2f(FlyOutOrigin)));
}

FGeometry UIH_BuildPaletteHostWidget::GetHitTestGeometry() const
{
	const FGeometry ViewportGeometry = GetBaseGeometry();
	if (ViewportGeometry.GetLocalSize().X >= 1.f && ViewportGeometry.GetLocalSize().Y >= 1.f)
	{
		return ViewportGeometry;
	}

	if (const TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		return ResolveBaseGeometry(Cached->GetTickSpaceGeometry());
	}

	return ViewportGeometry;
}



float UIH_BuildPaletteHostWidget::GetTabStripHeight() const
{
	static constexpr float TabRowHeight = 18.f;
	return static_cast<float>(UE_ARRAY_COUNT(TabDefs)) * TabRowHeight + 12.f;
}

float UIH_BuildPaletteHostWidget::ComputeHostPanelHeight() const

{

	static constexpr float TabRowHeight = 18.f;

	const float TabStripMinH = GetTabStripHeight();

	static constexpr float StubFlyOutMinH = 72.f;

	if (!ActiveFlyOutTab.IsSet())

	{

		return TabStripMinH;

	}

	switch (ActiveFlyOutTab.GetValue())

	{

	case EIHBuildPaletteTab::Grid:
	{
		const int32 RowCount = FMath::Max(CachedGridRows.Num(), 1);
		const float TemplateListH = static_cast<float>(RowCount) * IH_BuildPalettePanelStyle::GridTemplateRowHeight
			+ static_cast<float>(FMath::Max(RowCount - 1, 0)) * IH_BuildPalettePanelStyle::GridTemplateRowGap;
		const float FlyOutContentH = IH_BuildPalettePanelStyle::GridFlyOutHeaderBlockH
			+ 8.f + TemplateListH + 8.f + IH_BuildPalettePanelStyle::GridFlyOutStubSectionsH;
		return FMath::Max(TabStripMinH, FlyOutContentH);
	}
	case EIHBuildPaletteTab::Build:
	{
		const int32 RowCount = FMath::Max(CachedBuildRows.Num(), 1);
		const float TemplateListH = static_cast<float>(RowCount) * IH_BuildPalettePanelStyle::GridTemplateRowHeight
			+ static_cast<float>(FMath::Max(RowCount - 1, 0)) * IH_BuildPalettePanelStyle::GridTemplateRowGap;
		const float FlyOutContentH = IH_BuildPalettePanelStyle::GridFlyOutHeaderBlockH + 8.f + TemplateListH + 8.f;
		return FMath::Max(TabStripMinH, FlyOutContentH);
	}
	case EIHBuildPaletteTab::World:
	{
		const float Tile = IH_BuildPalettePanelStyle::WorldStampTileSize;
		const float Gap = IH_BuildPalettePanelStyle::WorldStampTileGap;
		const float GridH = static_cast<float>(IH_BuildPalettePanelStyle::WorldStampActiveRows) * (Tile + Gap) - Gap;
		const float SpecialBlockH = IH_BuildPalettePanelStyle::WorldStampSectionHeaderH + Tile
			+ IH_BuildPalettePanelStyle::WorldStampSpecialGap;
		const float ReservedBlockH = IH_BuildPalettePanelStyle::WorldStampSectionHeaderH + Tile
			+ IH_BuildPalettePanelStyle::WorldStampReservedGap;
		const float FlyOutContentH = IH_BuildPalettePanelStyle::GridFlyOutHeaderBlockH + GridH
			+ SpecialBlockH + ReservedBlockH + 16.f;
		return FMath::Max(TabStripMinH, FlyOutContentH);
	}
	default:

		return FMath::Max(TabStripMinH, StubFlyOutMinH);

	}

}



float UIH_BuildPaletteHostWidget::GetActiveFlyOutWidth() const
{
	if (ActiveFlyOutTab.IsSet() && ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::World)
	{
		return IH_BuildPalettePanelStyle::WorldFlyOutWidth;
	}
	return IH_BuildPalettePanelStyle::FlyOutWidth;
}



void UIH_BuildPaletteHostWidget::RequestLayoutRefresh()

{

	EnsureWidgetTree();

	SyncHostLayout();

	Invalidate(EInvalidateWidget::LayoutAndVolatility | EInvalidateWidget::Paint);

	if (const TSharedPtr<SWidget> SlateWidget = GetCachedWidget())

	{

		SlateWidget->Invalidate(EInvalidateWidget::LayoutAndVolatility | EInvalidateWidget::Paint);

	}

}



void UIH_BuildPaletteHostWidget::EnsureWidgetTreeBuilt()

{

	EnsureWidgetTree();

	if (!WidgetTree || !WidgetTree->RootWidget)

	{

		UE_LOG(LogTemp, Warning, TEXT("BuildPaletteHost: widget tree missing after EnsureWidgetTree"));

	}

}



void UIH_BuildPaletteHostWidget::LogLayoutDiagnostics(const TCHAR* Context) const

{

	FVector2D RootSize = FVector2D::ZeroVector;

	if (RootSizeBox)

	{

		RootSize = RootSizeBox->GetCachedGeometry().GetLocalSize();

	}

	const bool bFlyOutOpen = ActiveFlyOutTab.IsSet();

	const float TabStripW = IH_BuildPalettePanelStyle::TabStripWidth;
	const float FlyOutW = bFlyOutOpen ? GetActiveFlyOutWidth() : 0.f;
	const float PanelW = bFlyOutOpen ? FlyOutW : IH_BuildPalettePanelStyle::TabStripWidth;
	FVector2D ViewportSize = ResolveViewportSize();
	const float TabStripLeftX = ResolveTabStripLeftViewportX();
	const float TargetLocalX = bFlyOutOpen
		? FMath::Max(0.f, TabStripLeftX - FlyOutW - IH_BuildPalettePanelStyle::FlyOutTabStripGap)
		: TabStripLeftX;
	const float TargetLocalY = IH_BuildPalettePanelStyle::TabStripTopMargin;
	FVector2D ViewportAbs = FVector2D::ZeroVector;
	ResolveViewportMetrics(ViewportSize, ViewportAbs);
	FVector2D PanelAbsPos = FVector2D::ZeroVector;
	if (RootSizeBox)
	{
		PanelAbsPos = RootSizeBox->GetCachedGeometry().GetAbsolutePosition();
	}

	UE_LOG(

		LogIH_WB_Demo004, Log,

		TEXT("BuildPaletteHostWidget[%s]: layout=v17-viewportOverlay visibility=%s inViewport=%d viewport=(%.0f,%.0f) viewportAbs=(%.0f,%.0f) targetLocal=(%.0f,%.0f) panelAbs=(%.0f,%.0f) panel=(%.0f,%.0f) flyOutOpen=%d gridRows=%d tabStripVisible=%d"),

		Context,

		*UEnum::GetValueAsString(GetVisibility()),

		IsInViewport() ? 1 : 0,

		ViewportSize.X,

		ViewportSize.Y,

		ViewportAbs.X,

		ViewportAbs.Y,

		TargetLocalX,

		TargetLocalY,

		PanelAbsPos.X,

		PanelAbsPos.Y,

		PanelW,

		RootSize.Y,

		bFlyOutOpen ? 1 : 0,

		CachedGridRows.Num(),

		bTabStripVisible ? 1 : 0);

}



void UIH_BuildPaletteHostWidget::SyncHostLayout()

{

	EnsureWidgetTree();

	if (!RootCanvas || !RootSizeBox || !FlyOutSizeBox || !TabStripSizeBox || !HostHBox)

	{

		return;

	}

	const bool bFlyOutOpen = ActiveFlyOutTab.IsSet();

	const float TabStripW = IH_BuildPalettePanelStyle::TabStripWidth;
	const float FlyOutW = bFlyOutOpen ? GetActiveFlyOutWidth() : 0.f;
	const float PanelH = bFlyOutOpen ? FMath::Max(ComputeHostPanelHeight(), 1.f) : 0.f;
	const float PanelW = bFlyOutOpen ? FlyOutW : 0.f;

	const FVector2D ViewportSize = ResolveOverlaySize(GetHitTestGeometry());
	const float TabStripLeftX = FMath::Max(
		0.f, ViewportSize.X - TabStripW - IH_BuildPalettePanelStyle::RightHUDInset);
	const float PanelLeftX = bFlyOutOpen
		? FMath::Max(0.f, TabStripLeftX - FlyOutW - IH_BuildPalettePanelStyle::FlyOutTabStripGap)
		: TabStripLeftX;

	if (RootSizeBox)
	{
		RootSizeBox->SetVisibility(bFlyOutOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bFlyOutOpen)
		{
			RootSizeBox->SetWidthOverride(FlyOutW);
			RootSizeBox->SetHeightOverride(PanelH);
		}
	}

	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(RootSizeBox->Slot))
	{
		if (bFlyOutOpen)
		{
			IH_BuildPalettePanelStyle::ApplyTopRightPanelCanvasSlot(
				PanelSlot,
				PanelLeftX,
				IH_BuildPalettePanelStyle::TabStripTopMargin,
				PanelW,
				PanelH);
		}
	}

	if (RootSizeBox)
	{
		RootSizeBox->SetRenderTranslation(FVector2D::ZeroVector);
	}

	if (FlyOutSizeBox)
	{
		FlyOutSizeBox->SetWidthOverride(bFlyOutOpen ? GetActiveFlyOutWidth() : 0.f);
		FlyOutSizeBox->SetHeightOverride(bFlyOutOpen ? PanelH : 0.f);
		FlyOutSizeBox->SetVisibility(bFlyOutOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* FlyOutSlot = Cast<UHorizontalBoxSlot>(FlyOutSizeBox->Slot))
		{
			FlyOutSlot->SetPadding(FMargin(
				0.f, 0.f, bFlyOutOpen ? IH_BuildPalettePanelStyle::FlyOutTabStripGap : 0.f, 0.f));
		}
	}
	if (FlyOutBorder)
	{
		FlyOutBorder->SetVisibility(bFlyOutOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		IH_BuildPalettePanelStyle::ApplyRightFlyOutBorderStyle(FlyOutBorder, bFlyOutOpen);
	}
	if (TabStripSizeBox)
	{
		TabStripSizeBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TabStripBorder)
	{
		TabStripBorder->SetVisibility(ESlateVisibility::Collapsed);
	}

	SyncFlyOutContentVisibility();

	if (bTabStripVisible)
	{
		Invalidate(EInvalidateWidget::Paint);
	}

	SetVisibility(bTabStripVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}



float UIH_BuildPaletteHostWidget::ResolveTabStripLeftViewportX() const
{
	const FVector2D ViewportSize = ResolveOverlaySize(GetHitTestGeometry());
	return FMath::Max(
		0.f, ViewportSize.X - IH_BuildPalettePanelStyle::TabStripWidth - IH_BuildPalettePanelStyle::RightHUDInset);
}

bool UIH_BuildPaletteHostWidget::TryGetTabStripScreenRect(FSlateRect& OutRect) const
{
	if (!bTabStripVisible)
	{
		return false;
	}

	if (bHasCachedTabStripScreenRect)
	{
		OutRect = CachedTabStripScreenRect;
		return true;
	}

	const FGeometry Base = GetHitTestGeometry();
	const float StripW = IH_BuildPalettePanelStyle::TabStripWidth;
	const float StripH = GetTabStripHeight();
	if (StripW < 1.f || StripH < 1.f)
	{
		return false;
	}

	const FGeometry StripGeometry = MakeTabStripGeometry(Base);
	const FVector2D TopLeft = StripGeometry.GetAbsolutePosition();
	OutRect = FSlateRect(TopLeft.X, TopLeft.Y, TopLeft.X + StripW, TopLeft.Y + StripH);
	return true;
}

void UIH_BuildPaletteHostWidget::DrawSolidLocalRect(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& Geometry,
	const FSlateRect& LocalRect,
	const FLinearColor& Color) const
{
	const FVector2f Size(LocalRect.Right - LocalRect.Left, LocalRect.Bottom - LocalRect.Top);
	if (Size.X < 1.f || Size.Y < 1.f)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(LocalRect.Left, LocalRect.Top))),
		WhiteBrush,
		ESlateDrawEffect::None,
		Color);
}

namespace IH_BuildPaletteHostWidgetPrivate
{
static float GetFlyOutTemplateRowTop(int32 RowIndex)
{
	return IH_BuildPalettePanelStyle::GridFlyOutRowStartY
		+ static_cast<float>(RowIndex)
			* (IH_BuildPalettePanelStyle::GridTemplateRowHeight
				+ IH_BuildPalettePanelStyle::GridTemplateRowGap);
}

static FSlateRect MakeTemplateIconTileLocalRect(float RowTop, float TileTopOffsetY)
{
	const float IconX = IH_BuildPalettePanelStyle::GridFlyOutContentInsetX;
	const float IconSize = IH_BuildPalettePanelStyle::GridTemplateTileSize;
	const float TileTop = RowTop + TileTopOffsetY;
	return FSlateRect(IconX, TileTop, IconX + IconSize, TileTop + IconSize);
}

/** Full fly-out row band for pointer hit tests (icon + label text). */
static FSlateRect MakeFullTemplateRowLocalRect(float RowTop)
{
	return FSlateRect(
		0.f,
		RowTop,
		IH_BuildPalettePanelStyle::FlyOutWidth,
		RowTop + IH_BuildPalettePanelStyle::GridTemplateRowHeight);
}
} // namespace IH_BuildPaletteHostWidgetPrivate

bool UIH_BuildPaletteHostWidget::TryGetGridTemplateRowLocalRect(int32 RowIndex, FSlateRect& OutLocalRect) const
{
	if (!CachedGridRows.IsValidIndex(RowIndex))
	{
		return false;
	}

	const float RowTop = IH_BuildPaletteHostWidgetPrivate::GetFlyOutTemplateRowTop(RowIndex);
	OutLocalRect = IH_BuildPaletteHostWidgetPrivate::MakeFullTemplateRowLocalRect(RowTop);
	return true;
}

bool UIH_BuildPaletteHostWidget::TryGetBuildTemplateRowLocalRect(int32 RowIndex, FSlateRect& OutLocalRect) const
{
	if (!CachedBuildRows.IsValidIndex(RowIndex))
	{
		return false;
	}

	const float RowTop = IH_BuildPaletteHostWidgetPrivate::GetFlyOutTemplateRowTop(RowIndex);
	OutLocalRect = IH_BuildPaletteHostWidgetPrivate::MakeFullTemplateRowLocalRect(RowTop);
	return true;
}

int32 UIH_BuildPaletteHostWidget::PaintGridFlyOutContent(
	const FGeometry& FlyOutGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	if (!ActiveFlyOutTab.IsSet() || ActiveFlyOutTab.GetValue() != EIHBuildPaletteTab::Grid)
	{
		return LayerId;
	}

	const float FlyOutW = IH_BuildPalettePanelStyle::FlyOutWidth;
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle(
		"Bold", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle(
		"Regular", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	const FLinearColor HeaderColor = IH_BuildPalettePanelStyle::FocusBlue;
	const FLinearColor RowColor = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")));

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		FlyOutGeometry.ToPaintGeometry(
			FVector2f(FlyOutW - IH_BuildPalettePanelStyle::GridFlyOutContentInsetX * 2.f, 18.f),
			FSlateLayoutTransform(FVector2f(
				IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
				IH_BuildPalettePanelStyle::GridFlyOutHeaderY))),
		TEXT("Town Grids"),
		HeaderFont,
		ESlateDrawEffect::None,
		HeaderColor);

	int32 CurrentLayer = LayerId + 1;
	for (int32 RowIndex = 0; RowIndex < CachedGridRows.Num(); ++RowIndex)
	{
		const FIHBuildPaletteItemRow& Row = CachedGridRows[RowIndex];
		const FString Label = Row.displayName.IsEmpty() ? Row.itemID.ToString() : Row.displayName;
		const float RowTop = IH_BuildPalettePanelStyle::GridFlyOutRowStartY
			+ static_cast<float>(RowIndex)
				* (IH_BuildPalettePanelStyle::GridTemplateRowHeight + IH_BuildPalettePanelStyle::GridTemplateRowGap);
		const float IconX = IH_BuildPalettePanelStyle::GridFlyOutContentInsetX;
		const float IconSize = IH_BuildPalettePanelStyle::GridTemplateTileSize;

		if (UTexture2D* IconTexture = Row.icon.Get())
		{
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(IconTexture);
			IconBrush.ImageSize = FVector2D(IconSize, IconSize);
			IconBrush.DrawAs = ESlateBrushDrawType::Image;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				CurrentLayer,
				FlyOutGeometry.ToPaintGeometry(
					FVector2f(IconSize, IconSize),
					FSlateLayoutTransform(FVector2f(IconX, RowTop))),
				&IconBrush,
				ESlateDrawEffect::None,
				FLinearColor::White);
		}

		FSlateDrawElement::MakeText(
			OutDrawElements,
			CurrentLayer,
			FlyOutGeometry.ToPaintGeometry(
				FVector2f(
					FlyOutW - IconX - IconSize - IH_BuildPalettePanelStyle::GridFlyOutIconLabelGap
						- IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
					IconSize),
				FSlateLayoutTransform(FVector2f(
					IconX + IconSize + IH_BuildPalettePanelStyle::GridFlyOutIconLabelGap,
					RowTop + 14.f))),
			Label,
			RowFont,
			ESlateDrawEffect::None,
			RowColor);
		++CurrentLayer;
	}

	return CurrentLayer;
}

int32 UIH_BuildPaletteHostWidget::PaintBuildFlyOutContent(
	const FGeometry& FlyOutGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	if (!ActiveFlyOutTab.IsSet() || ActiveFlyOutTab.GetValue() != EIHBuildPaletteTab::Build)
	{
		return LayerId;
	}

	const float FlyOutW = IH_BuildPalettePanelStyle::FlyOutWidth;
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle(
		"Bold", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle(
		"Regular", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	const FLinearColor HeaderColor = IH_BuildPalettePanelStyle::FocusBlue;
	const FLinearColor RowColor = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")));

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		FlyOutGeometry.ToPaintGeometry(
			FVector2f(FlyOutW - IH_BuildPalettePanelStyle::GridFlyOutContentInsetX * 2.f, 18.f),
			FSlateLayoutTransform(FVector2f(
				IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
				IH_BuildPalettePanelStyle::GridFlyOutHeaderY))),
		TEXT("Dev Structures"),
		HeaderFont,
		ESlateDrawEffect::None,
		HeaderColor);

	int32 CurrentLayer = LayerId + 1;
	for (int32 RowIndex = 0; RowIndex < CachedBuildRows.Num(); ++RowIndex)
	{
		const FIHBuildPaletteItemRow& Row = CachedBuildRows[RowIndex];
		const FString Label = Row.displayName.IsEmpty() ? Row.itemID.ToString() : Row.displayName;
		const float RowTop = IH_BuildPalettePanelStyle::GridFlyOutRowStartY
			+ static_cast<float>(RowIndex)
				* (IH_BuildPalettePanelStyle::GridTemplateRowHeight + IH_BuildPalettePanelStyle::GridTemplateRowGap);
		const float IconX = IH_BuildPalettePanelStyle::GridFlyOutContentInsetX;
		const float IconSize = IH_BuildPalettePanelStyle::GridTemplateTileSize;
		const FString ZoneGlyph = StaticEnum<EIHParcelZoneCode>()
			? StaticEnum<EIHParcelZoneCode>()->GetNameStringByValue(static_cast<int64>(Row.zoneRequired))
			: FString();
		const FString IconLabel = ZoneGlyph.IsEmpty() ? Label : FString::Printf(TEXT("[%s]"), *ZoneGlyph);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			CurrentLayer,
			FlyOutGeometry.ToPaintGeometry(
				FVector2f(IconSize, IconSize),
				FSlateLayoutTransform(FVector2f(IconX, RowTop + 8.f))),
			IconLabel,
			RowFont,
			ESlateDrawEffect::None,
			HeaderColor);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			CurrentLayer,
			FlyOutGeometry.ToPaintGeometry(
				FVector2f(
					FlyOutW - IconX - IconSize - IH_BuildPalettePanelStyle::GridFlyOutIconLabelGap
						- IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
					IconSize),
				FSlateLayoutTransform(FVector2f(
					IconX + IconSize + IH_BuildPalettePanelStyle::GridFlyOutIconLabelGap,
					RowTop + 14.f))),
			Label,
			RowFont,
			ESlateDrawEffect::None,
			RowColor);
		++CurrentLayer;
	}

	return CurrentLayer;
}

bool UIH_BuildPaletteHostWidget::TryGetWorldStampSlotLocalRect(
	const int32 SlotIndex,
	FSlateRect& OutLocalRect) const
{
	if (!CachedWorldStampSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	const float InsetX = IH_BuildPalettePanelStyle::GridFlyOutContentInsetX;
	const float Tile = IH_BuildPalettePanelStyle::WorldStampTileSize;
	const float Gap = IH_BuildPalettePanelStyle::WorldStampTileGap;
	const float ColStride = Tile + Gap;
	const int32 MainSlotCount = IHInvisibleHandSpec::TerrainStampPaletteMainSlotCount;

	if (SlotIndex < MainSlotCount)
	{
		const int32 Col = SlotIndex % IH_BuildPalettePanelStyle::WorldStampGridColumns;
		const int32 Row = SlotIndex / IH_BuildPalettePanelStyle::WorldStampGridColumns;
		const float Left = InsetX + static_cast<float>(Col) * ColStride;
		const float Top = IH_BuildPalettePanelStyle::WorldStampGridStartY + static_cast<float>(Row) * (Tile + Gap);
		OutLocalRect = FSlateRect(Left, Top, Left + Tile, Top + Tile);
		return true;
	}

	if (SlotIndex == MainSlotCount)
	{
		const float GridH = static_cast<float>(IH_BuildPalettePanelStyle::WorldStampActiveRows) * (Tile + Gap) - Gap;
		const float Top = IH_BuildPalettePanelStyle::WorldStampGridStartY + GridH
			+ IH_BuildPalettePanelStyle::WorldStampSpecialGap
			+ IH_BuildPalettePanelStyle::WorldStampSectionHeaderH;
		OutLocalRect = FSlateRect(InsetX, Top, InsetX + Tile, Top + Tile);
		return true;
	}

	const int32 ReservedIndex = SlotIndex - MainSlotCount - 1;
	if (ReservedIndex >= 0 && ReservedIndex < IHInvisibleHandSpec::TerrainStampPaletteReservedSlotCount)
	{
		const float GridH = static_cast<float>(IH_BuildPalettePanelStyle::WorldStampActiveRows) * (Tile + Gap) - Gap;
		const float SpecialBlockH = IH_BuildPalettePanelStyle::WorldStampSpecialGap
			+ IH_BuildPalettePanelStyle::WorldStampSectionHeaderH + Tile;
		const float Top = IH_BuildPalettePanelStyle::WorldStampGridStartY + GridH + SpecialBlockH
			+ IH_BuildPalettePanelStyle::WorldStampReservedGap
			+ IH_BuildPalettePanelStyle::WorldStampSectionHeaderH;
		const float Left = InsetX + static_cast<float>(ReservedIndex) * ColStride;
		OutLocalRect = FSlateRect(Left, Top, Left + Tile, Top + Tile);
		return true;
	}

	return false;
}

int32 UIH_BuildPaletteHostWidget::PaintWorldStampSlot(
	const FGeometry& FlyOutGeometry,
	const FWorldStampPaletteSlot& StampSlot,
	const FSlateRect& LocalRect,
	const bool bHovered,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId) const
{
	const float W = LocalRect.Right - LocalRect.Left;
	const float H = LocalRect.Bottom - LocalRect.Top;
	const FLinearColor Fill = StampSlot.bReserved
		? FLinearColor(0.12f, 0.13f, 0.15f, 0.45f)
		: (bHovered
			? FLinearColor(0.22f, 0.38f, 0.28f, 0.95f)
			: FLinearColor(0.16f, 0.28f, 0.20f, 0.92f));
	DrawSolidLocalRect(
		OutDrawElements,
		LayerId,
		FlyOutGeometry,
		LocalRect,
		Fill);

	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(
		"Regular", IH_P1C08_DevPanelStyle::CompactLabelFontSize - 1);
	const FLinearColor LabelColor = StampSlot.bReserved
		? IH_BuildPalettePanelStyle::DisabledTabText
		: FLinearColor::White;
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		FlyOutGeometry.ToPaintGeometry(
			FVector2f(W, H),
			FSlateLayoutTransform(FVector2f(LocalRect.Left + 2.f, LocalRect.Top + 14.f))),
		StampSlot.bReserved ? TEXT("+") : StampSlot.ShortLabel,
		LabelFont,
		ESlateDrawEffect::None,
		LabelColor);
	return LayerId;
}

int32 UIH_BuildPaletteHostWidget::PaintWorldFlyOutContent(
	const FGeometry& FlyOutGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId) const
{
	if (!ActiveFlyOutTab.IsSet() || ActiveFlyOutTab.GetValue() != EIHBuildPaletteTab::World)
	{
		return LayerId;
	}

	const float FlyOutW = IH_BuildPalettePanelStyle::WorldFlyOutWidth;
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle(
		"Bold", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	const FSlateFontInfo SubHeaderFont = FCoreStyle::GetDefaultFontStyle(
		"Regular", IH_P1C08_DevPanelStyle::CompactLabelFontSize - 1);
	const FLinearColor HeaderColor = IH_BuildPalettePanelStyle::FocusBlue;
	const FLinearColor SubHeaderColor = IH_BuildPalettePanelStyle::DisabledTabText;

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		FlyOutGeometry.ToPaintGeometry(
			FVector2f(FlyOutW - IH_BuildPalettePanelStyle::GridFlyOutContentInsetX * 2.f, 18.f),
			FSlateLayoutTransform(FVector2f(
				IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
				IH_BuildPalettePanelStyle::GridFlyOutHeaderY))),
		TEXT("Terrain stamps"),
		HeaderFont,
		ESlateDrawEffect::None,
		HeaderColor);

	const float Tile = IH_BuildPalettePanelStyle::WorldStampTileSize;
	const float Gap = IH_BuildPalettePanelStyle::WorldStampTileGap;
	const float GridH = static_cast<float>(IH_BuildPalettePanelStyle::WorldStampActiveRows) * (Tile + Gap) - Gap;
	const float SpecialHeaderY = IH_BuildPalettePanelStyle::WorldStampGridStartY + GridH
		+ IH_BuildPalettePanelStyle::WorldStampSpecialGap;
	const float ReservedHeaderY = SpecialHeaderY + IH_BuildPalettePanelStyle::WorldStampSectionHeaderH + Tile
		+ IH_BuildPalettePanelStyle::WorldStampReservedGap;

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		FlyOutGeometry.ToPaintGeometry(
			FVector2f(FlyOutW, 14.f),
			FSlateLayoutTransform(FVector2f(
				IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
				SpecialHeaderY))),
		TEXT("Special"),
		SubHeaderFont,
		ESlateDrawEffect::None,
		SubHeaderColor);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		FlyOutGeometry.ToPaintGeometry(
			FVector2f(FlyOutW, 14.f),
			FSlateLayoutTransform(FVector2f(
				IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
				ReservedHeaderY))),
		TEXT("Reserved (mods)"),
		SubHeaderFont,
		ESlateDrawEffect::None,
		SubHeaderColor);

	int32 CurrentLayer = LayerId + 1;
	for (int32 SlotIndex = 0; SlotIndex < CachedWorldStampSlots.Num(); ++SlotIndex)
	{
		FSlateRect SlotRect;
		if (!TryGetWorldStampSlotLocalRect(SlotIndex, SlotRect))
		{
			continue;
		}
		CurrentLayer = PaintWorldStampSlot(
			FlyOutGeometry,
			CachedWorldStampSlots[SlotIndex],
			SlotRect,
			SlotIndex == HoveredWorldStampIndex,
			OutDrawElements,
			CurrentLayer);
		++CurrentLayer;
	}

	return CurrentLayer;
}

int32 UIH_BuildPaletteHostWidget::PaintFlyOutRowHoverOutline(
	const FGeometry& AllottedGeometry,
	const FGeometry& FlyOutGeometry,
	const FSlateRect& RowLocal,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	static constexpr float OutlinePadding = 2.f;
	const FVector2D AbsMin = FlyOutGeometry.LocalToAbsolute(FVector2D(RowLocal.Left, RowLocal.Top));
	const FVector2D AbsMax = FlyOutGeometry.LocalToAbsolute(FVector2D(RowLocal.Right, RowLocal.Bottom));
	const FVector2D OutlineOffset(
		IH_BuildPalettePanelStyle::FocusOutlineOffsetX,
		IH_BuildPalettePanelStyle::FocusOutlineOffsetY);
	const FVector2D LocalMin = AllottedGeometry.AbsoluteToLocal(AbsMin)
		- FVector2D(OutlinePadding, OutlinePadding) + OutlineOffset;
	const FVector2D LocalMax = AllottedGeometry.AbsoluteToLocal(AbsMax)
		+ FVector2D(OutlinePadding, OutlinePadding) + OutlineOffset;
	const FVector2f BoxSize(LocalMax.X - LocalMin.X, LocalMax.Y - LocalMin.Y);
	if (BoxSize.X < 1.f || BoxSize.Y < 1.f)
	{
		return LayerId;
	}

	static const FSlateRoundedBoxBrush RowOutlineBrush(
		FLinearColor::Transparent,
		FVector4(3.f, 3.f, 3.f, 3.f),
		IH_BuildPalettePanelStyle::FocusBlue,
		IH_P1C08_DevPanelStyle::RowSelectionOutlineThickness);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(FVector2f(LocalMin))),
		&RowOutlineBrush,
		ESlateDrawEffect::None,
		FLinearColor::Transparent);

	return LayerId + 1;
}

int32 UIH_BuildPaletteHostWidget::PaintGridTemplateHoverOutline(
	const FGeometry& AllottedGeometry,
	const FGeometry& FlyOutGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	if (!CachedGridRows.IsValidIndex(HoveredTemplateIndex))
	{
		return LayerId;
	}

	FSlateRect RowLocal;
	if (!TryGetGridTemplateRowLocalRect(HoveredTemplateIndex, RowLocal))
	{
		return LayerId;
	}

	return PaintFlyOutRowHoverOutline(AllottedGeometry, FlyOutGeometry, RowLocal, OutDrawElements, LayerId);
}

int32 UIH_BuildPaletteHostWidget::PaintGridTemplateHoverTooltip(
	const FGeometry& FlyOutGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	if (!CachedGridRows.IsValidIndex(HoveredTemplateIndex))
	{
		return LayerId;
	}

	const FString& Tooltip = CachedGridRows[HoveredTemplateIndex].tooltip;
	if (Tooltip.IsEmpty())
	{
		return LayerId;
	}

	FSlateRect RowLocal;
	if (!TryGetGridTemplateRowLocalRect(HoveredTemplateIndex, RowLocal))
	{
		return LayerId;
	}

	const FSlateFontInfo TooltipFont = FCoreStyle::GetDefaultFontStyle(
		"Regular", IH_P1C08_DevPanelStyle::CompactLabelFontSize - 1);
	const FLinearColor TooltipColor = IH_BuildPalettePanelStyle::FocusBlue;
	const float TooltipY = RowLocal.Bottom + 2.f;

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 1,
		FlyOutGeometry.ToPaintGeometry(
			FVector2f(
				IH_BuildPalettePanelStyle::FlyOutWidth - IH_BuildPalettePanelStyle::GridFlyOutContentInsetX * 2.f,
				18.f),
			FSlateLayoutTransform(FVector2f(
				IH_BuildPalettePanelStyle::GridFlyOutContentInsetX,
				TooltipY))),
		*Tooltip,
		TooltipFont,
		ESlateDrawEffect::None,
		TooltipColor);

	return LayerId + 1;
}

int32 UIH_BuildPaletteHostWidget::HitTestTabIndexInStripRect(
	const FSlateRect& StripRect,
	const FVector2D& ScreenAbsolute) const
{
	static constexpr float TabRowHeight = 18.f;
	static constexpr float TabTopPadding = 6.f;

	const float LocalY = ScreenAbsolute.Y - StripRect.Top;
	if (LocalY < TabTopPadding || LocalY > StripRect.Bottom - StripRect.Top - TabTopPadding)
	{
		return INDEX_NONE;
	}

	const int32 TabIndex = FMath::Clamp(
		FMath::FloorToInt((LocalY - TabTopPadding) / TabRowHeight),
		0,
		static_cast<int32>(UE_ARRAY_COUNT(TabDefs)) - 1);
	const float RowTop = TabTopPadding + static_cast<float>(TabIndex) * TabRowHeight;
	const float RowBottom = RowTop + TabRowHeight;
	if (LocalY >= RowTop && LocalY < RowBottom)
	{
		return TabIndex;
	}

	return INDEX_NONE;
}



bool UIH_BuildPaletteHostWidget::TryGetFlyOutScreenRect(FSlateRect& OutRect) const
{
	if (!ActiveFlyOutTab.IsSet() || GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}

	const float FlyOutW = GetActiveFlyOutWidth();
	const float PanelH = ComputeHostPanelHeight();
	if (FlyOutW < 1.f || PanelH < 1.f)
	{
		return false;
	}

	// Match NativePaint / MakeFlyOutPaintGeometry — UMG FlyOutSizeBox canvas slot can diverge in PIE.
	const FGeometry Base = GetHitTestGeometry();
	const FGeometry FlyOutGeometry = MakeFlyOutPaintGeometry(Base);
	const FVector2D TopLeft = FlyOutGeometry.GetAbsolutePosition();
	OutRect = FSlateRect(TopLeft.X, TopLeft.Y, TopLeft.X + FlyOutW, TopLeft.Y + PanelH);
	return true;
}



bool UIH_BuildPaletteHostWidget::IsPointInsideWidget(const UWidget* Widget, const FVector2D& ScreenAbsolute) const

{

	if (!Widget || !Widget->IsVisible())

	{

		return false;

	}



	const FGeometry& Geo = Widget->GetCachedGeometry();

	if (Geo.GetLocalSize().IsNearlyZero())

	{

		return false;

	}



	const FVector2D Local = Geo.AbsoluteToLocal(ScreenAbsolute);

	const FVector2D Size = Geo.GetLocalSize();

	return Local.X >= 0.f && Local.Y >= 0.f && Local.X <= Size.X && Local.Y <= Size.Y;

}



bool UIH_BuildPaletteHostWidget::IsScreenPointOverTabStrip(const FVector2D& ScreenAbsolute) const
{
	if (!bTabStripVisible)
	{
		return false;
	}

	FSlateRect StripRect;
	return TryGetTabStripScreenRect(StripRect) && StripRect.ContainsPoint(ScreenAbsolute);
}

int32 UIH_BuildPaletteHostWidget::HitTestTabIndex(const FVector2D& ScreenAbsolute) const
{
	FSlateRect StripRect;
	if (!TryGetTabStripScreenRect(StripRect) || !StripRect.ContainsPoint(ScreenAbsolute))
	{
		return INDEX_NONE;
	}

	return HitTestTabIndexInStripRect(StripRect, ScreenAbsolute);
}

void UIH_BuildPaletteHostWidget::SyncTabStripVisuals()
{
	const int32 ActiveTabIndex = ActiveFlyOutTab.IsSet()
		? EnumToTabIndex(ActiveFlyOutTab.GetValue())
		: INDEX_NONE;
	for (int32 TabIndex = 0; TabIndex < TabKeyLabels.Num(); ++TabIndex)
	{
		if (UTextBlock* Label = TabKeyLabels[TabIndex])
		{
			const FLinearColor TextColor = TabIndex == ActiveTabIndex
				? IH_BuildPalettePanelStyle::FocusBlue
				: UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")));
			Label->SetColorAndOpacity(FSlateColor(TextColor));
		}
	}
}

bool UIH_BuildPaletteHostWidget::IsScreenPointOverBuildPalette(const FVector2D& ScreenAbsolute) const

{

	if (IsScreenPointOverTabStrip(ScreenAbsolute))
	{
		return true;
	}

	if (GetVisibility() == ESlateVisibility::Collapsed || !ActiveFlyOutTab.IsSet())
	{
		return false;
	}

	FSlateRect FlyOutRect;
	return TryGetFlyOutScreenRect(FlyOutRect) && FlyOutRect.ContainsPoint(ScreenAbsolute);

}



int32 UIH_BuildPaletteHostWidget::HitTestGridTemplateTile(const FVector2D& ScreenAbsolute) const
{
	if (!ActiveFlyOutTab.IsSet() || ActiveFlyOutTab.GetValue() != EIHBuildPaletteTab::Grid)
	{
		return INDEX_NONE;
	}

	FSlateRect FlyOutRect;
	if (!TryGetFlyOutScreenRect(FlyOutRect))
	{
		return INDEX_NONE;
	}

	const FVector2D Local(ScreenAbsolute.X - FlyOutRect.Left, ScreenAbsolute.Y - FlyOutRect.Top);
	for (int32 RowIndex = 0; RowIndex < CachedGridRows.Num(); ++RowIndex)
	{
		FSlateRect RowLocal;
		if (!TryGetGridTemplateRowLocalRect(RowIndex, RowLocal))
		{
			continue;
		}

		if (Local.X >= RowLocal.Left && Local.X <= RowLocal.Right
			&& Local.Y >= RowLocal.Top && Local.Y <= RowLocal.Bottom)
		{
			return RowIndex;
		}
	}

	return INDEX_NONE;
}

int32 UIH_BuildPaletteHostWidget::HitTestBuildTemplateTile(const FVector2D& ScreenAbsolute) const
{
	if (!ActiveFlyOutTab.IsSet() || ActiveFlyOutTab.GetValue() != EIHBuildPaletteTab::Build)
	{
		return INDEX_NONE;
	}

	FSlateRect FlyOutRect;
	if (!TryGetFlyOutScreenRect(FlyOutRect))
	{
		return INDEX_NONE;
	}

	const FVector2D Local(ScreenAbsolute.X - FlyOutRect.Left, ScreenAbsolute.Y - FlyOutRect.Top);
	for (int32 RowIndex = 0; RowIndex < CachedBuildRows.Num(); ++RowIndex)
	{
		FSlateRect RowLocal;
		if (!TryGetBuildTemplateRowLocalRect(RowIndex, RowLocal))
		{
			continue;
		}

		if (Local.X >= RowLocal.Left && Local.X <= RowLocal.Right
			&& Local.Y >= RowLocal.Top && Local.Y <= RowLocal.Bottom)
		{
			return RowIndex;
		}
	}

	return INDEX_NONE;
}

int32 UIH_BuildPaletteHostWidget::HitTestWorldStampTile(const FVector2D& ScreenAbsolute) const
{
	if (!ActiveFlyOutTab.IsSet() || ActiveFlyOutTab.GetValue() != EIHBuildPaletteTab::World)
	{
		return INDEX_NONE;
	}

	if (!IHInvisibleHandSpec::IsCoastB2bWorldStampPaletteEnabled())
	{
		return INDEX_NONE;
	}

	FSlateRect FlyOutRect;
	if (!TryGetFlyOutScreenRect(FlyOutRect))
	{
		return INDEX_NONE;
	}

	const FVector2D Local(ScreenAbsolute.X - FlyOutRect.Left, ScreenAbsolute.Y - FlyOutRect.Top);
	for (int32 SlotIndex = 0; SlotIndex < CachedWorldStampSlots.Num(); ++SlotIndex)
	{
		const FWorldStampPaletteSlot& StampSlot = CachedWorldStampSlots[SlotIndex];
		if (!StampSlot.bActive || StampSlot.bReserved)
		{
			continue;
		}

		FSlateRect SlotLocal;
		if (!TryGetWorldStampSlotLocalRect(SlotIndex, SlotLocal))
		{
			continue;
		}

		if (Local.X >= SlotLocal.Left && Local.X <= SlotLocal.Right
			&& Local.Y >= SlotLocal.Top && Local.Y <= SlotLocal.Bottom)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

bool UIH_BuildPaletteHostWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)

{

	const int32 TabIndex = HitTestTabIndex(ScreenAbsolute);
	if (TabIndex != INDEX_NONE)
	{
		if (UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())
		{
			if (AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())
			{
				const EIHBuildPaletteTab Tab = TabIndexToEnum(TabIndex);
				if (Tab == EIHBuildPaletteTab::Build)
				{
					PC->DeselectTownGridManager();
				}
				Subsystem->ToggleTabFlyOut(Tab, PC);
				RequestLayoutRefresh();
				UE_LOG(
					LogIH_WB_Demo004, Log,
					TEXT("BuildPaletteHost: click tab=%s flyOutOpen=%d"),
					TabDefs[TabIndex].KeyLetter,
					Subsystem->IsFlyOutOpen() ? 1 : 0);
				return true;
			}
		}
		return false;
	}

	if (!ActiveFlyOutTab.IsSet() || !IsScreenPointOverBuildPalette(ScreenAbsolute))

	{

		return false;

	}



	const int32 GridTileIndex = HitTestGridTemplateTile(ScreenAbsolute);
	if (GridTileIndex != INDEX_NONE && CachedGridRows.IsValidIndex(GridTileIndex))
	{
		if (UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())
		{
			const FName ItemID = CachedGridRows[GridTileIndex].itemID;
			if (Subsystem->BeginDragFromItem(ItemID, OwnerPC.Get()))
			{
				UE_LOG(
					LogIH_WB_Demo004, Log,
					TEXT("BuildPaletteHost: grip drag started tile=%d item=%s"),
					GridTileIndex, *ItemID.ToString());
			}
			else
			{
				UE_LOG(
					LogIH_WB_Demo004, Warning,
					TEXT("BuildPaletteHost: grip drag failed tile=%d item=%s"),
					GridTileIndex, *ItemID.ToString());
			}
		}
		return true;
	}

	const int32 BuildTileIndex = HitTestBuildTemplateTile(ScreenAbsolute);
	if (BuildTileIndex != INDEX_NONE && CachedBuildRows.IsValidIndex(BuildTileIndex))
	{
		if (AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())
		{
			PC->DeselectTownGridManager();
		}
		if (UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())
		{
			const FName ItemID = CachedBuildRows[BuildTileIndex].itemID;
			if (Subsystem->BeginDragFromItem(ItemID, OwnerPC.Get()))
			{
				UE_LOG(
					LogIH_WB_Demo004, Log,
					TEXT("BuildPaletteHost: structure drag started tile=%d item=%s"),
					BuildTileIndex, *ItemID.ToString());
			}
			else
			{
				UE_LOG(
					LogIH_WB_Demo004, Warning,
					TEXT("BuildPaletteHost: structure drag failed tile=%d item=%s"),
					BuildTileIndex, *ItemID.ToString());
			}
		}
		return true;
	}

	const int32 WorldStampIndex = HitTestWorldStampTile(ScreenAbsolute);
	if (WorldStampIndex != INDEX_NONE && CachedWorldStampSlots.IsValidIndex(WorldStampIndex))
	{
		const FWorldStampPaletteSlot& StampSlot = CachedWorldStampSlots[WorldStampIndex];
		if (StampSlot.bActive && !StampSlot.bReserved)
		{
			if (UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())
			{
				if (Subsystem->BeginDragFromTerrainStamp(StampSlot.StampId, OwnerPC.Get()))
				{
					UE_LOG(
						LogIH_WB_Demo004, Log,
						TEXT("BuildPaletteHost: terrain stamp drag started slot=%d stamp=%s"),
						WorldStampIndex,
						*FIHTerrainStampCatalog::Get(StampSlot.StampId).RowName.ToString());
				}
				else
				{
					UE_LOG(
						LogIH_WB_Demo004, Warning,
						TEXT("BuildPaletteHost: terrain stamp drag failed slot=%d stamp=%s"),
						WorldStampIndex,
						*FIHTerrainStampCatalog::Get(StampSlot.StampId).RowName.ToString());
				}
			}
			return true;
		}
	}

	UE_LOG(
		LogIH_WB_Demo004, Verbose,
		TEXT("BuildPaletteHost: pointer down on fly-out but no template row hit (buildRows=%d)"),
		CachedBuildRows.Num());

	return true;

}



bool UIH_BuildPaletteHostWidget::HandleScreenPointerMove(const FVector2D& ScreenAbsolute)

{

	const int32 NewGridHover = HitTestGridTemplateTile(ScreenAbsolute);
	const int32 NewBuildHover = HitTestBuildTemplateTile(ScreenAbsolute);
	const int32 NewWorldHover = HitTestWorldStampTile(ScreenAbsolute);
	if (NewGridHover != HoveredTemplateIndex
		|| NewBuildHover != HoveredBuildTemplateIndex
		|| NewWorldHover != HoveredWorldStampIndex)
	{
		HoveredTemplateIndex = NewGridHover;
		HoveredBuildTemplateIndex = NewBuildHover;
		HoveredWorldStampIndex = NewWorldHover;
		Invalidate(EInvalidateWidget::Paint);

	}

	return IsScreenPointOverBuildPalette(ScreenAbsolute);

}



int32 UIH_BuildPaletteHostWidget::NativePaint(

	const FPaintArgs& Args,

	const FGeometry& AllottedGeometry,

	const FSlateRect& MyCullingRect,

	FSlateWindowElementList& OutDrawElements,

	int32 LayerId,

	const FWidgetStyle& InWidgetStyle,

	bool bParentEnabled) const

{

	int32 PaintLayer = LayerId;

	const bool bBuildDragActive = BuildPaletteSubsystem.IsValid()
		&& BuildPaletteSubsystem->IsDragActive()
		&& BuildPaletteSubsystem->GetDragPayload().paletteTab == EIHBuildPaletteTab::Build;
	const bool bTerrainStampDragActive = BuildPaletteSubsystem.IsValid()
		&& BuildPaletteSubsystem->IsTerrainStampDragActive();

	if ((GetVisibility() == ESlateVisibility::Collapsed || !bTabStripVisible)
		&& !bBuildDragActive && !bTerrainStampDragActive)
	{
		bHasCachedTabStripScreenRect = false;
		return Super::NativePaint(
			Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	const FGeometry BaseGeometry = ResolveBaseGeometry(AllottedGeometry);
	const FVector2D OverlaySize = ResolveOverlaySize(BaseGeometry);

	// Fly-out chrome must paint *before* Super::NativePaint so UMG tile rows stay visible.
	if (ActiveFlyOutTab.IsSet())
	{
		const float FlyOutW = GetActiveFlyOutWidth();
		const float FlyOutH = ComputeHostPanelHeight();
		const FGeometry FlyOutGeometry = MakeFlyOutPaintGeometry(BaseGeometry);

		if (FlyOutW >= 1.f && FlyOutH >= 1.f)
		{
			const FLinearColor FlyOutBackground = UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(
				FName(TEXT("PanelBackground")), IH_P1C08_DevPanelStyle::PanelBackgroundAlpha);
			DrawSolidLocalRect(
				OutDrawElements,
				PaintLayer,
				FlyOutGeometry,
				FSlateRect(0.f, 0.f, FlyOutW, FlyOutH),
				FlyOutBackground);

			static const FSlateRoundedBoxBrush FlyOutOutlineBrush(
				FLinearColor::Transparent,
				FVector4(3.f, 3.f, 3.f, 3.f),
				IH_BuildPalettePanelStyle::FocusBlue,
				2.f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				PaintLayer + 1,
				FlyOutGeometry.ToPaintGeometry(FVector2f(FlyOutW, FlyOutH), FSlateLayoutTransform(FVector2f::ZeroVector)),
				&FlyOutOutlineBrush,
				ESlateDrawEffect::None,
				FLinearColor::Transparent);

			PaintLayer += 2;
		}
	}

	int32 MaxLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, PaintLayer, InWidgetStyle, bParentEnabled);

	if (ActiveFlyOutTab.IsSet())
	{
		const FGeometry FlyOutGeometry = MakeFlyOutPaintGeometry(BaseGeometry);
		if (ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::Grid)
		{
			MaxLayer = PaintGridFlyOutContent(FlyOutGeometry, OutDrawElements, MaxLayer + 1);
		}
		else if (ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::Build)
		{
			MaxLayer = PaintBuildFlyOutContent(FlyOutGeometry, OutDrawElements, MaxLayer + 1);
		}
		else if (ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::World)
		{
			MaxLayer = PaintWorldFlyOutContent(FlyOutGeometry, OutDrawElements, MaxLayer + 1);
		}
	}

	const FGeometry StripGeometry = MakeTabStripGeometry(BaseGeometry);
	const float StripW = IH_BuildPalettePanelStyle::TabStripWidth;
	const float StripH = GetTabStripHeight();
	const FVector2D StripAbs = StripGeometry.GetAbsolutePosition();

	if (StripW >= 1.f && StripH >= 1.f)
	{
		CachedTabStripScreenRect = FSlateRect(
			StripAbs.X, StripAbs.Y, StripAbs.X + StripW, StripAbs.Y + StripH);
		bHasCachedTabStripScreenRect = true;

		if (!bLoggedTabStripPaintOnce)
		{
			bLoggedTabStripPaintOnce = true;
			UE_LOG(
				LogTemp, Warning,
				TEXT("BuildPaletteHost: tab strip PAINTED abs=(%.0f,%.0f) size=(%.0f,%.0f) viewport=(%.0f,%.0f) allotted=(%.0f,%.0f)"),
				StripAbs.X, StripAbs.Y, StripW, StripH,
				OverlaySize.X, OverlaySize.Y,
				AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y);
		}

		const FLinearColor PanelBackground = UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(
			FName(TEXT("PanelBackground")), IH_P1C08_DevPanelStyle::PanelBackgroundAlpha * 0.85f);
		DrawSolidLocalRect(
			OutDrawElements,
			MaxLayer + 1,
			StripGeometry,
			FSlateRect(0.f, 0.f, StripW, StripH),
			PanelBackground);

		const FSlateFontInfo TabFont = FCoreStyle::GetDefaultFontStyle(
			"Bold", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
		const FLinearColor DefaultText = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")));
		const int32 ActiveTabIndex = ActiveFlyOutTab.IsSet()
			? EnumToTabIndex(ActiveFlyOutTab.GetValue())
			: INDEX_NONE;

		static constexpr float TabRowHeight = 18.f;
		static constexpr float TabTopPadding = 6.f;
		for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(TabDefs); ++TabIndex)
		{
			const float RowTop = TabTopPadding + static_cast<float>(TabIndex) * TabRowHeight;
			const bool bActive = TabIndex == ActiveTabIndex;
			const FLinearColor TextColor = bActive
				? IH_BuildPalettePanelStyle::FocusBlue
				: DefaultText;

			FSlateDrawElement::MakeText(
				OutDrawElements,
				MaxLayer + 2,
				StripGeometry.ToPaintGeometry(
					FVector2f(StripW, TabRowHeight),
					FSlateLayoutTransform(FVector2f(10.f, RowTop + 1.f))),
				TabDefs[TabIndex].KeyLetter,
				TabFont,
				ESlateDrawEffect::None,
				TextColor);
		}

		MaxLayer += 2;
	}
	else
	{
		bHasCachedTabStripScreenRect = false;
	}

	if (ActiveFlyOutTab.IsSet() && ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::Grid
		&& CachedGridRows.IsValidIndex(HoveredTemplateIndex))
	{
		const FGeometry FlyOutGeometry = MakeFlyOutPaintGeometry(BaseGeometry);
		MaxLayer = PaintGridTemplateHoverOutline(AllottedGeometry, FlyOutGeometry, OutDrawElements, MaxLayer);
		MaxLayer = PaintGridTemplateHoverTooltip(FlyOutGeometry, OutDrawElements, MaxLayer);
	}
	else if (ActiveFlyOutTab.IsSet() && ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::Build
		&& CachedBuildRows.IsValidIndex(HoveredBuildTemplateIndex))
	{
		FSlateRect RowLocal;
		if (TryGetBuildTemplateRowLocalRect(HoveredBuildTemplateIndex, RowLocal))
		{
			const FGeometry FlyOutGeometry = MakeFlyOutPaintGeometry(BaseGeometry);
			MaxLayer = PaintFlyOutRowHoverOutline(
				AllottedGeometry, FlyOutGeometry, RowLocal, OutDrawElements, MaxLayer);
		}
	}

	// Structure placement uses world mesh ghost; skip HUD square marker (was mistaken for lasso).

	return MaxLayer;

}

int32 UIH_BuildPaletteHostWidget::PaintBuildDragPlacementMarker(
	const FGeometry& /*AllottedGeometry*/,
	FSlateWindowElementList& /*OutDrawElements*/,
	int32 LayerId) const
{
	// Structure placement uses in-world translucent mesh ghost (IH_StructurePlacementActor).
	return LayerId;
}

void UIH_BuildPaletteHostWidget::InitializeBuildPalette(

	UIH_BuildPaletteSubsystem* InSubsystem,

	AIH_Cube2FlyPlayerController* InPC)

{

	BuildPaletteSubsystem = InSubsystem;

	OwnerPC = InPC;

	if (InSubsystem && InSubsystem->HasTabStripEnabled())
	{
		bTabStripVisible = true;
	}

	EnsureWidgetTree();

	RefreshGridTemplateList();
	RefreshBuildTemplateList();
	RefreshWorldStampPalette();

	SyncHostLayout();

}

void UIH_BuildPaletteHostWidget::RefreshBuildTemplateList()
{
	CachedBuildRows.Reset();

	if (UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())
	{
		if (UIHTownGridDataSubsystem* Data = Subsystem->GetBuildPaletteDataSubsystem())
		{
			if (const UDataTable* ItemTable = Data->GetBuildPaletteItemTable())
			{
				for (const FName& RowName : ItemTable->GetRowNames())
				{
					if (const FIHBuildPaletteItemRow* Row = ItemTable->FindRow<FIHBuildPaletteItemRow>(
						RowName, TEXT("BuildPaletteHostWidget::RefreshBuildTemplateList")))
					{
						if (Row->paletteTab == EIHBuildPaletteTab::Build
							&& Row->interactionType == EIHBuildPaletteInteraction::DropActor)
						{
							CachedBuildRows.Add(*Row);
						}
					}
				}
			}
		}
	}

	CachedBuildRows.Sort([](const FIHBuildPaletteItemRow& A, const FIHBuildPaletteItemRow& B) {
		if (A.sortOrder != B.sortOrder)
		{
			return A.sortOrder < B.sortOrder;
		}
		return A.displayName < B.displayName;
	});

	RequestLayoutRefresh();
}

void UIH_BuildPaletteHostWidget::RefreshWorldStampPalette()
{
	CachedWorldStampSlots.Reset();
	HoveredWorldStampIndex = INDEX_NONE;

	if (!IHInvisibleHandSpec::IsCoastB2bWorldStampPaletteEnabled())
	{
		RequestLayoutRefresh();
		return;
	}

	auto MakeShortLabel = [](const FIHTerrainStampDefinition& Def) -> FString
	{
		FString Label = Def.RowName.ToString().Replace(TEXT("Stamp_"), TEXT(""));
		if (Label.Len() > 7)
		{
			return Label.Left(7);
		}
		return Label;
	};

	for (const EIHTerrainStampId StampId : FIHTerrainStampCatalog::GetAllStampIds())
	{
		if (StampId == EIHTerrainStampId::IslandShelf || StampId >= EIHTerrainStampId::MAX)
		{
			continue;
		}

		const FIHTerrainStampDefinition& Def = FIHTerrainStampCatalog::Get(StampId);
		FWorldStampPaletteSlot StampSlot;
		StampSlot.StampId = StampId;
		StampSlot.bActive = true;
		StampSlot.ShortLabel = MakeShortLabel(Def);
		CachedWorldStampSlots.Add(StampSlot);
	}

	{
		FWorldStampPaletteSlot SpecialSlot;
		SpecialSlot.StampId = EIHTerrainStampId::IslandShelf;
		SpecialSlot.bActive = true;
		SpecialSlot.ShortLabel = TEXT("Shelf");
		CachedWorldStampSlots.Add(SpecialSlot);
	}

	for (int32 ReservedIndex = 0; ReservedIndex < IHInvisibleHandSpec::TerrainStampPaletteReservedSlotCount; ++ReservedIndex)
	{
		FWorldStampPaletteSlot ReservedSlot;
		ReservedSlot.bReserved = true;
		ReservedSlot.ShortLabel = TEXT("+");
		CachedWorldStampSlots.Add(ReservedSlot);
	}

	ensure(CachedWorldStampSlots.Num() == IHInvisibleHandSpec::TerrainStampPaletteSlotCapacity);
	RequestLayoutRefresh();
}



void UIH_BuildPaletteHostWidget::SetTabStripVisible(bool bVisible)
{
	bTabStripVisible = bVisible;
	bHasCachedTabStripScreenRect = false;
	SyncHostLayout();
}

void UIH_BuildPaletteHostWidget::SetActiveFlyOutTab(TOptional<EIHBuildPaletteTab> Tab)
{
	ActiveFlyOutTab = Tab;
	bHasCachedTabStripScreenRect = false;
	SyncHostLayout();
}

bool UIH_BuildPaletteHostWidget::IsWorldFlyOutVisible() const
{
	return ActiveFlyOutTab.IsSet() && ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::World;
}

void UIH_BuildPaletteHostWidget::SetGridFlyOutOpen(bool bOpen)
{
	if (bOpen)
	{
		ActiveFlyOutTab = EIHBuildPaletteTab::Grid;
	}
	else if (ActiveFlyOutTab.IsSet() && ActiveFlyOutTab.GetValue() == EIHBuildPaletteTab::Grid)
	{
		ActiveFlyOutTab.Reset();
	}

	SyncHostLayout();
}

int32 UIH_BuildPaletteHostWidget::EnumToTabIndex(EIHBuildPaletteTab Tab)
{
	switch (Tab)
	{
	case EIHBuildPaletteTab::Grid: return 0;
	case EIHBuildPaletteTab::World: return 1;
	case EIHBuildPaletteTab::Build: return 2;
	case EIHBuildPaletteTab::Convey: return 3;
	case EIHBuildPaletteTab::Defense: return 4;
	default: return INDEX_NONE;
	}
}

EIHBuildPaletteTab UIH_BuildPaletteHostWidget::TabIndexToEnum(int32 TabIndex) const
{
	switch (TabIndex)
	{
	case 0: return EIHBuildPaletteTab::Grid;
	case 1: return EIHBuildPaletteTab::World;
	case 2: return EIHBuildPaletteTab::Build;
	case 3: return EIHBuildPaletteTab::Convey;
	case 4: return EIHBuildPaletteTab::Defense;
	default: return EIHBuildPaletteTab::Grid;
	}
}

void UIH_BuildPaletteHostWidget::SyncFlyOutContentVisibility()
{
	const EIHBuildPaletteTab VisibleTab = ActiveFlyOutTab.IsSet()
		? ActiveFlyOutTab.GetValue()
		: EIHBuildPaletteTab::Grid;

	auto SetVBoxVisible = [](UVerticalBox* VBox, bool bVisible)
	{
		if (VBox)
		{
			VBox->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	};

	// Grid / Build lists are painted in NativePaint; keep UMG fly-out vboxes collapsed.
	SetVBoxVisible(GridFlyOutVBox, false);
	SetVBoxVisible(BuildFlyOutVBox, false);
	SetVBoxVisible(WorldFlyOutVBox, VisibleTab == EIHBuildPaletteTab::World && ActiveFlyOutTab.IsSet());
	SetVBoxVisible(ConveyFlyOutVBox, VisibleTab == EIHBuildPaletteTab::Convey && ActiveFlyOutTab.IsSet());
	SetVBoxVisible(DefenseFlyOutVBox, VisibleTab == EIHBuildPaletteTab::Defense && ActiveFlyOutTab.IsSet());
}

namespace IH_BuildPaletteHostWidgetPrivate
{
	FString GridTemplateIconGlyph(const FIHBuildPaletteItemRow& Row)
	{
		switch (Row.townGridTemplate)
		{
		case EIHTownGridTemplate::Squared: return TEXT("T1");
		case EIHTownGridTemplate::Harmonic: return TEXT("T2");
		case EIHTownGridTemplate::Radial: return TEXT("T3");
		case EIHTownGridTemplate::Citadel: return TEXT("T4");
		case EIHTownGridTemplate::Valley: return TEXT("T5");
		default: break;
		}
		return Row.displayName.IsEmpty() ? Row.itemID.ToString() : Row.displayName;
	}
}

void UIH_BuildPaletteHostWidget::RefreshGridTemplateList()

{

	if (!TemplateListVBox || !WidgetTree)

	{

		return;

	}



	TemplateListVBox->ClearChildren();

	TemplateTiles.Reset();

	CachedGridRows.Reset();



	if (UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get())

	{

		if (UIHTownGridDataSubsystem* Data = Subsystem->GetBuildPaletteDataSubsystem())

		{

			if (const UDataTable* ItemTable = Data->GetBuildPaletteItemTable())

			{

				const TArray<FName> RowNames = ItemTable->GetRowNames();

				for (const FName& RowName : RowNames)

				{

					if (const FIHBuildPaletteItemRow* Row = ItemTable->FindRow<FIHBuildPaletteItemRow>(

						RowName, TEXT("BuildPaletteHostWidget::RefreshGridTemplateList")))

					{

						if (Row->paletteTab == EIHBuildPaletteTab::Grid)

						{

							CachedGridRows.Add(*Row);

						}

					}

				}



				if (CachedGridRows.Num() == 0 && RowNames.Num() > 0)

				{

					for (const FName& RowName : RowNames)

					{

						if (const FIHBuildPaletteItemRow* Row = ItemTable->FindRow<FIHBuildPaletteItemRow>(

							RowName, TEXT("BuildPaletteHostWidget::RefreshGridTemplateList::Diagnose")))

						{

							UE_LOG(

								LogIH_WB_Demo004, Warning,

								TEXT("BuildPaletteHostWidget: row %s skipped — paletteTab=%d interactionType=%d displayName=%s"),

								*RowName.ToString(),

								static_cast<int32>(Row->paletteTab),

								static_cast<int32>(Row->interactionType),

								*Row->displayName);



							if (Row->interactionType == EIHBuildPaletteInteraction::GripTemplate

								&& Row->categoryPath.Contains(TEXT("TownTemplates")))

							{

								CachedGridRows.Add(*Row);

							}

						}

						else

						{

							UE_LOG(

								LogIH_WB_Demo004, Warning,

								TEXT("BuildPaletteHostWidget: row %s FindRow failed (row struct mismatch?)"),

								*RowName.ToString());

						}

					}

				}

			}

		}

	}



	CachedGridRows.Sort([](const FIHBuildPaletteItemRow& A, const FIHBuildPaletteItemRow& B) {

		if (A.sortOrder != B.sortOrder)

		{

			return A.sortOrder < B.sortOrder;

		}

		return A.displayName < B.displayName;

	});

	for (FIHBuildPaletteItemRow& Row : CachedGridRows)
	{
		if (!Row.icon.IsNull())
		{
			Row.icon.LoadSynchronous();
		}
	}



	for (int32 Index = 0; Index < CachedGridRows.Num(); ++Index)

	{

		const FIHBuildPaletteItemRow& Row = CachedGridRows[Index];

		const FString LabelText = Row.displayName.IsEmpty()

			? Row.itemID.ToString()

			: Row.displayName;



		FGridTemplateTileWidgets TileWidgets;

		TileWidgets.ItemID = Row.itemID;



		TileWidgets.RowBorder = WidgetTree->ConstructWidget<UBorder>(

			UBorder::StaticClass(), *FString::Printf(TEXT("GridTemplateBorder_%d"), Index));

		IH_BuildPalettePanelStyle::ApplyGridTemplateTileBorderStyle(TileWidgets.RowBorder);

		TileWidgets.RowBorder->SetPadding(FMargin(4.f, 2.f));



		UHorizontalBox* RowHBox = WidgetTree->ConstructWidget<UHorizontalBox>(

			UHorizontalBox::StaticClass(), *FString::Printf(TEXT("GridTemplateRow_%d"), Index));

		TileWidgets.RowBorder->AddChild(RowHBox);



		TileWidgets.IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(

			USizeBox::StaticClass(), *FString::Printf(TEXT("GridTemplateIconBox_%d"), Index));

		TileWidgets.IconSizeBox->SetWidthOverride(IH_BuildPalettePanelStyle::GridTemplateTileSize);

		TileWidgets.IconSizeBox->SetHeightOverride(IH_BuildPalettePanelStyle::GridTemplateTileSize);



		UOverlay* IconOverlay = WidgetTree->ConstructWidget<UOverlay>(

			UOverlay::StaticClass(), *FString::Printf(TEXT("GridTemplateIconOverlay_%d"), Index));

		TileWidgets.IconSizeBox->AddChild(IconOverlay);



		const FString IconGlyph = IH_BuildPaletteHostWidgetPrivate::GridTemplateIconGlyph(Row);

		UTexture2D* IconTexture = Row.icon.IsNull() ? nullptr : Row.icon.LoadSynchronous();

		if (IconTexture)

		{

			TileWidgets.IconImage = WidgetTree->ConstructWidget<UImage>(

				UImage::StaticClass(), *FString::Printf(TEXT("GridTemplateIcon_%d"), Index));

			TileWidgets.IconImage->SetBrushFromTexture(IconTexture, true);

			if (UOverlaySlot* ImageSlot = IconOverlay->AddChildToOverlay(TileWidgets.IconImage))

			{

				ImageSlot->SetHorizontalAlignment(HAlign_Fill);

				ImageSlot->SetVerticalAlignment(VAlign_Fill);

			}

		}

		else

		{

			UTextBlock* IconFallbackText = IH_P1C08_DevPanelStyle::MakeHUDLabel(

				WidgetTree,

				*FString::Printf(TEXT("GridTemplateIconFallback_%d"), Index),

				IconGlyph,

				FName(TEXT("BodyText")));

			IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(

				IconFallbackText, IH_P1C08_DevPanelStyle::CompactLabelFontSize + 2.f);

			if (UOverlaySlot* FallbackSlot = IconOverlay->AddChildToOverlay(IconFallbackText))

			{

				FallbackSlot->SetHorizontalAlignment(HAlign_Center);

				FallbackSlot->SetVerticalAlignment(VAlign_Center);

			}

		}



		TileWidgets.LabelText = IH_P1C08_DevPanelStyle::MakeHUDLabel(

			WidgetTree,

			*FString::Printf(TEXT("GridTemplateLabel_%d"), Index),

			LabelText,

			FName(TEXT("BodyText")));

		IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(

			TileWidgets.LabelText, IH_P1C08_DevPanelStyle::CompactLabelFontSize);



		if (UHorizontalBoxSlot* IconSlot = RowHBox->AddChildToHorizontalBox(TileWidgets.IconSizeBox))

		{

			IconSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

			IconSlot->SetVerticalAlignment(VAlign_Center);

		}

		if (UHorizontalBoxSlot* LabelSlot = RowHBox->AddChildToHorizontalBox(TileWidgets.LabelText))

		{

			LabelSlot->SetVerticalAlignment(VAlign_Center);

			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		}



		if (!Row.tooltip.IsEmpty())

		{

			const FText TooltipText = FText::FromString(Row.tooltip);

			TileWidgets.RowBorder->SetToolTipText(TooltipText);

			if (TileWidgets.IconImage)

			{

				TileWidgets.IconImage->SetToolTipText(TooltipText);

			}

			TileWidgets.LabelText->SetToolTipText(TooltipText);

		}



		TemplateTiles.Add(TileWidgets);



		if (UVerticalBoxSlot* RowSlot = TemplateListVBox->AddChildToVerticalBox(TileWidgets.RowBorder))

		{

			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, IH_BuildPalettePanelStyle::GridTemplateRowGap));

		}

	}



	SyncHostLayout();

}

