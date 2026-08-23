// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_P1C08_DevSeedPanelWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IH_WB_Demo004GameMode.h"
#include "IH_Cube2FlyPlayerController.h"
#include "IHMapSeedFrameworkLibrary.h"
#include "IHSeedIslandLibrary.h"
#include "IHSeedValidationLibrary.h"

#include "IHUIColorSchemeLibrary.h"

#include "Components/Border.h"

#include "Components/Button.h"

#include "Components/CanvasPanel.h"

#include "Components/CanvasPanelSlot.h"

#include "Components/EditableTextBox.h"

#include "Components/HorizontalBox.h"

#include "Components/HorizontalBoxSlot.h"

#include "Components/SizeBox.h"

#include "Components/TextBlock.h"

#include "Components/VerticalBox.h"

#include "Components/VerticalBoxSlot.h"

#include "Blueprint/WidgetTree.h"

#include "Engine/GameInstance.h"

#include "GameFramework/PlayerController.h"

#include "Math/RandomStream.h"

#include "Styling/CoreStyle.h"



namespace

{

	static constexpr int32 MaxRealmSeedLength = 6;

}



void UIH_P1C08_DevSeedPanelWidget::EnsureWidgetTree()

{

	if (!WidgetTree || WidgetTree->RootWidget)

	{

		return;

	}



	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SeedPanelRoot"));

	WidgetTree->RootWidget = Canvas;



	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SeedPanelBorder"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder, 0.9f, true);



	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SeedPanelVBox"));



	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("SeedPanelTitle"), TEXT("Realm Seed"));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText, IH_P1C08_DevPanelStyle::CompactLabelFontSize);

	if (UVerticalBoxSlot* TitleSlot = VB->AddChildToVerticalBox(TitleText))

	{

		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	}



	SeedTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("SeedInput"));

	SeedTextBox->SetHintText(FText::FromString(TEXT("ABBEY2")));

	SeedTextBox->SetText(FText::FromString(TEXT("ABBEY2")));

	SeedTextBox->SetClearKeyboardFocusOnCommit(false);

	SeedTextBox->OnTextChanged.AddDynamic(this, &UIH_P1C08_DevSeedPanelWidget::HandleSeedTextChanged);

	SeedTextBox->OnTextCommitted.AddDynamic(this, &UIH_P1C08_DevSeedPanelWidget::HandleSeedTextCommitted);

	{
		FEditableTextBoxStyle TextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
		TextBoxStyle.Padding = FMargin(4.f, 2.f);
		TextBoxStyle.TextStyle.Font = FCoreStyle::GetDefaultFontStyle("Regular", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
		SeedTextBox->WidgetStyle = TextBoxStyle;
	}

	if (UVerticalBoxSlot* SeedSlot = VB->AddChildToVerticalBox(SeedTextBox))

	{

		SeedSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

	}



	IslandHintText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("IslandHint"), TEXT("-> 3 islands"), FName(TEXT("SecondaryText")));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(IslandHintText, IH_P1C08_DevPanelStyle::CompactLabelFontSize);

	if (UVerticalBoxSlot* HintSlot = VB->AddChildToVerticalBox(IslandHintText))

	{

		HintSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	}



	SculptButton = MakePanelButton(TEXT("SculptBtn"), TEXT("Sculpt This Seed"), FName(TEXT("HandleSculptClicked")));

	if (UVerticalBoxSlot* SculptSlot = VB->AddChildToVerticalBox(SculptButton))

	{

		SculptSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));

	}



	RandomRealmButton = MakePanelButton(TEXT("RandomBtn"), TEXT("Render a Random Realm"), FName(TEXT("HandleRandomRealmClicked")));

	if (UVerticalBoxSlot* RandomSlot = VB->AddChildToVerticalBox(RandomRealmButton))

	{

		RandomSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));

	}



	UHorizontalBox* CountRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("IslandCountRow"));

	IslandMinusButton = MakePanelButton(TEXT("IslandMinus"), TEXT("−"), FName(TEXT("HandleIslandMinusClicked")));

	IslandPlusButton = MakePanelButton(TEXT("IslandPlus"), TEXT("+"), FName(TEXT("HandleIslandPlusClicked")));

	RandomRealmCountText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("RandomCountLabel"), TEXT("Islands: 4"), FName(TEXT("SecondaryText")));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(RandomRealmCountText, IH_P1C08_DevPanelStyle::CompactLabelFontSize);



	if (UHorizontalBoxSlot* MinusSlot = CountRow->AddChildToHorizontalBox(IslandMinusButton))

	{

		MinusSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	}

	CountRow->AddChildToHorizontalBox(RandomRealmCountText);

	if (UHorizontalBoxSlot* PlusSlot = CountRow->AddChildToHorizontalBox(IslandPlusButton))

	{

		PlusSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	}

	if (UVerticalBoxSlot* CountSlot = VB->AddChildToVerticalBox(CountRow))
	{
		CountSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	TankInfoLabel = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("TankInfoLabel"), TEXT("Realm: 42.1x26 km (φ) - 0 ac"), FName(TEXT("SecondaryText")));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TankInfoLabel, IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	if (UVerticalBoxSlot* TankLabelSlot = VB->AddChildToVerticalBox(TankInfoLabel))
	{
		TankLabelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	StatusText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("SeedStatus"), TEXT(""), FName(TEXT("ErrorWarningText")));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(StatusText, IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	StatusText->SetAutoWrapText(true); // Plan Addendum 20: long "Regenerated N from SEED - templates [...]" messages were overflowing the panel's fixed width instead of wrapping
	StatusText->SetVisibility(ESlateVisibility::Collapsed);

	if (UVerticalBoxSlot* StatusSlot = VB->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}



	PanelBorder->AddChild(VB);

	// Plan Addendum 21: force this panel to the same width as Island Nav/Sun Position (user
	// reported the three top-left panels rendering at different widths despite sharing the same
	// CanvasSlot PanelWidth) - wrapping in a SizeBox with an explicit width override guarantees
	// it regardless of whether the Border's own content happens to be narrower.
	USizeBox* WidthLock = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RealmSeedWidthLock"));
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
			IH_P1C08_DevPanelStyle::EStackSlot::RealmSeed,
			IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
				IH_P1C08_DevPanelStyle::EStackSlot::RealmSeed, IslandCount),
			IslandCount);

	}

}



