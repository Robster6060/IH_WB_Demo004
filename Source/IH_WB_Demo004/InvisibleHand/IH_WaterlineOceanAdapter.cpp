// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_WaterlineOceanAdapter.h"
#include "IH_WB_IslandActor.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"

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

	/** Int-typed properties (e.g. "Resolution"/"Capture Size") may compile as FIntProperty or, under
	 * Large World Coordinates, as a float/double slot — same defensive multi-cast pattern as above. */
	static bool SetWaterlineIntProperty(AActor* Actor, const TCHAR* PropertyName, int32 Value)
	{
		if (!Actor) { return false; }
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(PropertyName));
		if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		{
			IntProp->SetPropertyValue_InContainer(Actor, Value);
			return true;
		}
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			FloatProp->SetPropertyValue_InContainer(Actor, static_cast<float>(Value));
			return true;
		}
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			DoubleProp->SetPropertyValue_InContainer(Actor, static_cast<double>(Value));
			return true;
		}
		return false;
	}

	/** Appends an actor reference to an object-array property (e.g. "Capture Actors"), found
	 * empty (0 elements) via direct GUI inspection during PIE — RT Shore Final/RT JFA 1 stayed
	 * 100% black through repeated Force Update calls, and an empty "what am I allowed to render"
	 * list would explain permanent black regardless of position/timing/resolution. */
	static bool AddActorToWaterlineActorArrayProperty(AActor* Actor, const TCHAR* PropertyName, AActor* ToAdd)
	{
		if (!Actor || !ToAdd) { return false; }
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Actor->GetClass()->FindPropertyByName(FName(PropertyName)));
		if (!ArrayProp) { return false; }
		FObjectProperty* InnerObjProp = CastField<FObjectProperty>(ArrayProp->Inner);
		if (!InnerObjProp) { return false; }
		void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Actor);
		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
		const int32 NewIndex = ArrayHelper.AddValue();
		InnerObjProp->SetObjectPropertyValue(ArrayHelper.GetRawPtr(NewIndex), ToAdd);
		return true;
	}

	/** FBoolProperty::SetPropertyValue_InContainer already handles UE's packed-bitfield bool
	 * representation correctly, so this doesn't need the multi-cast fallback pattern above. */
	static bool SetWaterlineBoolProperty(AActor* Actor, const TCHAR* PropertyName, bool Value)
	{
		if (!Actor) { return false; }
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Actor->GetClass()->FindPropertyByName(FName(PropertyName))))
		{
			BoolProp->SetPropertyValue_InContainer(Actor, Value);
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

	/** Reads a Shore Manager's render-target property (e.g. "RT Shore Final") and logs whether it
	 * contains any real, non-black pixel data — a direct, material/visibility-independent way to
	 * tell whether the capture pipeline is actually producing anything, after "Debug Canvas" (the
	 * vendor's own intended visualization aid) turned out to be hidden (bVisible=False) and still
	 * carrying the placeholder /Engine/BasicShapeMaterial rather than any real debug material. */
	static void LogWaterlineRenderTargetStats(AActor* Actor, const TCHAR* PropertyName)
	{
		if (!Actor) { return; }
		FObjectProperty* Prop = CastField<FObjectProperty>(Actor->GetClass()->FindPropertyByName(FName(PropertyName)));
		if (!Prop)
		{
			UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: property '%s' not found on '%s'."), PropertyName, *Actor->GetName());
			return;
		}
		UTextureRenderTarget2D* RT = Cast<UTextureRenderTarget2D>(Prop->GetObjectPropertyValue_InContainer(Actor));
		if (!RT)
		{
			UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: '%s' on '%s' is None."), PropertyName, *Actor->GetName());
			return;
		}
		FTextureRenderTargetResource* RTResource = RT->GameThread_GetRenderTargetResource();
		if (!RTResource)
		{
			UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: '%s' on '%s' (%dx%d) has no render resource yet."),
				PropertyName, *Actor->GetName(), RT->SizeX, RT->SizeY);
			return;
		}
		TArray<FColor> Pixels;
		if (!RTResource->ReadPixels(Pixels) || Pixels.Num() == 0)
		{
			UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: '%s' on '%s' (%dx%d) ReadPixels failed/empty."),
				PropertyName, *Actor->GetName(), RT->SizeX, RT->SizeY);
			return;
		}
		int64 NonBlackCount = 0;
		int64 SumR = 0, SumG = 0, SumB = 0, SumA = 0;
		uint8 MaxChannel = 0;
		for (const FColor& Px : Pixels)
		{
			if (Px.R != 0 || Px.G != 0 || Px.B != 0)
			{
				++NonBlackCount;
			}
			SumR += Px.R; SumG += Px.G; SumB += Px.B; SumA += Px.A;
			MaxChannel = FMath::Max(MaxChannel, FMath::Max3(Px.R, Px.G, Px.B));
		}
		const double InvCount = 1.0 / Pixels.Num();
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Waterline adapter DIAG: '%s' on '%s' (%dx%d): nonBlackPixels=%lld/%d (%.1f%%), avgRGBA=(%.1f,%.1f,%.1f,%.1f), maxChannel=%d."),
			PropertyName, *Actor->GetName(), RT->SizeX, RT->SizeY, NonBlackCount, Pixels.Num(),
			100.0 * NonBlackCount * InvCount, SumR * InvCount, SumG * InvCount, SumB * InvCount, SumA * InvCount, MaxChannel);
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
	// Tick only drives the Ocean_POV diagnostic log below — Waterline's own simulation runs itself.
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
}

