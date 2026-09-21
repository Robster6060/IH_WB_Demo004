// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_MannequinRegistrySubsystem.h"
#include "IH_P1C07_ShipFormation.h"
#include "IH_P1C08_MoveDestinationFlag.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// Human-scale troop spacing for formation offsets - the ship system's own BerthSpacingCm (40m,
	// tuned for ~36m hulls) would spread infantry absurdly far apart.
	static constexpr float TroopFormationSpacingCm = 200.f;
}

void UIH_P1C08_MannequinRegistrySubsystem::RegisterMannequin(AActor* Mannequin)
{
	if (!Mannequin)
	{
		return;
	}
	RegisteredMannequins.AddUnique(Mannequin);
}

void UIH_P1C08_MannequinRegistrySubsystem::UnregisterMannequin(AActor* Mannequin)
{
	if (!Mannequin)
	{
		return;
	}
	RegisteredMannequins.Remove(Mannequin);
	SelectedMannequins.Remove(Mannequin);
}

TArray<AActor*> UIH_P1C08_MannequinRegistrySubsystem::GetSelectedMannequins() const
{
	TArray<AActor*> Out;
	for (const TWeakObjectPtr<AActor>& Ptr : SelectedMannequins)
	{
		if (Ptr.IsValid())
		{
			Out.Add(Ptr.Get());
		}
	}
	return Out;
}

void UIH_P1C08_MannequinRegistrySubsystem::ClearSelection()
{
	for (const TWeakObjectPtr<AActor>& Ptr : SelectedMannequins)
	{
		if (AActor* Mannequin = Ptr.Get())
		{
			if (IIH_P1C08_SelectableMannequin* Sel = Cast<IIH_P1C08_SelectableMannequin>(Mannequin))
			{
				Sel->SetMannequinSelected(false);
			}
		}
	}
	SelectedMannequins.Reset();
}

void UIH_P1C08_MannequinRegistrySubsystem::ToggleSelectMannequin(AActor* Mannequin)
{
	if (!Mannequin || !Mannequin->Implements<UIH_P1C08_SelectableMannequin>())
	{
		return;
	}

	if (SelectedMannequins.Contains(Mannequin))
	{
		SelectedMannequins.Remove(Mannequin);
		if (IIH_P1C08_SelectableMannequin* Sel = Cast<IIH_P1C08_SelectableMannequin>(Mannequin))
		{
			Sel->SetMannequinSelected(false);
		}
	}
	else
	{
		SelectedMannequins.Add(Mannequin);
		if (IIH_P1C08_SelectableMannequin* Sel = Cast<IIH_P1C08_SelectableMannequin>(Mannequin))
		{
			Sel->SetMannequinSelected(true);
		}
	}
}

void UIH_P1C08_MannequinRegistrySubsystem::SetSelection(const TArray<AActor*>& Mannequins)
{
	ClearSelection();
	for (AActor* Mannequin : Mannequins)
	{
		if (Mannequin && Mannequin->Implements<UIH_P1C08_SelectableMannequin>())
		{
			SelectedMannequins.Add(Mannequin);
			if (IIH_P1C08_SelectableMannequin* Sel = Cast<IIH_P1C08_SelectableMannequin>(Mannequin))
			{
				Sel->SetMannequinSelected(true);
			}
		}
	}
}

void UIH_P1C08_MannequinRegistrySubsystem::SelectMannequinsInScreenRect(
	APlayerController* PC,
	const FVector2D& ScreenA,
	const FVector2D& ScreenB,
	const bool bAdditive)
{
	if (!PC)
	{
		return;
	}

	const float MinX = FMath::Min(ScreenA.X, ScreenB.X);
	const float MaxX = FMath::Max(ScreenA.X, ScreenB.X);
	const float MinY = FMath::Min(ScreenA.Y, ScreenB.Y);
	const float MaxY = FMath::Max(ScreenA.Y, ScreenB.Y);

	TArray<AActor*> Boxed;
	for (const TWeakObjectPtr<AActor>& Ptr : RegisteredMannequins)
	{
		AActor* Mannequin = Ptr.Get();
		if (!Mannequin || !Mannequin->Implements<UIH_P1C08_SelectableMannequin>())
		{
			continue;
		}

		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(Mannequin->GetActorLocation(), ScreenPos, true))
		{
			continue;
		}

		if (ScreenPos.X >= MinX && ScreenPos.X <= MaxX && ScreenPos.Y >= MinY && ScreenPos.Y <= MaxY)
		{
			Boxed.Add(Mannequin);
		}
	}

	if (!bAdditive)
	{
		SetSelection(Boxed);
		return;
	}

	for (AActor* Mannequin : Boxed)
	{
		if (!IsMannequinSelected(Mannequin))
		{
			SelectedMannequins.Add(Mannequin);
			if (IIH_P1C08_SelectableMannequin* Sel = Cast<IIH_P1C08_SelectableMannequin>(Mannequin))
			{
				Sel->SetMannequinSelected(true);
			}
		}
	}
}

bool UIH_P1C08_MannequinRegistrySubsystem::IsMannequinSelected(AActor* Mannequin) const
{
	return Mannequin && SelectedMannequins.Contains(Mannequin);
}

