// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_WaterlineOceanAdapter.h"
#include "IH_WB_IslandActor.h"
#include "Components/BoxComponent.h"

namespace
{
	/** Same FFloatProperty/FDoubleProperty-aware pattern established for UDS this session
	 * (IH_WB_Demo004GameMode.cpp) — UE5 Blueprint "Float" variables commonly compile to
	 * FDoubleProperty under Large World Coordinates, not FFloatProperty, so a cast-only-to-float
	 * helper silently no-ops on most Waterline properties too. Duplicated here rather than shared
	 * to keep this adapter self-contained. */
	static bool SetWaterlineFloatProperty(AActor* Actor, const TCHAR* PropertyName, float Value)
	{
		if (!Actor) { return false; }
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(PropertyName));
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			FloatProp->SetPropertyValue_InContainer(Actor, Value);
			return true;
		}
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			DoubleProp->SetPropertyValue_InContainer(Actor, static_cast<double>(Value));
			return true;
		}
		return false;
	}

	/** Same FByteProperty/FEnumProperty-aware pattern established for UDS this session. */
	static bool SetWaterlineByteEnumProperty(AActor* Actor, const TCHAR* PropertyName, uint8 Value)
	{
		if (!Actor) { return false; }
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(PropertyName));
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			ByteProp->SetPropertyValue_InContainer(Actor, Value);
			return true;
		}
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(Actor);
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value));
			return true;
		}
		return false;
	}

	static bool SetWaterlineObjectProperty(AActor* Actor, const TCHAR* PropertyName, UObject* Value)
	{
		if (!Actor) { return false; }
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Actor->GetClass()->FindPropertyByName(FName(PropertyName))))
		{
			ObjProp->SetObjectPropertyValue_InContainer(Actor, Value);
			return true;
		}
		return false;
	}

	static UBoxComponent* GetWaterlineBoxComponent(AActor* Actor, const TCHAR* PropertyName)
	{
		if (!Actor) { return nullptr; }
		if (FObjectProperty* Prop = CastField<FObjectProperty>(Actor->GetClass()->FindPropertyByName(FName(PropertyName))))
		{
			return Cast<UBoxComponent>(Prop->GetObjectPropertyValue_InContainer(Actor));
		}
		return nullptr;
	}

	/** Same ParmsSize-safe ProcessEvent pattern established for UDS this session — a bare
	 * ProcessEvent(Func, nullptr) is unsafe whenever Func->ParmsSize > 0 (common even for
	 * "no-arg" custom events, which can carry a hidden return/local property). Used here to call
	 * BP_Shore_Manager_Gen4's own exposed "Force *" custom events (confirmed real via
	 * BlueprintEditorLibrary.list_events headless introspection) — Mode alone does not appear to
	 * be sufficient to make the shore manager actually perform its capture/generation. */
	static bool CallWaterlineFunction(AActor* Actor, const TCHAR* FunctionName)
	{
		if (!Actor) { return false; }
		UFunction* Func = Actor->FindFunction(FName(FunctionName));
		if (!Func)
		{
			return false;
		}
		if (Func->ParmsSize > 0)
		{
			TArray<uint8> ParamBuffer;
			ParamBuffer.AddZeroed(Func->ParmsSize);
			Actor->ProcessEvent(Func, ParamBuffer.GetData());
		}
		else
		{
			Actor->ProcessEvent(Func, nullptr);
		}
		return true;
	}
}

AIH_WaterlineOceanAdapter::AIH_WaterlineOceanAdapter()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

