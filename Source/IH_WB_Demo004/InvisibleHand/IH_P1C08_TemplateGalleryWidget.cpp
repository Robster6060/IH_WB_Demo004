// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_TemplateGalleryWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IH_P1C08_TemplateGallerySubsystem.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IH_WB_Demo004GameMode.h"
#include "IHIslandTemplateProfileLibrary.h"
#include "IHMapSeedFrameworkLibrary.h"
#include "IH_Cube2FlyPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

void UIH_P1C08_TemplateGalleryWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GalleryRoot"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GalleryPanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder, 0.9f, true);

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GalleryVBox"));
	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("GalleryTitle"), TEXT("Template Gallery"));
	if (UVerticalBoxSlot* TitleSlot = VB->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	StatusText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("GalleryStatus"), TEXT("Gallery — (loading)"), FName(TEXT("SecondaryText")));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(StatusText, IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	if (UVerticalBoxSlot* StatusSlot = VB->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GalleryButtons"));
	PrevButton = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("GalleryPrev"), TEXT("Prev"), FName(TEXT("HandlePrevClicked")), true);
	NextButton = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("GalleryNext"), TEXT("Next"), FName(TEXT("HandleNextClicked")), true);
	ApplyButton = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("GalleryApply"), TEXT("Apply Preview"), FName(TEXT("HandleApplyPreviewClicked")), true);

	if (UHorizontalBoxSlot* PrevSlot = ButtonRow->AddChildToHorizontalBox(PrevButton))
	{
		PrevSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
	if (UHorizontalBoxSlot* NextSlot = ButtonRow->AddChildToHorizontalBox(NextButton))
	{
		NextSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
	ButtonRow->AddChildToHorizontalBox(ApplyButton);
	VB->AddChildToVerticalBox(ButtonRow);

	PanelBorder->AddChild(VB);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		int32 IslandCount = 3;
		if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
		{
			IslandCount = GI->GetProceduralIslandCount();
		}
		IH_P1C08_DevPanelStyle::ConfigureTopLeftPanelSlot(
			CSlot,
			IH_P1C08_DevPanelStyle::EStackSlot::TemplateGallery,
			IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
				IH_P1C08_DevPanelStyle::EStackSlot::TemplateGallery, IslandCount),
			IslandCount);
	}
}

TSharedRef<SWidget> UIH_P1C08_TemplateGalleryWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_TemplateGalleryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.f, 0.f)); // Plan Addendum 19
	RefreshFromGallery();
}

void UIH_P1C08_TemplateGalleryWidget::RefreshFromGallery()
{
	if (!StatusText)
	{
		return;
	}

	FString Status = TEXT("Gallery — no cells");
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_TemplateGallerySubsystem* Gallery = GI->GetSubsystem<UIH_P1C08_TemplateGallerySubsystem>())
		{
			FIHIslandTemplateGalleryCell Cell;
			if (Gallery->GetActiveGalleryCell(Cell))
			{
				Status = FString::Printf(
					TEXT("[%04d/%04d] %s | %s | %s | %s"),
					Cell.GalleryIndex + 1,
					Gallery->GetGalleryCells().Num(),
					*Cell.ReviewSeedWord,
					*UIHMapSeedFrameworkLibrary::IslandTemplateTypeToString(Cell.TemplateType),
					*UIHIslandTemplateProfileLibrary::GalleryViewModeToString(Cell.ViewMode),
					*UIHIslandTemplateProfileLibrary::GalleryZoomLevelToString(Cell.ZoomLevel));
			}
		}
	}
	StatusText->SetText(FText::FromString(Status));
}

void UIH_P1C08_TemplateGalleryWidget::UpdatePanelLayout()
{
	int32 IslandCount = 3;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		IslandCount = GI->GetProceduralIslandCount();
	}

	ApplyDevPanelStackPosition(
		IH_P1C08_DevPanelStyle::GetStackPosition(
			IH_P1C08_DevPanelStyle::EStackSlot::TemplateGallery, IslandCount).Y,
		IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
			IH_P1C08_DevPanelStyle::EStackSlot::TemplateGallery, IslandCount));
}

void UIH_P1C08_TemplateGalleryWidget::ApplyDevPanelStackPosition(float TopY, float ContentHeight)
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

bool UIH_P1C08_TemplateGalleryWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	if (!PanelBorder)
	{
		return false;
	}
	return PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_TemplateGalleryWidget::DispatchButtonClickIfUnder(UButton* Button, const FVector2D& ScreenAbsolute)
{
	if (!Button || Button->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}
	if (Button->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		Button->OnClicked.Broadcast();
		return true;
	}
	return false;
}

bool UIH_P1C08_TemplateGalleryWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!IsPointOverPanel(ScreenAbsolute))
	{
		return false;
	}

	if (DispatchButtonClickIfUnder(PrevButton, ScreenAbsolute)
		|| DispatchButtonClickIfUnder(NextButton, ScreenAbsolute)
		|| DispatchButtonClickIfUnder(ApplyButton, ScreenAbsolute))
	{
		return true;
	}
	return true;
}

void UIH_P1C08_TemplateGalleryWidget::HandlePrevClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_TemplateGallerySubsystem* Gallery = GI->GetSubsystem<UIH_P1C08_TemplateGallerySubsystem>())
		{
			Gallery->StepGalleryIndex(-1);
			RefreshFromGallery();
		}
	}
}

void UIH_P1C08_TemplateGalleryWidget::HandleNextClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_TemplateGallerySubsystem* Gallery = GI->GetSubsystem<UIH_P1C08_TemplateGallerySubsystem>())
		{
			Gallery->StepGalleryIndex(1);
			RefreshFromGallery();
		}
	}
}

void UIH_P1C08_TemplateGalleryWidget::HandleApplyPreviewClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->ApplyActiveGalleryPreview();
		}
	}
	RefreshFromGallery();
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC))
		{
			FlyPC->RefreshDevPanelStackLayout();
			FlyPC->RefreshIslandNavFromSubsystem();
		}
	}
}
