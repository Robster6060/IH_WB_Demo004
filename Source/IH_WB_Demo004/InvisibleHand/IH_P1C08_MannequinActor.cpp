// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_MannequinActor.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "IH_P1C08_MannequinRegistrySubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AIH_P1C08_MannequinActor::AIH_P1C08_MannequinActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	SetActorEnableCollision(true);
	// 2026-09-11: ported from IH_WB_Stamps001's own DevReference Mannequin port, where this was
	// root-caused 2026-09-05 ("Mannequin is still not responsive to click") - the engine's stock
	// "Pawn" collision profile explicitly IGNORES the Visibility channel (BaseEngine.ini), so
	// without this override a line trace against ECC_Visibility (which this project's own click
	// routing uses throughout, e.g. TryPlaceMannequinAtScreen/TryIssueMoveOrderAtScreen) can never
	// hit a Mannequin's capsule at all. Demo004 never had this fix - needed regardless of the
	// separate, still-open body-invisibility investigation, since Mannequin select/move (planned)
	// depends on it.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 2026-09-11: switched from Waterline's bundled legacy UE4 SK_Mannequin to UE5.8's own bundled
	// Manny mesh (SKM_Manny_Simple) after the legacy mesh was "exhaustively diagnosed as invisible
	// with no root cause found." That diagnosis was wrong in one respect: the real cause (confirmed
	// this session) was the AnimationBlueprint-mode-with-no-AnimClass pose bug affecting ANY mesh
	// equally, not something specific to this asset.
	//
	// 2026-09-13: switched BACK to the legacy Waterline SK_Mannequin, now that the actual bug is
	// fixed - per the user's explicit request, to get a mesh with no weapon-like animation-retarget
	// artifact (SKM_Manny_Simple's skeleton is merely "compatible" with ThirdPerson_AnimBP's
	// TargetSkeleton, UE4_Mannequin_Skeleton, not the literal same asset, per this session's
	// diagnostic - that cross-skeleton evaluation is the suspected source of the artifact). This
	// mesh uses UE4_Mannequin_Skeleton directly - the AnimBP's exact, native target, zero ambiguity.
	// Note: /Game/Characters/Mannequins/Meshes/SK_Mannequin is a DIFFERENT asset at a similar-looking
	// path - that one is a bare Skeleton, not a mesh (confirmed via this session's own diagnostic) -
	// this FObjectFinder path is deliberately the Waterline one.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(TEXT(
		"/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
	if (MeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
	// 2026-09-11/12 root cause history (kept for context): AnimationMode defaults to
	// EAnimationMode::AnimationBlueprint, and with no AnimClass assigned a component in that mode
	// has no AnimInstance to evaluate a pose from every frame - it renders nothing at all, not even
	// a bind pose. That was the actual "pink stick" cause (the real body never rendered, leaving
	// only the LocatorBeacon, since retired, visible). The interim fix forced AnimationSingleNode +
	// bUseRefPoseOnInitAnim=true for a static reference pose - correct once rendering needed
	// confirming, but with troop movement now working end to end, a static pose while walking reads
	// as "floating." Now that a real AnimClass is assigned below, that workaround is removed.
	//
	// 2026-09-13: assigning the project's existing classic ThirdPerson_AnimBP - the same one
	// IH_WB_Stamps001's own working Mannequin dev-reference uses, built for the legacy SK_Mannequin
	// skeleton. SKM_Manny_Simple shares that IDENTICAL skeleton (confirmed via this session's own
	// diagnostic: SKM_Manny_Simple's Skeleton property resolves to SK_Mannequin's), so this AnimBP's
	// velocity-driven idle/walk blend should drive Manny directly with no retargeting needed.
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPFinder(TEXT(
		"/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Animations/ThirdPerson_AnimBP.ThirdPerson_AnimBP_C"));
	if (AnimBPFinder.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPFinder.Class);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
	}

	// 2026-09-12: ported from IH_WB_Stamps001's own DevReference Mannequin - auto-possess with a
	// plain AI controller so UNavigationSystemV1::SimpleMoveToLocation can walk this Character along
	// the NavMesh once selected and given a walk-to order (CommandWalkTo below).
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 2026-09-12: replaces the retired 60m magenta LocatorBeacon. This is NOT a "spot from far
	// away" exaggeration - it's sized to exactly match the real CapsuleComponent (radius 34cm,
	// total height 2*88cm = 176cm), so it doubles as (a) a see-through visibility aid at true
	// human scale and (b) a way to visually confirm the capsule/mesh isn't sinking below the
	// terrain, since it's centered on the same capsule the character actually stands on.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleVisMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CapsuleVisualizer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapsuleVisualizer"));
	CapsuleVisualizer->SetupAttachment(GetCapsuleComponent());
	CapsuleVisualizer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleVisualizer->SetCanEverAffectNavigation(false);
	CapsuleVisualizer->SetCastShadow(false);
	if (CapsuleVisMeshFinder.Succeeded())
	{
		CapsuleVisualizer->SetStaticMesh(CapsuleVisMeshFinder.Object);
	}
	// Engine Cylinder is 100x100x100 (pivot at center) - scale to the capsule's own diameter/height
	// and center it on the capsule (capsule's local origin is already its center).
	const float CapsuleDiameterCm = 34.f * 2.f;
	const float CapsuleTotalHeightCm = 88.f * 2.f;
	CapsuleVisualizer->SetRelativeScale3D(FVector(
		CapsuleDiameterCm / 100.f, CapsuleDiameterCm / 100.f, CapsuleTotalHeightCm / 100.f));
	if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/EngineDebugMaterials/M_SimpleTranslucent.M_SimpleTranslucent")))
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this))
		{
			// Cyan, 25% opacity - distinct from the gold selection tint and the red move-destination
			// flag. M_SimpleTranslucent has exactly one parameter, "Color" - it has no separate
			// opacity/alpha scalar, so translucency comes from this vector's own alpha channel
			// (confirmed via MaterialEditingLibrary introspection, not guessed - a bare RGB literal
			// here defaults alpha to 1.0/fully opaque, which was the bug in the first pass).
			MID->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor(0.f, 0.9f, 1.f, 0.25f));
			CapsuleVisualizer->SetMaterial(0, MID);
		}
	}
}