bool AIH_WaterlineOceanAdapter::InitializeWaterlineOcean()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UClass* OceanClass = StaticLoadClass(AActor::StaticClass(), nullptr,
		TEXT("/Game/Waterline/9_Ocean_Sim_BETA/BP_Waterline_Ocean_Gen_4.BP_Waterline_Ocean_Gen_4_C"));
	if (!OceanClass)
	{
		UE_LOG(LogIH_WB_Demo004, Warning,
			TEXT("Waterline adapter: failed to load BP_Waterline_Ocean_Gen_4 class — falling back to legacy ocean."));
		return false;
	}

	// Deferred spawn: a Blueprint actor's Construction Script and BeginPlay run synchronously
	// inside SpawnActor() once the world has already begun play (true here — we spawn mid-session,
	// well after the world's own BeginPlay). If Waterline's own init logic reads its properties
	// during that pass, a plain SpawnActor()-then-set-properties sequence configures everything
	// too late — the actor already initialized off its unconfigured defaults. SpawnActorDeferred +
	// FinishSpawning() runs our configuration first, Construction Script/BeginPlay after.
	const FTransform SpawnTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	AActor* OceanActor = World->SpawnActorDeferred<AActor>(OceanClass, SpawnTransform);
	if (!OceanActor)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("Waterline adapter: SpawnActorDeferred failed — falling back to legacy ocean."));
		return false;
	}

	// "1 Water Level" confirmed real via headless reflection (already ~0 by vendor default, set
	// explicitly here so canonical sea level Z=0 is never left to an unverified default).
	const bool bWaterLevelSet = SetWaterlineFloatProperty(OceanActor, TEXT("1 Water Level"), 0.f);

	OceanActor->FinishSpawning(SpawnTransform);

	OceanActor->Tags.Add(TEXT("IH.Ocean.Primary"));
	WaterlineOceanInstance = OceanActor;

	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("Waterline adapter: spawned BP_Waterline_Ocean_Gen_4 ('%s'), waterLevelSet=%d. FFT simulation and Enable Ocean confirmed already true by vendor default (verified via headless reflection, not assumed)."),
		*OceanActor->GetName(), bWaterLevelSet ? 1 : 0);

	return true;
}

bool AIH_WaterlineOceanAdapter::SampleOceanSurface(const FVector& WorldLocation, FIHOceanSurfaceSample& OutSample) const
{
	// Phase 5 stub — see FIHOceanSurfaceSample's header comment. Canonical flat fallback only.
	OutSample.Location = FVector(WorldLocation.X, WorldLocation.Y, 0.f);
	OutSample.Normal = FVector::UpVector;
	OutSample.Velocity = FVector::ZeroVector;
	OutSample.DepthCm = 0.f;
	OutSample.bHasDynamicWaves = false;
	return true;
}

void AIH_WaterlineOceanAdapter::SetOceanVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	if (IsValid(WaterlineOceanInstance))
	{
		WaterlineOceanInstance->SetActorHiddenInGame(!bVisible);
	}
	for (const TObjectPtr<AActor>& Shore : ShoreManagerInstances)
	{
		if (IsValid(Shore))
		{
			Shore->SetActorHiddenInGame(!bVisible);
		}
	}
}

