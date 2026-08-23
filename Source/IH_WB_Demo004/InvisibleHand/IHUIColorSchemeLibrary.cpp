// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHUIColorSchemeLibrary.h"
#include "Components/Slider.h"
#include "Math/Color.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace IHUIColorSchemePrivate
{
	static TMap<FName, FLinearColor> BuildHUDStartingScheme()
	{
		auto H = [](const TCHAR* Hex) -> FLinearColor {
			return FLinearColor::FromSRGBColor(FColor::FromHex(FString(Hex)));
		};
		TMap<FName, FLinearColor> M;
		M.Add(FName(TEXT("PrimaryText")), H(TEXT("FFF8DC")));
		M.Add(FName(TEXT("SecondaryText")), H(TEXT("DEB887")));
		M.Add(FName(TEXT("HeadingText")), H(TEXT("DA8A3D")));
		M.Add(FName(TEXT("BodyText")), H(TEXT("D2B48C")));
		M.Add(FName(TEXT("DisabledText")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("ErrorWarningText")), H(TEXT("CD5C5C")));
		M.Add(FName(TEXT("SuccessText")), H(TEXT("6B8E23")));
		M.Add(FName(TEXT("LinkText")), H(TEXT("5F9EA0")));
		M.Add(FName(TEXT("RealmSeedCaption")), H(TEXT("020510")));
		M.Add(FName(TEXT("CanvasPanelBackgroundTint")), H(TEXT("3E2723")));
		M.Add(FName(TEXT("BoxBackgroundTint")), H(TEXT("6B5B52")));
		M.Add(FName(TEXT("PanelBackground")), H(TEXT("5A5242")));
		M.Add(FName(TEXT("ModalDialogBackground")), H(TEXT("4A3C2E")));
		M.Add(FName(TEXT("TooltipBackground")), H(TEXT("2E241E")));
		M.Add(FName(TEXT("InputFieldBackground")), H(TEXT("6B5B4A")));
		M.Add(FName(TEXT("ProgressBarBackground")), H(TEXT("4A3C2E")));
		M.Add(FName(TEXT("ScrollbarBackground")), H(TEXT("5A4A3C")));
		M.Add(FName(TEXT("BoxOutlineTint")), H(TEXT("A0826D")));
		M.Add(FName(TEXT("ButtonOutline")), H(TEXT("BC9A6A")));
		M.Add(FName(TEXT("InputFieldBorder")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("BorderFrameTint")), H(TEXT("9A8B7A")));
		M.Add(FName(TEXT("SelectedBorder")), H(TEXT("DAA520")));
		M.Add(FName(TEXT("FocusBorder")), H(TEXT("F4A460")));
		M.Add(FName(TEXT("ButtonNormalTint")), H(TEXT("6B5B52")));
		M.Add(FName(TEXT("ButtonHoveredTint")), H(TEXT("7B6B5A")));
		M.Add(FName(TEXT("ButtonPressedTint")), H(TEXT("5A4A3C")));
		M.Add(FName(TEXT("ButtonDisabledTint")), H(TEXT("5A4A3C")));
		M.Add(FName(TEXT("ButtonNormalText")), H(TEXT("FFF8DC")));
		M.Add(FName(TEXT("ButtonHoveredText")), H(TEXT("FFFFFF")));
		M.Add(FName(TEXT("ButtonPressedText")), H(TEXT("DEB887")));
		M.Add(FName(TEXT("ButtonDisabledText")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("PrimaryButtonTint")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("SecondaryButtonTint")), H(TEXT("7B6B5A")));
		M.Add(FName(TEXT("SuccessButtonTint")), H(TEXT("6B8E23")));
		M.Add(FName(TEXT("DangerButtonTint")), H(TEXT("CD5C5C")));
		M.Add(FName(TEXT("ProgressBarFill")), H(TEXT("DAA520")));
		M.Add(FName(TEXT("HealthBarFill")), H(TEXT("CD5C5C")));
		M.Add(FName(TEXT("HealthBarBackground")), H(TEXT("4A2F2F")));
		M.Add(FName(TEXT("ManaEnergyFill")), H(TEXT("5F9EA0")));
		M.Add(FName(TEXT("ExperienceFill")), H(TEXT("6B8E23")));
		M.Add(FName(TEXT("ScrollbarThumbNormal")), H(TEXT("9A8B7A")));
		M.Add(FName(TEXT("ScrollbarThumbHovered")), H(TEXT("BC9A6A")));
		M.Add(FName(TEXT("ScrollbarThumbPressed")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("CheckboxBackground")), H(TEXT("6B5B4A")));
		M.Add(FName(TEXT("CheckboxChecked")), H(TEXT("DAA520")));
		M.Add(FName(TEXT("CheckboxBorder")), H(TEXT("A0826D")));
		M.Add(FName(TEXT("SliderTrackBackground")), H(TEXT("5A4A3C")));
		M.Add(FName(TEXT("SliderTrackFill")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("SliderThumbNormal")), H(TEXT("BC9A6A")));
		M.Add(FName(TEXT("SliderThumbHovered")), H(TEXT("DA8A3D")));
		M.Add(FName(TEXT("ShadowDropShadow")), H(TEXT("000000")));
		M.Add(FName(TEXT("GlowEffect")), H(TEXT("DAA520")));
		M.Add(FName(TEXT("SelectionHighlight")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("HoverHighlight")), H(TEXT("9A8B7A")));
		M.Add(FName(TEXT("HeaderTitleBackground")), H(TEXT("6B5B4A")));
		M.Add(FName(TEXT("HeaderTitleText")), H(TEXT("DAA520")));
		M.Add(FName(TEXT("DividerSeparator")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("TooltipText")), H(TEXT("FFFFFF")));
		M.Add(FName(TEXT("NotificationBackground")), H(TEXT("5A4A3C")));
		M.Add(FName(TEXT("NotificationText")), H(TEXT("FFF8DC")));
		M.Add(FName(TEXT("TabNormalBackground")), H(TEXT("6B5B4A")));
		M.Add(FName(TEXT("TabActiveBackground")), H(TEXT("7B6B5A")));
		M.Add(FName(TEXT("TabHoveredBackground")), H(TEXT("8B7355")));
		M.Add(FName(TEXT("TabText")), H(TEXT("DEB887")));
		M.Add(FName(TEXT("TabActiveText")), H(TEXT("FFF8DC")));
		return M;
	}

	static const TMap<FName, FLinearColor>& GetMap()
	{
		static TMap<FName, FLinearColor> Map = BuildHUDStartingScheme();
		return Map;
	}
}

