// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_WaterlineOceanAdapter.h"
#include "IH_WB_IslandActor.h"
#include "Components/BoxComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "Materials/MaterialInterface.h"

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

	/** Logs the real enum names/values behind a byte/enum property (e.g. "Dynamic Foam Mode") —
	 * "All Dynamic Foam" turned out to mean "unmasked, everywhere" rather than shore-aware, so the
	 * next guess needs the actual option list, not another blind increment. Works for both
	 * FByteProperty (with an attached UEnum) and FEnumProperty. */
	static void LogWaterlineEnumOptions(AActor* Actor, const TCHAR* PropertyName)
	{
		if (!Actor) { return; }
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(PropertyName));
		UEnum* Enum = nullptr;
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			Enum = ByteProp->Enum;
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			Enum = EnumProp->GetEnum();
		}
		if (!Enum)
		{
			UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: '%s' on '%s' has no attached UEnum (Prop=%s)."),
				PropertyName, *Actor->GetName(), Prop ? *Prop->GetClass()->GetName() : TEXT("NOT FOUND"));
			return;
		}
		// GetNameStringByIndex returns the raw internal enumerator name, which for a
		// Blueprint-authored UUserDefinedEnum is an unhelpful placeholder ("NewEnumerator0") —
		// confirmed via the first log from this diagnostic. The text actually shown in the Editor
		// dropdown ("No Dynamic Foam", "All Dynamic Foam") is separate display-name metadata,
		// read via GetDisplayNameTextByIndex instead.
		FString Options;
		for (int32 i = 0; i < Enum->NumEnums(); ++i)
		{
			Options += FString::Printf(TEXT("[%lld]=\"%s\" "), Enum->GetValueByIndex(i), *Enum->GetDisplayNameTextByIndex(i).ToString());
		}
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: '%s' on '%s' real enum '%s' options: %s"),
			PropertyName, *Actor->GetName(), *Enum->GetName(), *Options);
	}

	/** Dumps every FBoolProperty on an actor's class (including inherited/Blueprint-added
	 * variables) with its current value — a complete map of every on/off switch in one pass,
	 * rather than finding hidden "Enable X" toggles one GUI screenshot at a time. This session has
	 * already found three separate hidden bool/empty-array gates (Dynamic Foam, Use Foam, Capture
	 * Actors) by scrolling categories one at a time; this replaces that with a single log line. */
	static void LogAllWaterlineBoolProperties(AActor* Actor, const TCHAR* Label)
	{
		if (!Actor) { return; }
		FString Dump;
		int32 Count = 0;
		for (TFieldIterator<FBoolProperty> It(Actor->GetClass()); It; ++It)
		{
			FBoolProperty* BoolProp = *It;
			const bool Value = BoolProp->GetPropertyValue_InContainer(Actor);
			Dump += FString::Printf(TEXT("%s=%s "), *BoolProp->GetName(), Value ? TEXT("true") : TEXT("false"));
			++Count;
		}
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter DIAG: %s ('%s') has %d bool properties: %s"),
			Label, *Actor->GetName(), Count, *Dump);
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

	/** FStructProperty-aware setter for FVector-typed plain variables (e.g. "Trigger Volume
	 * Extent") — distinct from the live UBoxComponent extent set via SetBoxExtent(); this is a
	 * separate Blueprint variable the vendor's own Construction Script/timer logic may read to
	 * (re)derive the actual Trigger Volume component's size, independent of whatever the
	 * component's live extent currently is. */
	static bool SetWaterlineVectorProperty(AActor* Actor, const TCHAR* PropertyName, const FVector& Value)
	{
		if (!Actor) { return false; }
		FStructProperty* StructProp = CastField<FStructProperty>(Actor->GetClass()->FindPropertyByName(FName(PropertyName)));
		if (!StructProp || StructProp->Struct != TBaseStructure<FVector>::Get())
		{
			return false;
		}
		void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(Actor);
		*static_cast<FVector*>(ValuePtr) = Value;
		return true;
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
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

void AIH_WaterlineOceanAdapter::RefreshShoreManagersBounded()
{
	--ShoreRefreshCallsRemaining;
	for (const TObjectPtr<AActor>& Shore : ShoreManagerInstances)
	{
		if (IsValid(Shore))
		{
			CallWaterlineFunction(Shore, TEXT("Force Update"));
			CallWaterlineFunction(Shore, TEXT("Force Transfer"));
		}
	}
	UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter: bounded Shore Manager refresh fired (%d call(s) remaining after this one)."),
		ShoreRefreshCallsRemaining);
	if (ShoreRefreshCallsRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(ShoreRefreshTimerHandle);
		// One-time sanity check on the final bounded call only — confirms capture is still
		// producing real data without the recurring per-3-seconds GPU-stall cost this diagnostic
		// used to have.
		if (ShoreManagerInstances.IsValidIndex(0) && IsValid(ShoreManagerInstances[0]))
		{
			LogWaterlineRenderTargetStats(ShoreManagerInstances[0], TEXT("RT Shore Final"));
			LogWaterlineRenderTargetStats(ShoreManagerInstances[0], TEXT("RT JFA 1"));
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

	// 2026-08-28: reverted to match the native Gen4 Shore_Map reference exactly (opened directly in
	// the new IH_WB_WavesDemo01 lab project — see IH_Waterline_Progress.md). That level's own
	// BP_Waterline_Ocean_Gen_4 instance has Dynamic Foam=false, Dynamic Foam Mode="No Dynamic Foam",
	// and Use Foam=false — the SAME vendor defaults this session spent Attempts 11/14 turning on —
	// yet produces real, correctly-composited white shore-break foam. Enabling these was never the
	// real mechanism; whatever visible response resulted in IH_WB_Demo004 (blotchy, raw JFA-colored,
	// bleeding onto land before the CD Dynamic Foam Mode fix) was a secondary, unwanted effect.
	// Left OFF here so the Water Surface Material swap below is tested in isolation.
	const bool bDynamicFoamSet = SetWaterlineBoolProperty(OceanActor, TEXT("Dynamic Foam"), false);
	const bool bDynamicFoamModeSet = SetWaterlineByteEnumProperty(OceanActor, TEXT("Dynamic Foam Mode"), 0);
	const bool bUseFoamSet = SetWaterlineBoolProperty(OceanActor, TEXT("Use Foam"), false);

	// 2026-08-28: values confirmed directly from Shore_Map's own placed, working
	// BP_Waterline_Ocean_Gen_4 instance — extracted headlessly via Python EditorAssetLibrary
	// reflection against IH_WB_WavesDemo01 (not a guess or a screenshot read). The earlier
	// both-false revert was itself unconfirmed; the real reference has "Sim Active?" false but
	// "Ocean is Live?" TRUE.
	const bool bSimActiveSet = SetWaterlineBoolProperty(OceanActor, TEXT("Sim Active?"), false);
	const bool bOceanIsLiveSet = SetWaterlineBoolProperty(OceanActor, TEXT("Ocean is Live?"), true);

	// THE likely real root cause of the wave-scale/"huge tsunami pulse" problem, confirmed via the
	// same extraction: the working reference uses "1 Ocean Simulation Type" =
	// BAKED_OCEAN_SIMULATION (enum value 1), NOT FFT_SIMULATION (0) — the vendor default this
	// session has been running with the entire time, confirmed via earlier reflection ("FFT
	// simulation... confirmed already true by vendor default") without checking whether FFT was
	// actually the *right* mode. FFT is a live, reactive, real-time simulation; baked is a
	// pre-computed, stable texture loop — much more likely to hold up correctly at IH's much larger
	// realm scale, and exactly what the confirmed-working reference actually uses.
	const bool bSimTypeSet = SetWaterlineByteEnumProperty(OceanActor, TEXT("1 Ocean Simulation Type"), 1);
	const bool bBakedSpeedSet = SetWaterlineFloatProperty(OceanActor, TEXT("1 Baked Ocean Speed"), 30.f);

	// THE fix, per the native Gen4 Shore_Map reference: that level's own placed
	// BP_Waterline_Ocean_Gen_4 instance overrides "1 Water Surface Material" to MI_WS_Gen4_Shore —
	// NOT the generic MI_Water_Surface_Gen4 the Blueprint class itself defaults to (confirmed via a
	// binary string scan of the .uasset). Loaded here from an IH-owned duplicate,
	// MI_IH_Water_Surface_Shore (/Game/InvisibleHand/Materials/Waterline/), not the vendor original
	// — standing rule: never edit vendor assets under /Game/Waterline directly; this copy is free
	// to tune (e.g. for the huge-wave/scale-mismatch problem found at IH's realm scale) without
	// touching vendor content. Duplicated via a headless Python EditorAssetLibrary script, 2026-08-28.
	UMaterialInterface* ShoreWaterMaterial = Cast<UMaterialInterface>(StaticLoadObject(
		UMaterialInterface::StaticClass(), nullptr,
		TEXT("/Game/InvisibleHand/Materials/Waterline/MI_IH_Water_Surface_Shore.MI_IH_Water_Surface_Shore")));
	const bool bShoreMaterialSet = SetWaterlineObjectProperty(OceanActor, TEXT("1 Water Surface Material"), ShoreWaterMaterial);

	OceanActor->FinishSpawning(SpawnTransform);

	OceanActor->Tags.Add(TEXT("IH.Ocean.Primary"));
	WaterlineOceanInstance = OceanActor;

	// Complete bool-property map of the Ocean actor, post-construction — hunting for any other
	// hidden "Enable X" gate the same way Dynamic Foam/Use Foam were found, without needing another
	// round of manual GUI category-scrolling.
	LogAllWaterlineBoolProperties(OceanActor, TEXT("Ocean actor"));

	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("Waterline adapter: spawned BP_Waterline_Ocean_Gen_4 ('%s'), waterLevelSet=%d, dynamicFoamSet=%d, dynamicFoamModeSet=%d, useFoamSet=%d, simActiveSet=%d, oceanIsLiveSet=%d, simTypeSet=%d, bakedSpeedSet=%d, shoreMaterialLoaded=%d, shoreMaterialSet=%d."),
		*OceanActor->GetName(), bWaterLevelSet ? 1 : 0, bDynamicFoamSet ? 1 : 0, bDynamicFoamModeSet ? 1 : 0, bUseFoamSet ? 1 : 0, bSimActiveSet ? 1 : 0, bOceanIsLiveSet ? 1 : 0, bSimTypeSet ? 1 : 0, bBakedSpeedSet ? 1 : 0, ShoreWaterMaterial ? 1 : 0, bShoreMaterialSet ? 1 : 0);

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
		// exactly what every prior diagnostic this session showed. Add this Shore Manager's own
		// island (the terrain it needs to render) — NOT the ocean: confirmed via a full headless
		// Python property extraction of Shore_Map's own working, placed Shore Manager instance
		// (2026-08-28) that its real Capture Actors list is only the level's Landscape + 9
		// StaticMeshActors (terrain geometry), never the Ocean actor itself.
		const bool bCaptureActorIslandAdded = AddActorToWaterlineActorArrayProperty(ShoreActor, TEXT("Capture Actors"), Island);

		// 2026-08-28: reverted to the vendor defaults, confirmed via the same extraction — the real
		// working Shore_Map instance uses Capture Size=2500, Resolution=512, NOT scaled to the
		// island's footprint. This only makes sense given the Shore Manager repositions itself every
		// tick to follow the camera (the Ocean_POV drift diagnostic already proved this) — it was
		// never meant to capture an entire island at once, only a small, local area wherever the
		// camera currently is. The earlier whole-island-footprint scaling was the wrong fix.
		const bool bCaptureSizeSet = SetWaterlineIntProperty(ShoreActor, TEXT("Capture Size"), 2500);
		const bool bResolutionSet = SetWaterlineIntProperty(ShoreActor, TEXT("Resolution"), 512);
		const bool bOffsetWaterLevelSet = SetWaterlineFloatProperty(ShoreActor, TEXT("Offset Water Level"), 50.f);

		// 2026-08-28, Observation 1 investigation (beach-relocation delay/disappear-reappear):
		// list_member_variable_names surfaced two real properties the earlier ~130-name extraction
		// never tried, since their names weren't known in advance. Both are confirmed, via headless
		// reflection, to be the UNTOUCHED vendor class default AND to match the confirmed-working
		// Shore_Map reference instance exactly (neither is an override) — so this is real vendor
		// design, not an IH-side regression, just one that IH's abrupt beach-to-beach camera jumps
		// expose more visibly than the reference demo's small, static test scene ever would:
		//   "Shore Generation Framerate" = 0.1 (CDO and reference both) — the internal
		//   capture/composite regen cycle runs once every 10 SECONDS (1/0.1), which is very likely
		//   the direct, well-evidenced cause of the observed "several seconds of delay/absence."
		// Raised moderately (5x, to a 2s cycle) rather than maximized — this is the vendor's own
		// internal timer, not the external per-3-second ReadPixels/Capture Scene diagnostic loop
		// that caused this session's real PIE memory-pressure crash, but the same lesson (bounded,
		// not unbounded) still applies. Needs a PIE regression check, not assumed safe.
		const bool bGenFramerateSet = SetWaterlineFloatProperty(ShoreActor, TEXT("Shore Generation Framerate"), 0.5f);

		// "Trigger Volume Extent" (100,100,32cm vendor default, also confirmed untouched/matching
		// the reference) is a SEPARATE plain variable from the live "Trigger Volume" BoxComponent
		// resized below, post-FinishSpawning(). If the vendor's own Construction Script/timer logic
		// re-derives the live component's size from this property on each regen cycle (plausible,
		// given the same actor also exposes a 10s-default regen timer), our post-spawn component
		// resize would get silently snapped back to a ~1m box every cycle — a real candidate for the
		// "abruptly disappears" half of Observation 1. Synced here, pre-FinishSpawning(), to the same
		// HalfExtentXYCm the live component gets resized to below, so both stay consistent regardless
		// of which one any given internal code path actually reads.
		const bool bTriggerVolumeExtentSet = SetWaterlineVectorProperty(ShoreActor, TEXT("Trigger Volume Extent"), FVector(HalfExtentXYCm, HalfExtentXYCm, 5000.f));

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

		// Complete bool-property map, first island only (avoids 3x log spam) — same hunt as the
		// Ocean actor's dump above, this time on the Shore Manager itself, looking specifically for
		// whatever gates its own dedicated Shore Wave texture system (T_PL_Wave_1_Disp/Nrml) since
		// the Ocean's Dynamic Foam/Use Foam are confirmed rendering raw JFA data as color instead.
		if (SpawnedCount == 0)
		{
			LogAllWaterlineBoolProperties(ShoreActor, TEXT("Shore Manager"));
		}

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
			TEXT("Waterline adapter: Shore Manager spawned for island '%s' at (%.0f,%.0f), footprintRadiusCm=%.0f, waterBodySet=%d, captureActorIslandAdded=%d, captureSizeSet=%d, resolutionSet=%d, offsetWaterLevelSet=%d, genFramerateSet=%d, triggerVolumeExtentSet=%d, captureVolumeResized=%d, triggerVolumeResized=%d, forceUpdate=%d, forceTransfer=%d, fullDynamicGen=%d."),
			*Island->GetName(), IslandOrigin.X, IslandOrigin.Y, FootprintRadiusCm, bWaterBodySet ? 1 : 0, bCaptureActorIslandAdded ? 1 : 0, bCaptureSizeSet ? 1 : 0, bResolutionSet ? 1 : 0, bOffsetWaterLevelSet ? 1 : 0, bGenFramerateSet ? 1 : 0, bTriggerVolumeExtentSet ? 1 : 0, bCaptureVolumeResized ? 1 : 0, bTriggerVolumeResized ? 1 : 0,
			bForceUpdateFound ? 1 : 0, bForceTransferFound ? 1 : 0, bFullDynamicGenFound ? 1 : 0);
	}

	UE_LOG(LogIH_WB_Demo004, Log, TEXT("Waterline adapter: spawned %d Shore Manager(s) for %d island(s)."),
		SpawnedCount, Islands.Num());

	// Bounded, timer-based re-call (NOT a per-Tick loop — see RefreshShoreManagersBounded's header
	// comment for why the previous every-3-seconds-forever version caused real PIE sluggishness/
	// memory pressure). 4 calls, 2s apart: clears Shore Warmup's one-shot gate (needs a 2nd call
	// >1s after the 1st) and gives Force Transfer a few chances to push real data once capture is
	// confirmed producing it, then stops — BP_Shore_Manager_Gen4's own internal "Full Dynamic Gen"
	// timer (already triggered once above) continues on its own from there.
	if (SpawnedCount > 0)
	{
		ShoreRefreshCallsRemaining = 4;
		GetWorldTimerManager().SetTimer(ShoreRefreshTimerHandle, this,
			&AIH_WaterlineOceanAdapter::RefreshShoreManagersBounded, 2.f, true);
	}
}