void AIH_WaterlineOceanAdapter::SpawnShoreManagersForIslands(const TArray<TObjectPtr<AIH_WB_IslandActor>>& Islands)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(WaterlineOceanInstance))
	{
		return;
	}

	UClass* ShoreClass = StaticLoadClass(AActor::StaticClass(), nullptr,
		TEXT("/Game/Waterline/8_Ocean_Shore/BP_Shore_Manager_Gen4.BP_Shore_Manager_Gen4_C"));
	if (!ShoreClass)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("Waterline adapter: failed to load BP_Shore_Manager_Gen4 class."));
		return;
	}

	int32 SpawnedCount = 0;
	for (const TObjectPtr<AIH_WB_IslandActor>& Island : Islands)
	{
		if (!IsValid(Island))
		{
			continue;
		}

		// Real post-generation geometry, not a guess — same evidence-based approach proven in the
		// IH-DEC-042 spike: GetActorLocation() is the island's true recentered landmass origin,
		// GetMainLandFootprintRadiusCm() a real circumscribing radius from actual generated cells.
		const FVector IslandOrigin = Island->GetActorLocation();
		const float FootprintRadiusCm = Island->GetMainLandFootprintRadiusCm();

		// Deferred spawn (see InitializeWaterlineOcean's comment) — critical here specifically:
		// three prior fix attempts (WaterBody wiring, Capture Volume resize, Mode switch) all
		// produced zero visible change even though every reflection write succeeded, which is the
		// exact signature of the shore manager's own init logic already having run — and skipped,
		// finding WaterBody still None — before our code got a chance to configure it via a plain
		// SpawnActor() call. Configuring before FinishSpawning() fixes that ordering.
		const FTransform ShoreTransform(FRotator::ZeroRotator, FVector(IslandOrigin.X, IslandOrigin.Y, 0.f));
		AActor* ShoreActor = World->SpawnActorDeferred<AActor>(ShoreClass, ShoreTransform);
		if (!ShoreActor)
		{
			continue;
		}

		// Mode: confirmed real enum E_Shore_Manager_Modes via headless reflection —
		// STATIC_MODE=0, DYNAMIC_SET_MODE=1, DYNAMIC_GEN_MODE=2, FULL_DYNAMIC_MODE=3 (vendor
		// default). Kept at FULL_DYNAMIC_MODE for this deferred-spawn retest; revisit STATIC_MODE
		// (lower per-tick cost, matches the accepted Task 3 preference) once dynamic mode is
		// confirmed actually working with correct spawn-time configuration.
		SetWaterlineByteEnumProperty(ShoreActor, TEXT("Mode"), 3);

		const bool bWaterBodySet = SetWaterlineObjectProperty(ShoreActor, TEXT("WaterBody"), WaterlineOceanInstance);

		// Real regression caught via PIE log (2026-08-26): "Capture Volume"/"Trigger Volume" are
		// Blueprint-added components (created by the Blueprint's own Construction Script), NOT
		// native CreateDefaultSubobject components — SpawnActorDeferred() only runs the native
		// constructor, so these components don't exist yet at this point and every resize attempt
		// silently found nothing (confirmed: captureVolumeResized=0/triggerVolumeResized=0 in the
		// log despite the code looking identical to the working pre-deferred-spawn version). Must
		// resize AFTER FinishSpawning() runs the Construction Script, unlike WaterBody/Mode above
		// (plain variable slots that exist regardless of construction phase, so must be set BEFORE
		// FinishSpawning() so the actor's own init logic sees them in time).
		ShoreActor->FinishSpawning(ShoreTransform);

		const float HalfExtentXYCm = FMath::Max(FootprintRadiusCm * 1.3f, 5000.f);

		bool bCaptureVolumeResized = false;
		if (UBoxComponent* CaptureVolume = GetWaterlineBoxComponent(ShoreActor, TEXT("Capture Volume")))
		{
			CaptureVolume->SetBoxExtent(FVector(HalfExtentXYCm, HalfExtentXYCm, 5000.f));
			bCaptureVolumeResized = true;
		}

		// "Trigger Volume" is a SEPARATE BoxComponent (default extent ~1x1x0.3m — confirmed via
		// headless reflection) never touched by prior attempts. Its name strongly implies a
		// proximity-based activation gate (common LOD/perf pattern: only run shore effects near
		// the camera) — left at its comically tiny default, shore effects may simply never
		// activate regardless of every other setting being correct. Resized to match.
		bool bTriggerVolumeResized = false;
		if (UBoxComponent* TriggerVolume = GetWaterlineBoxComponent(ShoreActor, TEXT("Trigger Volume")))
		{
			TriggerVolume->SetBoxExtent(FVector(HalfExtentXYCm, HalfExtentXYCm, 5000.f));
			bTriggerVolumeResized = true;
		}

		// Real, non-blind fix candidate: headless BlueprintEditorLibrary graph introspection
		// (list_events) confirmed three real, implemented custom events on this Blueprint —
		// "Force Update", "Force Transfer", "Full Dynamic Gen" — clearly designer-exposed manual
		// trigger points (their names, not a guess). ReceiveTick is confirmed NOT implemented, so
		// nothing per-frame is driving generation on its own; whatever periodic mechanism exists
		// (if any) is internal/Timer-based, not something Mode alone visibly kicks off. Called
		// once, after every property/component is correctly configured, so if these are the real
		// "run the capture now" entry points, they fire with correct data already in place.
		const bool bForceUpdateFound = CallWaterlineFunction(ShoreActor, TEXT("Force Update"));
		const bool bForceTransferFound = CallWaterlineFunction(ShoreActor, TEXT("Force Transfer"));
		const bool bFullDynamicGenFound = CallWaterlineFunction(ShoreActor, TEXT("Full Dynamic Gen"));

		ShoreActor->Tags.Add(TEXT("IH.Ocean.Shore"));
		ShoreManagerInstances.Add(ShoreActor);
		++SpawnedCount;

		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Waterline adapter: Shore Manager spawned for island '%s' at (%.0f,%.0f), footprintRadiusCm=%.0f, waterBodySet=%d, captureVolumeResized=%d, triggerVolumeResized=%d, forceUpdate=%d, forceTransfer=%d, fullDynamicGen=%d."),
			*Island->GetName(), IslandOrigin.X, IslandOrigin.Y, FootprintRadiusCm, bWaterBodySet ? 1 : 0, bCaptureVolumeResized ? 1 : 0, bTriggerVolumeResized ? 1 : 0,
			bForceUpdateFound ? 1 : 0, bForceTransferFound ? 1 : 0, bFullDynamicGenFound ? 1 : 0);
	}

	UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter: spawned %d Shore Manager(s) for %d island(s)."),
		SpawnedCount, Islands.Num());
}
