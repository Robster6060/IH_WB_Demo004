// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_SelectionLassoWidget.h"
#include "Rendering/DrawElements.h"

void UIH_P1C07_SelectionLassoWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetPositionInViewport(FVector2D::ZeroVector);
}

void UIH_P1C07_SelectionLassoWidget::SetDragRect(const FVector2D& InStart, const FVector2D& InEnd, bool bInActive)
{
	DragStart = InStart;
	DragEnd = InEnd;
	bDragActive = bInActive;
	if (bDragActive)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		Invalidate(EInvalidateWidget::Paint);
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

int32 UIH_P1C07_SelectionLassoWidget::NativePaint(
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

	if (!bDragActive)
	{
		return MaxLayer;
	}

	const FVector2D LocalStart = AllottedGeometry.AbsoluteToLocal(DragStart);
	const FVector2D LocalEnd = AllottedGeometry.AbsoluteToLocal(DragEnd);

	const float MinX = FMath::Min(LocalStart.X, LocalEnd.X);
	const float MinY = FMath::Min(LocalStart.Y, LocalEnd.Y);
	const float MaxX = FMath::Max(LocalStart.X, LocalEnd.X);
	const float MaxY = FMath::Max(LocalStart.Y, LocalEnd.Y);
	const FVector2D Size(MaxX - MinX, MaxY - MinY);
	if (Size.X < 1.f && Size.Y < 1.f)
	{
		return MaxLayer;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2f TopLeft(MinX, MinY);
	const FVector2f BoxSize(Size.X, Size.Y);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MaxLayer + 1,
		AllottedGeometry.ToPaintGeometry(TopLeft, BoxSize),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.12f, 0.78f, 1.f, 0.14f));

	const float Thickness = 2.f;
	const FLinearColor BorderColor(0.35f, 0.92f, 1.f, 0.95f);

	auto DrawLine = [&](const FVector2D& A, const FVector2D& B) {
		TArray<FVector2D> Points;
		Points.Add(A);
		Points.Add(B);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MaxLayer + 2,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			BorderColor,
			true,
			Thickness);
	};

	const FVector2D TL(MinX, MinY);
	const FVector2D TR(MaxX, MinY);
	const FVector2D BL(MinX, MaxY);
	const FVector2D BR(MaxX, MaxY);
	DrawLine(TL, TR);
	DrawLine(TR, BR);
	DrawLine(BR, BL);
	DrawLine(BL, TL);

	return MaxLayer + 2;
}
