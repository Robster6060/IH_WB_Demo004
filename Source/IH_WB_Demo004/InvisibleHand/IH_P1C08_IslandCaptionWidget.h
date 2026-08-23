// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "Blueprint/UserWidget.h"

#include "IH_P1C08_IslandCaptionWidget.generated.h"



/** Screen-space island name + transliteration caption (Phase 5d). */

UCLASS()

class IH_WB_DEMO004_API UIH_P1C08_IslandCaptionWidget : public UUserWidget

{

	GENERATED_BODY()



public:

	void ShowForIsland(int32 IslandIndex, const FString& Name, const FString& Transliteration, const FVector& WorldAnchorCm);

	void HideCaption();

	void TickCaption(float DeltaTime, class APlayerController* PC);



protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual int32 NativePaint(

		const FPaintArgs& Args,

		const FGeometry& AllottedGeometry,

		const FSlateRect& MyCullingRect,

		FSlateWindowElementList& OutDrawElements,

		int32 LayerId,

		const FWidgetStyle& InWidgetStyle,

		bool bParentEnabled) const override;



private:

	void DrawOutlinedTextLine(

		FSlateWindowElementList& OutDrawElements,

		int32 LayerId,

		const FGeometry& AllottedGeometry,

		const FString& Text,

		const FVector2D& AbsoluteScreenPos,

		const FSlateFontInfo& Font,

		const FLinearColor& FillColor,

		const FLinearColor& OutlineColor,

		float OutlineSize,

		bool bItalic) const;



	bool UpdateScreenPosition(APlayerController* PC);

	FVector2D MeasureOutlinedText(const FString& Text, const FSlateFontInfo& Font, bool bItalic) const;



	FString DisplayName;

	FString DisplayTransliteration;

	FVector WorldAnchorCm = FVector::ZeroVector;

	FVector2D ScreenPosition = FVector2D::ZeroVector;

	float FadeAlpha = 0.f;

	float VisibleElapsedSec = 0.f;

	int32 ActiveIslandIndex = INDEX_NONE;



	static constexpr float FadeInSec = 0.35f;

	static constexpr float FadeOutStartSec = 30.f;

	static constexpr float FadeOutDurationSec = 1.5f;

	/** Screen-space push toward open water / camera (was 48; +100 seaward). */
	static constexpr float SeawardScreenOffsetPx = 148.f;

	/** Vertical gap from name baseline to transliteration (LabelFontSize + gap; was +6, +10). */
	static constexpr float TransliterationLineGapPx = 16.f;

	static constexpr float NameOutlinePx = 2.f;

	static constexpr float TransliterationOutlinePx = 1.f;

};