FLinearColor UIHUIColorSchemeLibrary::GetHUDStartingColor(FName RoleKey)
{
	const TMap<FName, FLinearColor>& M = IHUIColorSchemePrivate::GetMap();
	if (const FLinearColor* Found = M.Find(RoleKey))
	{
		return *Found;
	}
	return FLinearColor::White;
}

FLinearColor UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(FName RoleKey, float Alpha)
{
	FLinearColor C = GetHUDStartingColor(RoleKey);
	C.A = Alpha;
	return C;
}

FLinearColor UIHUIColorSchemeLibrary::ScaleRGB(FLinearColor C, float Scale, bool bScaleAlpha)
{
	C.R *= Scale;
	C.G *= Scale;
	C.B *= Scale;
	if (bScaleAlpha)
	{
		C.A *= Scale;
	}
	return C;
}

void UIHUIColorSchemeLibrary::ApplyHUDSliderStyle(USlider* Slider)
{
	if (!Slider)
	{
		return;
	}

	const FLinearColor TrackBg = GetHUDStartingColor(FName(TEXT("SliderTrackBackground")));
	const FLinearColor TrackFill = GetHUDStartingColor(FName(TEXT("SliderTrackFill")));
	const FLinearColor ThumbNormal = GetHUDStartingColor(FName(TEXT("SliderThumbNormal")));
	const FLinearColor ThumbHovered = GetHUDStartingColor(FName(TEXT("SliderThumbHovered")));

	FSliderStyle Style = Slider->GetWidgetStyle();
	Style.NormalBarImage.TintColor = FSlateColor(TrackBg);
	Style.HoveredBarImage.TintColor = FSlateColor(TrackBg);
	Style.DisabledBarImage.TintColor = FSlateColor(ScaleRGB(TrackBg, 0.55f));
	Style.NormalThumbImage.TintColor = FSlateColor(ThumbNormal);
	Style.HoveredThumbImage.TintColor = FSlateColor(ThumbHovered);
	Style.DisabledThumbImage.TintColor = FSlateColor(ScaleRGB(ThumbNormal, 0.55f));
	Slider->SetWidgetStyle(Style);
	Slider->SetSliderBarColor(TrackFill);
}