void AIH_P1C08_MannequinActor::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_MannequinRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C08_MannequinRegistrySubsystem>())
		{
			Registry->RegisterMannequin(this);
		}
	}

	// 2026-09-12: this project has no hand-placed NavMeshBoundsVolume (see DefaultEngine.ini) -
	// registering as a navigation invoker makes UNavigationSystemV1 generate NavMesh tiles in a
	// radius around wherever this Mannequin actually is, which is what CommandWalkTo's
	// SimpleMoveToLocation needs to path at all. Radii are generous relative to a human-scale unit
	// (a few hundred meters) since troop move-orders can reasonably span a beach/local area, not
	// just a few footsteps.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->RegisterNavigationInvoker(this, InvokerTileGenerationRadiusCm, InvokerTileRemovalRadiusCm);
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("P1C08: Mannequin '%s' registered as nav invoker (gen=%.0fcm, removal=%.0fcm)"),
			*GetName(), InvokerTileGenerationRadiusCm, InvokerTileRemovalRadiusCm);
	}
	else
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("P1C08: Mannequin '%s' found no UNavigationSystemV1 - troop movement will not work"), *GetName());
	}
}

void AIH_P1C08_MannequinActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->UnregisterNavigationInvoker(this);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_MannequinRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C08_MannequinRegistrySubsystem>())
		{
			Registry->UnregisterMannequin(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

UPrimitiveComponent* AIH_P1C08_MannequinActor::GetMannequinSelectionPrimitive() const
{
	return GetCapsuleComponent();
}

void AIH_P1C08_MannequinActor::SetMannequinSelected(bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}
	bSelected = bInSelected;

	if (!GetMesh())
	{
		return;
	}

	if (bSelected)
	{
		if (CachedSourceMaterials.Num() == 0)
		{
			const int32 SlotCount = FMath::Max(1, GetMesh()->GetNumMaterials());
			CachedSourceMaterials.Reset(SlotCount);
			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				CachedSourceMaterials.Add(GetMesh()->GetMaterial(SlotIndex));
			}
		}

		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (UMaterialInstanceDynamic* MID = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr)
		{
			// Warm gold/amber - distinct from every stamp-system green (IHTerrainStampColors).
			static const FName ColorNames[] = {
				FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
			};
			for (const FName& ColorName : ColorNames)
			{
				MID->SetVectorParameterValue(ColorName, FLinearColor(1.f, 0.72f, 0.1f));
			}
			for (int32 SlotIndex = 0; SlotIndex < CachedSourceMaterials.Num(); ++SlotIndex)
			{
				GetMesh()->SetMaterial(SlotIndex, MID);
			}
		}
	}
	else
	{
		for (int32 SlotIndex = 0; SlotIndex < CachedSourceMaterials.Num(); ++SlotIndex)
		{
			GetMesh()->SetMaterial(SlotIndex, CachedSourceMaterials[SlotIndex]);
		}
		CachedSourceMaterials.Reset();
	}
	GetMesh()->MarkRenderStateDirty();
}