UButton* UIH_P1C08_DevSeedPanelWidget::MakePanelButton(const FName& Name, const FString& Label, const FName& ClickHandler)
{
	return IH_P1C08_DevPanelStyle::MakeHUDButton(WidgetTree, this, Name, Label, ClickHandler, true);
}

void UIH_P1C08_DevSeedPanelWidget::ApplyDevPanelStackPosition(float TopY, float ContentHeight)
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

void UIH_P1C08_DevSeedPanelWidget::UpdatePanelLayout()
{
	int32 IslandCount = 3;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		IslandCount = GI->GetProceduralIslandCount();
	}

	ApplyDevPanelStackPosition(
		IH_P1C08_DevPanelStyle::GetStackPosition(
			IH_P1C08_DevPanelStyle::EStackSlot::RealmSeed, IslandCount).Y,
		IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
			IH_P1C08_DevPanelStyle::EStackSlot::RealmSeed, IslandCount));
}



TSharedRef<SWidget> UIH_P1C08_DevSeedPanelWidget::RebuildWidget()

{

	EnsureWidgetTree();

	return Super::RebuildWidget();

}



void UIH_P1C08_DevSeedPanelWidget::NativeConstruct()

{

	Super::NativeConstruct();

	EnsureWidgetTree();

	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.f, 0.f)); // Plan Addendum 19

	// HitTestInvisible: PlayerController routes pointer hits via HandleScreenPointerDown.
	// Visible would let Slate UButton OnClicked fire too and double-step ± (3→5).
	SetVisibility(ESlateVisibility::HitTestInvisible);

	SyncFromGameInstance();

	RefreshIslandHint();
	RefreshTankInfoLabel();
}



bool UIH_P1C08_DevSeedPanelWidget::IsConsumingKeyboard() const

{

	return SeedTextBox && SeedTextBox->HasKeyboardFocus();

}



bool UIH_P1C08_DevSeedPanelWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return PanelBorder && PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_DevSeedPanelWidget::DispatchButtonClickIfUnder(UButton* Button, const FVector2D& ScreenAbsolute)
{
	if (!Button || !Button->GetIsEnabled())
	{
		return false;
	}

	if (!Button->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		return false;
	}

	Button->OnClicked.Broadcast();
	return true;
}

bool UIH_P1C08_DevSeedPanelWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!IsPointOverPanel(ScreenAbsolute))
	{
		return false;
	}

	if (DispatchButtonClickIfUnder(SculptButton, ScreenAbsolute)
		|| DispatchButtonClickIfUnder(RandomRealmButton, ScreenAbsolute)
		|| DispatchButtonClickIfUnder(IslandMinusButton, ScreenAbsolute)
		|| DispatchButtonClickIfUnder(IslandPlusButton, ScreenAbsolute))
	{
		return true;
	}

	if (SeedTextBox && SeedTextBox->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		SeedTextBox->SetKeyboardFocus();
		return true;
	}

	return true;
}

