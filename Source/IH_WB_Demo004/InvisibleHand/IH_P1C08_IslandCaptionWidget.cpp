// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_P1C08_IslandCaptionWidget.h"



#include "IH_P1C08_DevPanelStyle.h"

#include "IHUIColorSchemeLibrary.h"

#include "Blueprint/WidgetTree.h"

#include "Components/CanvasPanel.h"

#include "Engine/GameViewportClient.h"

#include "Engine/LocalPlayer.h"

#include "Fonts/SlateFontInfo.h"

#include "GameFramework/PlayerController.h"

#include "Rendering/DrawElements.h"

#include "Styling/CoreStyle.h"

#include "Fonts/FontMeasure.h"

#include "Framework/Application/SlateApplication.h"

#include "Widgets/SViewport.h"



TSharedRef<SWidget> UIH_P1C08_IslandCaptionWidget::RebuildWidget()

{

	if (!WidgetTree)

	{

		WidgetTree = NewObject<UWidgetTree>(this, TEXT("IslandCaptionTree"));

	}

	if (!WidgetTree->RootWidget)

	{

		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CaptionRoot"));

	}

	return Super::RebuildWidget();

}



void UIH_P1C08_IslandCaptionWidget::ShowForIsland(

	int32 IslandIndex,

	const FString& Name,

	const FString& Transliteration,

	const FVector& InWorldAnchorCm)

{

	ActiveIslandIndex = IslandIndex;

	DisplayName = Name;

	DisplayTransliteration = Transliteration;

	WorldAnchorCm = InWorldAnchorCm;

	VisibleElapsedSec = 0.f;

	FadeAlpha = 0.f;

	SetVisibility(ESlateVisibility::HitTestInvisible);

	Invalidate(EInvalidateWidget::Paint);

}



void UIH_P1C08_IslandCaptionWidget::HideCaption()

{

	ActiveIslandIndex = INDEX_NONE;

	DisplayName.Reset();

	DisplayTransliteration.Reset();

	FadeAlpha = 0.f;

	VisibleElapsedSec = 0.f;

	SetVisibility(ESlateVisibility::Collapsed);

	Invalidate(EInvalidateWidget::Paint);

}



void UIH_P1C08_IslandCaptionWidget::TickCaption(float DeltaTime, APlayerController* PC)

{

	if (ActiveIslandIndex == INDEX_NONE || !PC)

	{

		return;

	}



	VisibleElapsedSec += DeltaTime;

	if (VisibleElapsedSec >= FadeOutStartSec + FadeOutDurationSec)

	{

		HideCaption();

		return;

	}



	if (VisibleElapsedSec < FadeInSec)

	{

		FadeAlpha = FMath::Clamp(VisibleElapsedSec / FadeInSec, 0.f, 1.f);

	}

	else if (VisibleElapsedSec >= FadeOutStartSec)

	{

		const float T = (VisibleElapsedSec - FadeOutStartSec) / FadeOutDurationSec;

		FadeAlpha = 1.f - FMath::Clamp(T, 0.f, 1.f);

	}

	else

	{

		FadeAlpha = 1.f;

	}



	UpdateScreenPosition(PC);

	Invalidate(EInvalidateWidget::Paint);

}



bool UIH_P1C08_IslandCaptionWidget::UpdateScreenPosition(APlayerController* PC)

{

	if (!PC)

	{

		return false;

	}



	FVector2D ViewportPos;

	if (!PC->ProjectWorldLocationToScreen(WorldAnchorCm, ViewportPos, false))

	{

		return false;

	}



	const FVector CamLoc = PC->PlayerCameraManager

		? PC->PlayerCameraManager->GetCameraLocation()

		: (PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : WorldAnchorCm);

	FVector ToCamera = CamLoc - WorldAnchorCm;

	ToCamera.Z = 0.f;

	if (ToCamera.IsNearlyZero(1.f))

	{

		ToCamera = FVector(0.f, -1.f, 0.f);

	}

	ToCamera.Normalize();



	FVector2D AbsolutePos = ViewportPos;

	if (const ULocalPlayer* LP = PC->GetLocalPlayer())

	{

		if (const UGameViewportClient* GVC = LP->ViewportClient)

		{

			if (const TSharedPtr<SViewport> ViewportWidget = GVC->GetGameViewportWidget())

			{

				AbsolutePos = ViewportWidget->GetTickSpaceGeometry().GetAbsolutePosition() + ViewportPos;

			}

		}

	}



	ScreenPosition = AbsolutePos + FVector2D(ToCamera.X, -ToCamera.Y) * SeawardScreenOffsetPx;

	return true;

}



FVector2D UIH_P1C08_IslandCaptionWidget::MeasureOutlinedText(

	const FString& Text, const FSlateFontInfo& Font, bool bItalic) const