void AIH_P1C08_MannequinActor::CommandWalkTo(const FVector& WorldDestination)
{
	TrueDestinationWorld = WorldDestination;
	bHasActiveDestination = true;

	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC)
	{
		// Mirrors the lab project's own fix for this exact regression ("Mannequin's lost right-click
		// move-to-destination order") - self-heal via SpawnDefaultController() rather than silently
		// dropping the order if AutoPossessAI hasn't (re-)possessed this Character yet.
		SpawnDefaultController();
		AIC = Cast<AAIController>(GetController());
	}
	if (AIC)
	{
		// 2026-09-12: calling MoveToLocation directly (rather than the void-returning
		// SimpleMoveToLocation wrapper) so the actual EPathFollowingRequestResult can be logged -
		// this project had zero navigation-related log output during the "flag spawns, nobody
		// moves" regression, making it impossible to tell success from failure after the fact.
		// bProjectDestinationToNavigation=true snaps the clicked point onto the nearest navigable
		// surface rather than requiring an exact on-mesh hit.
		const EPathFollowingRequestResult::Type Result = AIC->MoveToLocation(
			WorldDestination, ArrivalRadiusCm, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true,
			/*bProjectDestinationToNavigation=*/true);
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("P1C08: Mannequin '%s' CommandWalkTo (%.0f, %.0f, %.0f) -> MoveToLocation result=%d (0=Failed,1=AlreadyAtGoal,2=RequestSuccessful)"),
			*GetName(), WorldDestination.X, WorldDestination.Y, WorldDestination.Z, static_cast<int32>(Result));
	}
	else
	{
		UE_LOG(LogIH_WB_Demo004, Warning,
			TEXT("P1C08: Mannequin '%s' CommandWalkTo failed - no AIController even after SpawnDefaultController()"),
			*GetName());
	}
}

bool AIH_P1C08_MannequinActor::HasArrivedAtDestination() const
{
	if (!bHasActiveDestination)
	{
		return true;
	}
	return FVector::Dist2D(GetActorLocation(), TrueDestinationWorld) <= ArrivalRadiusCm;
}

void AIH_P1C08_MannequinActor::EnqueueWalkWaypoint(const FVector& WorldDestination)
{
	// 2026-09-13: breadcrumb waypoint chain. If idle (no active order at all), walk there right
	// away - matches CommandWalkTo's own behavior so the very first Shift+click in a chain still
	// starts moving immediately. If already mid-walk toward an earlier waypoint, queue this one;
	// AdvanceToNextWaypoint (called by the flag currently tracking this Mannequin) pops it once
	// the current leg is reached.
	if (!bHasActiveDestination)
	{
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("P1C08: Mannequin '%s' EnqueueWalkWaypoint(%s) - idle, walking immediately"),
			*GetName(), *WorldDestination.ToString());
		CommandWalkTo(WorldDestination);
		return;
	}
	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("P1C08: Mannequin '%s' EnqueueWalkWaypoint(%s) - queued (queue now has %d), current dest=%s"),
		*GetName(), *WorldDestination.ToString(), PendingWaypoints.Num() + 1, *TrueDestinationWorld.ToString());
	PendingWaypoints.Add(WorldDestination);
}

void AIH_P1C08_MannequinActor::AdvanceToNextWaypoint()
{
	if (PendingWaypoints.Num() == 0)
	{
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("P1C08: Mannequin '%s' AdvanceToNextWaypoint - queue empty, staying put"), *GetName());
		return;
	}
	const FVector Next = PendingWaypoints[0];
	PendingWaypoints.RemoveAt(0);
	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("P1C08: Mannequin '%s' AdvanceToNextWaypoint - popped %s, %d remaining"),
		*GetName(), *Next.ToString(), PendingWaypoints.Num());
	CommandWalkTo(Next);
}