void AIH_WaterlineOceanAdapter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// DEV diagnostic: BP_Shore_Manager_Gen4's "Full Dynamic Gen" event repositions itself every
	// timer tick via Set Actor Location, fed by a Get Components by Tag(Sphere Collision,
	// "Ocean_POV") search. Logging each instance's live location vs. where we spawned it proves,
	// from the Output Log alone, whether that reposition is actually firing and where it lands —
	// no manual World Outliner hunting needed. Safe to remove once the Ocean_POV theory is settled.
	ShoreManagerDiagnosticLogAccumSec += DeltaTime;
	if (ShoreManagerDiagnosticLogAccumSec < 3.f)
	{
		return;
	}
	ShoreManagerDiagnosticLogAccumSec = 0.f;

	// Direct check, not a guess: enumerate every component in the world actually tagged "Ocean_POV"
	// right now, and where it is — proves whether Get Components by Tag is really resolving to our
	// tagged Red Cube sphere, or to something else entirely.
	int32 OceanPovComponentCount = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}
			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (Comp && Comp->ComponentHasTag(FName(TEXT("Ocean_POV"))))
				{
					++OceanPovComponentCount;
					if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
					{
						const FVector Loc = SceneComp->GetComponentLocation();
						UE_LOG(LogIH_WB_Demo004, Log,
							TEXT("Waterline adapter DIAG: Ocean_POV component #%d on actor '%s' at world (%.0f,%.0f,%.0f)."),
							OceanPovComponentCount, *Actor->GetName(), Loc.X, Loc.Y, Loc.Z);
					}
				}
			}
		}
		if (OceanPovComponentCount == 0)
		{
			UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: no component in the world is currently tagged Ocean_POV."));
		}

		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				const FVector PawnLoc = Pawn->GetActorLocation();
				UE_LOG(LogIH_WB_Demo004, Log,
					TEXT("Waterline adapter DIAG: player pawn '%s' at world (%.0f,%.0f,%.0f)."),
					*Pawn->GetName(), PawnLoc.X, PawnLoc.Y, PawnLoc.Z);
			}
		}
	}

	for (int32 Index = 0; Index < ShoreManagerInstances.Num(); ++Index)
	{
		AActor* Shore = ShoreManagerInstances[Index];
		if (!IsValid(Shore))
		{
			continue;
		}
		const FVector SpawnOrigin = ShoreManagerSpawnOrigins.IsValidIndex(Index) ? ShoreManagerSpawnOrigins[Index] : FVector::ZeroVector;
		const FVector CurrentLoc = Shore->GetActorLocation();
		const float DriftCm = FVector::Dist(SpawnOrigin, CurrentLoc);
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Waterline adapter DIAG: Shore Manager '%s' spawnOrigin=(%.0f,%.0f,%.0f) currentLoc=(%.0f,%.0f,%.0f) driftCm=%.0f."),
			*Shore->GetName(), SpawnOrigin.X, SpawnOrigin.Y, SpawnOrigin.Z, CurrentLoc.X, CurrentLoc.Y, CurrentLoc.Z, DriftCm);

		// RT Shore Final/RT JFA 1 stayed 100% black for a full PIE session with "Force Update"
		// called only once at spawn. Its "Capture Scene" call sits behind Branch(Shore Warmup) —
		// the first call finds Shore Warmup false, skips the capture, and only arms a 1s Delay
		// before setting Shore Warmup=true; the real Capture Scene only runs on a SECOND call made
		// after that delay. Re-calling it here every 3s tests that directly instead of guessing.
		CallWaterlineFunction(Shore, TEXT("Force Update"));

		// RT Shore Final/RT JFA 1 now confirmed producing real data (100% non-black, real JFA
		// distance-field signal) after the Capture Actors fix — yet the visible ocean surface still
		// shows no shore break. Per the Blueprint graph, "Force Transfer" is the event that pushes
		// captured texture data into the ocean material's Water Surface DYN/Post-Process DYN
		// parameters. It was only ever called once, at spawn — before Capture Actors was populated
		// and before the capture was producing anything — so it likely locked in empty/stale data
		// and never got a second chance to push the real data through. Same fix pattern as Force
		// Update's Warmup gate: re-call periodically instead of once.
		CallWaterlineFunction(Shore, TEXT("Force Transfer"));

		// ReadPixels flushes the GPU — only the first instance each cycle, not all three, to keep
		// this DEV diagnostic cheap. Checks both render targets named in "Force Update"'s Clear/Resize
		// calls (confirmed real property names via headless reflection this session).
		if (Index == 0)
		{
			LogWaterlineRenderTargetStats(Shore, TEXT("RT Shore Final"));
			LogWaterlineRenderTargetStats(Shore, TEXT("RT JFA 1"));
		}
	}
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

	// Found via direct GUI inspection during PIE: the Ocean's own "Water Simulation" category
	// ("Enable Shallow Water Sim" already true by vendor default) also exposes "Dynamic Foam"
	// (unchecked) and "Dynamic Foam Mode" (explicitly "No Dynamic Foam") — a directly-named,
	// currently-disabled toggle for the visible foam/wave-break effect, distinct from the Shore
	// Manager's JFA capture pipeline this session has otherwise fully repaired (capture confirmed
	// producing real data, transferred repeatedly) with zero visible change at the coastline.
	const bool bDynamicFoamSet = SetWaterlineBoolProperty(OceanActor, TEXT("Dynamic Foam"), true);
	const bool bDynamicFoamModeSet = SetWaterlineByteEnumProperty(OceanActor, TEXT("Dynamic Foam Mode"), 1);

	// A THIRD, separate foam toggle, found via the same GUI inspection under a different category
	// ("Ocean Simulation" > "Foam-Sim-VFX", not "Water Simulation") — "Use Foam" was still
	// unchecked even after Dynamic Foam/Dynamic Foam Mode were confirmed correctly enabled via the
	// spawn log, with zero visible change. Likely a master switch gating whether the main FFT
	// ocean material renders any foam/whitecap output at all, independent of Dynamic Foam.
	const bool bUseFoamSet = SetWaterlineBoolProperty(OceanActor, TEXT("Use Foam"), true);

	OceanActor->FinishSpawning(SpawnTransform);

	OceanActor->Tags.Add(TEXT("IH.Ocean.Primary"));
	WaterlineOceanInstance = OceanActor;

	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("Waterline adapter: spawned BP_Waterline_Ocean_Gen_4 ('%s'), waterLevelSet=%d, dynamicFoamSet=%d, dynamicFoamModeSet=%d, useFoamSet=%d. FFT simulation and Enable Ocean confirmed already true by vendor default (verified via headless reflection, not assumed)."),
		*OceanActor->GetName(), bWaterLevelSet ? 1 : 0, bDynamicFoamSet ? 1 : 0, bDynamicFoamModeSet ? 1 : 0, bUseFoamSet ? 1 : 0);

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
		const float HalfExtentXYCm = FMath::Max(FootprintRadiusCm * 1.3f, 5000.f);

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

		// Found empty (0 elements) via direct GUI inspection during PIE — the Shore Capture's
		// "what am I allowed to render" list was never populated. An empty list would explain
		// permanent black capture output regardless of position, timing, or resolution, which is
		// exactly what every prior diagnostic this session showed. Add the ocean and this Shore
		// Manager's own island, the two things it actually needs to render.
		const bool bCaptureActorOceanAdded = AddActorToWaterlineActorArrayProperty(ShoreActor, TEXT("Capture Actors"), WaterlineOceanInstance);
		const bool bCaptureActorIslandAdded = AddActorToWaterlineActorArrayProperty(ShoreActor, TEXT("Capture Actors"), Island);

		// Two properties confirmed real via headless reflection (Resolution=512, Capture Size=2500,
		// both vendor defaults) but never touched by any of the six prior fix attempts, since
		// positioning was the leading suspect until the 2026-08-27 Ocean_POV drift log confirmed the
		// shore manager DOES correctly track the camera's world position — closing off positioning as
		// the cause and leaving the capture/render pipeline as the remaining suspect. Capture Size at
		// its 2500 (cm) default is tiny next to a 5000m+ island footprint radius — almost certainly
		// too small an area to capture anything useful of the coastline; sized to the same real
		// per-island footprint already used for the Capture/Trigger Volume boxes below, not a guess.
		const bool bCaptureSizeSet = SetWaterlineIntProperty(ShoreActor, TEXT("Capture Size"), FMath::RoundToInt(HalfExtentXYCm * 2.f));
		const bool bResolutionSet = SetWaterlineIntProperty(ShoreActor, TEXT("Resolution"), 2048);

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
		ShoreManagerSpawnOrigins.Add(ShoreActor->GetActorLocation());
		++SpawnedCount;

		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Waterline adapter: Shore Manager spawned for island '%s' at (%.0f,%.0f), footprintRadiusCm=%.0f, waterBodySet=%d, captureActorOceanAdded=%d, captureActorIslandAdded=%d, captureSizeSet=%d, resolutionSet=%d, captureVolumeResized=%d, triggerVolumeResized=%d, forceUpdate=%d, forceTransfer=%d, fullDynamicGen=%d."),
			*Island->GetName(), IslandOrigin.X, IslandOrigin.Y, FootprintRadiusCm, bWaterBodySet ? 1 : 0, bCaptureActorOceanAdded ? 1 : 0, bCaptureActorIslandAdded ? 1 : 0, bCaptureSizeSet ? 1 : 0, bResolutionSet ? 1 : 0, bCaptureVolumeResized ? 1 : 0, bTriggerVolumeResized ? 1 : 0,
			bForceUpdateFound ? 1 : 0, bForceTransferFound ? 1 : 0, bFullDynamicGenFound ? 1 : 0);
	}

	UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter: spawned %d Shore Manager(s) for %d island(s)."),
		SpawnedCount, Islands.Num());
}