{

	if (Text.IsEmpty())

	{

		return FVector2D::ZeroVector;

	}



	FSlateFontInfo MeasureFont = Font;

	MeasureFont.TypefaceFontName = bItalic ? FName(TEXT("Italic")) : FName(TEXT("Regular"));



	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	return FontMeasure->Measure(Text, MeasureFont);

}



void UIH_P1C08_IslandCaptionWidget::DrawOutlinedTextLine(

	FSlateWindowElementList& OutDrawElements,

	int32 LayerId,

	const FGeometry& AllottedGeometry,

	const FString& Text,

	const FVector2D& AbsoluteScreenPos,

	const FSlateFontInfo& Font,

	const FLinearColor& FillColor,

	const FLinearColor& OutlineColor,

	float OutlineSize,

	bool bItalic) const

{

	if (Text.IsEmpty())

	{

		return;

	}



	FSlateFontInfo DrawFont = Font;

	DrawFont.TypefaceFontName = bItalic ? FName(TEXT("Italic")) : FName(TEXT("Regular"));



	const FVector2D LocalPos = AllottedGeometry.AbsoluteToLocal(AbsoluteScreenPos);

	static const TArray<FVector2D> Offsets = {

		FVector2D(-OutlineSize, 0.f), FVector2D(OutlineSize, 0.f),

		FVector2D(0.f, -OutlineSize), FVector2D(0.f, OutlineSize),

		FVector2D(-OutlineSize, -OutlineSize), FVector2D(OutlineSize, -OutlineSize),

		FVector2D(-OutlineSize, OutlineSize), FVector2D(OutlineSize, OutlineSize),

	};



	FLinearColor OutlineWithAlpha = OutlineColor;

	OutlineWithAlpha.A *= FadeAlpha;

	FLinearColor FillWithAlpha = FillColor;

	FillWithAlpha.A *= FadeAlpha;



	for (const FVector2D& Offset : Offsets)

	{

		FSlateDrawElement::MakeText(

			OutDrawElements,

			LayerId,

			AllottedGeometry.ToPaintGeometry(FVector2f(LocalPos + Offset), FVector2f(1.f, 1.f)),

			Text,

			DrawFont,

			ESlateDrawEffect::None,

			OutlineWithAlpha);

	}



	FSlateDrawElement::MakeText(

		OutDrawElements,

		LayerId + 1,

		AllottedGeometry.ToPaintGeometry(FVector2f(LocalPos), FVector2f(1.f, 1.f)),

		Text,

		DrawFont,

		ESlateDrawEffect::None,

		FillWithAlpha);

}



int32 UIH_P1C08_IslandCaptionWidget::NativePaint(

	const FPaintArgs& Args,

	const FGeometry& AllottedGeometry,

	const FSlateRect& MyCullingRect,

	FSlateWindowElementList& OutDrawElements,

	int32 LayerId,

	const FWidgetStyle& InWidgetStyle,

	bool bParentEnabled) const

{

	int32 MaxLayer = Super::NativePaint(

		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);



	if (ActiveIslandIndex == INDEX_NONE || FadeAlpha <= KINDA_SMALL_NUMBER || DisplayName.IsEmpty())

	{

		return MaxLayer;

	}



	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle(

		TEXT("Regular"), IH_P1C08_DevPanelStyle::LabelFontSize + 2);

	const FSlateFontInfo TransFont = FCoreStyle::GetDefaultFontStyle(

		TEXT("Italic"), FMath::Max(8, (IH_P1C08_DevPanelStyle::LabelFontSize + 2) / 2));



	const FLinearColor NameColor = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")));

	const FLinearColor TransColor = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("SecondaryText")));

	const FLinearColor OutlineColor(0.f, 0.f, 0.f, 0.85f);



	const FVector2D NameSize = MeasureOutlinedText(DisplayName, NameFont, false);

	const FVector2D NameDrawPos = ScreenPosition - FVector2D(NameSize.X * 0.5f, 0.f);



	DrawOutlinedTextLine(

		OutDrawElements, MaxLayer + 1, AllottedGeometry, DisplayName, NameDrawPos,

		NameFont, NameColor, OutlineColor, NameOutlinePx, false);



	if (!DisplayTransliteration.IsEmpty())

	{

		const float LineHeight = static_cast<float>(IH_P1C08_DevPanelStyle::LabelFontSize + TransliterationLineGapPx);

		const FVector2D TransSize = MeasureOutlinedText(DisplayTransliteration, TransFont, true);

		const FVector2D TransDrawPos = ScreenPosition + FVector2D(-TransSize.X * 0.5f, LineHeight);



		DrawOutlinedTextLine(

			OutDrawElements, MaxLayer + 1, AllottedGeometry, DisplayTransliteration,

			TransDrawPos, TransFont, TransColor, OutlineColor, TransliterationOutlinePx, true);

	}



	return MaxLayer + 2;

}