bool UIH_P1C08_DevSeedPanelWidget::ProcessPanelPointerDown(const FVector2D& ScreenAbsolute)
{
	return HandleScreenPointerDown(ScreenAbsolute);
}



void UIH_P1C08_DevSeedPanelWidget::SyncFromGameInstance()

{

	if (UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())

	{

		bSuppressSeedTextChanged = true;

		if (SeedTextBox)

		{

			SeedTextBox->SetText(FText::FromString(GI->GetCurrentWorldSeed()));

		}

		bSuppressSeedTextChanged = false;

		if (RandomRealmCountText)

		{

			RandomRealmCountText->SetText(FText::FromString(FString::Printf(TEXT("Islands: %d"), GI->GetRandomRealmIslandCount())));

		}

	}

}



void UIH_P1C08_DevSeedPanelWidget::RefreshIslandHint()

{

	if (!IslandHintText)

	{

		return;

	}

	const FString Seed = SeedTextBox ? SeedTextBox->GetText().ToString() : FString();

	const int32 N = UIHSeedValidationLibrary::ExtractIslandCountFromEightCharSeed(UIHSeedValidationLibrary::NormalizeSeedString(Seed));

	if (N >= 2 && N <= 7)

	{

		IslandHintText->SetText(FText::FromString(FString::Printf(TEXT("-> %d islands"), N)));

	}

	else

	{

		IslandHintText->SetText(FText::FromString(TEXT("-> ? islands (last digit 2-7)")));

	}

}



void UIH_P1C08_DevSeedPanelWidget::RefreshTankInfoLabel()
{
	UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	if (!GI || !TankInfoLabel)
	{
		return;
	}

	const float FullDepthKm = GI->GetRealmHalfExtentNSKm() * 2.f;
	const float FullWidthKm = GI->GetRealmHalfExtentEWKm() * 2.f;
	const int32 TotalAcres = GI->GetTotalLandAcres();
	TankInfoLabel->SetText(FText::FromString(
		FString::Printf(TEXT("Realm: %.1fx%.0f km (φ) - %d ac"), FullWidthKm, FullDepthKm, TotalAcres)));
}

void UIH_P1C08_DevSeedPanelWidget::SetStatusMessage(const FString& Message, bool bIsError)

{

	if (!StatusText)

	{

		return;

	}

	if (Message.IsEmpty())
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
		StatusText->SetText(FText::GetEmpty());
		return;
	}

	StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
	StatusText->SetText(FText::FromString(Message));

	StatusText->SetColorAndOpacity(FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(

		bIsError ? FName(TEXT("ErrorWarningText")) : FName(TEXT("SecondaryText")))));

}



void UIH_P1C08_DevSeedPanelWidget::HandleSeedTextChanged(const FText& Text)

{

	if (bSuppressSeedTextChanged)

	{

		return;

	}

	const FString Raw = Text.ToString();

	if (Raw.Len() > MaxRealmSeedLength && SeedTextBox)

	{

		bSuppressSeedTextChanged = true;

		SeedTextBox->SetText(FText::FromString(Raw.Left(MaxRealmSeedLength)));

		bSuppressSeedTextChanged = false;

	}

	RefreshIslandHint();

}



void UIH_P1C08_DevSeedPanelWidget::HandleSeedTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)

{

	if (CommitMethod != ETextCommit::OnEnter)

	{

		return;

	}

	ApplyValidatedSeedAndRegenerate(Text.ToString());

}



void UIH_P1C08_DevSeedPanelWidget::ApplyValidatedSeedAndRegenerate(const FString& RawSeed)