void UIH_P1C08_MannequinRegistrySubsystem::PruneActiveDestinationFlags()
{
	ActiveDestinationFlags.RemoveAll([](const TWeakObjectPtr<AIH_P1C08_MoveDestinationFlag>& Ptr) {
		return !Ptr.IsValid();
	});
}

TArray<AIH_P1C08_MoveDestinationFlag*> UIH_P1C08_MannequinRegistrySubsystem::GetActiveDestinationFlags()
{
	PruneActiveDestinationFlags();
	TArray<AIH_P1C08_MoveDestinationFlag*> Out;
	Out.Reserve(ActiveDestinationFlags.Num());
	for (const TWeakObjectPtr<AIH_P1C08_MoveDestinationFlag>& Ptr : ActiveDestinationFlags)
	{
		if (AIH_P1C08_MoveDestinationFlag* Flag = Ptr.Get())
		{
			Out.Add(Flag);
		}
	}
	return Out;
}

void UIH_P1C08_MannequinRegistrySubsystem::RemoveMannequinsFromActiveFlags(const TArray<AActor*>& Mannequins)
{
	for (const TWeakObjectPtr<AIH_P1C08_MoveDestinationFlag>& Ptr : ActiveDestinationFlags)
	{
		if (AIH_P1C08_MoveDestinationFlag* Flag = Ptr.Get())
		{
			Flag->RemoveTrackedMannequins(Mannequins);
		}
	}
	PruneActiveDestinationFlags();
}

bool UIH_P1C08_MannequinRegistrySubsystem::IssueMoveOrderToSelection(
	APlayerController* PC, const FVector& LandClickWorld, const bool bAppendWaypoint)
{
	TArray<AActor*> Selected = GetSelectedMannequins();
	if (Selected.Num() == 0 || !PC)
	{
		return false;
	}

	UWorld* World = PC->GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<TScriptInterface<IIH_P1C08_SelectableMannequin>> Ifaces;
	Ifaces.Reserve(Selected.Num());

	// 2026-09-13: reuses the ship fleet's own IH_P1C07Formation::ComputeBerthOffset (line-abreast +
	// slight column stagger) instead of every selected Mannequin walking to the exact same point -
	// per explicit user request, now that troop movement itself is confirmed working. The ship
	// default spacing (BerthSpacingCm, tuned for ~36m hulls) would spread human-scale troops
	// absurdly far apart, so this passes its own much smaller spacing instead. Approach direction is
	// derived from the selection's current average position toward the click point, since (unlike
	// the ship version) no explicit heading is supplied by the caller.
	const int32 Total = Selected.Num();
	FVector AverageOrigin = FVector::ZeroVector;
	int32 OriginSamples = 0;
	for (AActor* Mannequin : Selected)
	{
		if (const IIH_P1C08_SelectableMannequin* Sel = Cast<IIH_P1C08_SelectableMannequin>(Mannequin))
		{
			AverageOrigin += Sel->GetMannequinFeetLocation();
			++OriginSamples;
		}
	}
	if (OriginSamples > 0)
	{
		AverageOrigin /= static_cast<float>(OriginSamples);
	}
	FVector ApproachDir = (LandClickWorld - AverageOrigin).GetSafeNormal2D();
	if (ApproachDir.IsNearlyZero())
	{
		ApproachDir = FVector::ForwardVector;
	}

	int32 Idx = 0;
	for (AActor* Mannequin : Selected)
	{
		IIH_P1C08_SelectableMannequin* Sel = Cast<IIH_P1C08_SelectableMannequin>(Mannequin);
		if (!Sel)
		{
			++Idx;
			continue;
		}

		const FVector Offset = IH_P1C07Formation::ComputeBerthOffset(Idx, Total, ApproachDir, TroopFormationSpacingCm);
		// 2026-09-13: breadcrumb waypoint chain - a fresh (non-appended) order replaces whatever
		// chain was queued and walks there immediately, same as before; Shift+click instead queues
		// this point behind whatever leg is already in progress (EnqueueWalkWaypoint itself still
		// walks immediately if this Mannequin happened to be idle).
		if (!bAppendWaypoint)
		{
			Sel->ClearWaypointQueue();
			Sel->CommandWalkTo(LandClickWorld + Offset);
		}
		else
		{
			Sel->EnqueueWalkWaypoint(LandClickWorld + Offset);
		}
		++Idx;

		TScriptInterface<IIH_P1C08_SelectableMannequin> Entry;
		Entry.SetObject(Mannequin);
		Entry.SetInterface(Sel);
		Ifaces.Add(Entry);
	}

	if (Ifaces.Num() == 0)
	{
		return false;
	}

	if (!bAppendWaypoint)
	{
		RemoveMannequinsFromActiveFlags(Selected);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AIH_P1C08_MoveDestinationFlag* Flag = World->SpawnActor<AIH_P1C08_MoveDestinationFlag>(
			AIH_P1C08_MoveDestinationFlag::StaticClass(), LandClickWorld, FRotator::ZeroRotator, Params))
	{
		Flag->InitOrder(LandClickWorld, Ifaces);
		ActiveDestinationFlags.Add(Flag);
	}

	return true;
}