{

	UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();

	if (!GI)

	{

		SetStatusMessage(TEXT("Game instance not available."), true);

		return;

	}

	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(GetOwningPlayer());
	if (FlyPC)
	{
		FlyPC->BeginRealmRegenProgress(TEXT("Validating seed..."));
	}



	FString Err;

	FString Norm;

	int32 IslandCount = 0;

	int32 MasterSeed = 0;

	if (!UIHSeedValidationLibrary::ValidateSeedWithTableOrCsv(RawSeed, nullptr, DefaultSeedWordsCsvPath(), Err, Norm, IslandCount,

			MasterSeed))

	{

		if (FlyPC)
		{
			FlyPC->EndRealmRegenProgress();
		}
		SetStatusMessage(Err, true);

		return;

	}


	SetStatusMessage(FString::Printf(TEXT("Seed %s accepted - Building islands..."), *Norm), false);

	bSuppressSeedTextChanged = true;

	if (SeedTextBox)

	{

		SeedTextBox->SetText(FText::FromString(Norm));

	}

	bSuppressSeedTextChanged = false;

	RefreshIslandHint();

	auto FinishRegenerateUi = [this, GI, IslandCount, Norm]()
	{
		RefreshTankInfoLabel();

		FString TemplateSummary;
		if (const FIHMapSeedPhase1Result* Phase1 = &GI->GetMapSeedPhase1())
		{
			if (Phase1->bSuccess)
			{
				for (const FIHIslandSpawnPlan& Plan : Phase1->SpawnPlans)
				{
					if (!TemplateSummary.IsEmpty())
					{
						TemplateSummary += TEXT(", ");
					}
					TemplateSummary += FString::Printf(
						TEXT("%d:%s"),
						Plan.IslandIndex,
						*UIHMapSeedFrameworkLibrary::IslandTemplateTypeToString(Plan.TemplateType));
				}
			}
		}
		SetStatusMessage(
			TemplateSummary.IsEmpty()
				? FString::Printf(TEXT("Regenerated %d islands from %s."), IslandCount, *Norm)
				: FString::Printf(TEXT("Regenerated %d from %s - templates [%s]."), IslandCount, *Norm, *TemplateSummary),
			false);
	};

	if (FlyPC)
	{
		FlyPC->PrepareRealmRegenFromSeed(Norm, FinishRegenerateUi);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GI->SetCurrentWorldSeed(Norm);
			GM->RegenerateIslandsFromSeed();
			FinishRegenerateUi();
			return;
		}
	}

	SetStatusMessage(TEXT("Story game mode not available."), true);
}



void UIH_P1C08_DevSeedPanelWidget::HandleSculptClicked()

{

	const FString Raw = SeedTextBox ? SeedTextBox->GetText().ToString() : FString();

	ApplyValidatedSeedAndRegenerate(Raw);

}



void UIH_P1C08_DevSeedPanelWidget::HandleRandomRealmClicked()

{

	UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();

	if (!GI)

	{

		SetStatusMessage(TEXT("Game instance not available."), true);

		return;

	}



	FRandomStream Stream;

	Stream.GenerateNewSeed();

	FString OutSeed;

	FString Err;

	if (!UIHSeedValidationLibrary::GenerateRandomValidSeedWithTableOrCsv(

			nullptr, DefaultSeedWordsCsvPath(), Stream, OutSeed, Err, GI->GetRandomRealmIslandCount()))

	{

		SetStatusMessage(Err, true);

		return;

	}



	bSuppressSeedTextChanged = true;

	if (SeedTextBox)

	{

		SeedTextBox->SetText(FText::FromString(OutSeed));

	}

	bSuppressSeedTextChanged = false;

	ApplyValidatedSeedAndRegenerate(OutSeed);

}



FString UIH_P1C08_DevSeedPanelWidget::SeedWithIslandCountDigit(const FString& Seed, int32 IslandCount)
{
	FString Norm = UIHSeedValidationLibrary::NormalizeSeedString(Seed);
	if (Norm.Len() != 8)
	{
		return Norm;
	}

	const int32 Clamped = FMath::Clamp(IslandCount, 2, 7);
	Norm[7] = static_cast<TCHAR>(TEXT('0') + Clamped);
	return Norm;
}

void UIH_P1C08_DevSeedPanelWidget::StepRandomRealmIslandCountAndSyncUi(int32 Delta)
{
	UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	if (!GI)
	{
		return;
	}

	GI->StepRandomRealmIslandCount(Delta);
	const int32 Count = GI->GetRandomRealmIslandCount();

	if (RandomRealmCountText)
	{
		RandomRealmCountText->SetText(FText::FromString(FString::Printf(TEXT("Islands: %d"), Count)));
	}

	// Keep sculpt seed 8th digit aligned with ± counter (regen only on Sculpt / Random Realm).
	const FString PatchedSeed = SeedWithIslandCountDigit(GI->GetCurrentWorldSeed(), Count);
	bSuppressSeedTextChanged = true;
	if (SeedTextBox)
	{
		SeedTextBox->SetText(FText::FromString(PatchedSeed));
	}
	bSuppressSeedTextChanged = false;
	RefreshIslandHint();
}

void UIH_P1C08_DevSeedPanelWidget::HandleIslandMinusClicked()
{
	StepRandomRealmIslandCountAndSyncUi(-1);
}

void UIH_P1C08_DevSeedPanelWidget::HandleIslandPlusClicked()
{
	StepRandomRealmIslandCountAndSyncUi(+1);
}

