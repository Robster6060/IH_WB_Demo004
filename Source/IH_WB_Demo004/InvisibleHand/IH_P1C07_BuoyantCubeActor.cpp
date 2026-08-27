// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_P1C07_BuoyantCubeActor.h"



#include "BuoyancyComponent.h"

#include "BuoyancyTypes.h"

#include "WaterBodyActor.h"

#include "WaterBodyComponent.h"

#include "WaterBodyTypes.h"



#include "Components/PrimitiveComponent.h"

#include "Components/SphereComponent.h"

#include "Components/StaticMeshComponent.h"

#include "Engine/World.h"

#include "EngineUtils.h"

#include "Materials/MaterialInstanceDynamic.h"

#include "Materials/MaterialInterface.h"

#include "PhysicalMaterials/PhysicalMaterial.h"

#include "TimerManager.h"

#include "UObject/ConstructorHelpers.h"

#include "UObject/UObjectGlobals.h"



AIH_P1C07_BuoyantCubeActor::AIH_P1C07_BuoyantCubeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;



	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));

	SetRootComponent(Mesh);



	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (CubeAsset.Succeeded())

	{

		Mesh->SetStaticMesh(CubeAsset.Object);

	}



	Mesh->SetRelativeScale3D(FVector(100.f));

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	Mesh->SetCollisionObjectType(ECC_PhysicsBody);

	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);

	Mesh->SetSimulatePhysics(true);

	Mesh->SetEnableGravity(true);

	Mesh->BodyInstance.bUseCCD = true;

	Mesh->BodyInstance.SetOverrideIterationCounts(true);

	Mesh->BodyInstance.SetPositionSolverIterationCount(12);

	Mesh->BodyInstance.SetVelocitySolverIterationCount(10);

	Mesh->SetLinearDamping(0.42f);

	Mesh->SetAngularDamping(0.95f);

	Mesh->BodyInstance.bOverrideMass = true;

	Mesh->BodyInstance.SetMassOverride(5.95e7f, true);

	Mesh->SetGenerateOverlapEvents(true);



	Buoyancy = CreateDefaultSubobject<UBuoyancyComponent>(TEXT("Buoyancy"));

	// Pontoon forces alone equilibrate ~50% submerged on a 100 m cube; Archimedes lift runs in Tick.
	Buoyancy->SetCanBeActive(false);

	Buoyancy->bUseAsyncPath = false;

	Buoyancy->BuoyancyData.bCenterPontoonsOnCOM = false;

	Buoyancy->BuoyancyData.BuoyancyCoefficient = 1.55f;

	Buoyancy->BuoyancyData.BuoyancyDamp = 620.f;

	Buoyancy->BuoyancyData.BuoyancyDamp2 = 0.72f;

	Buoyancy->BuoyancyData.bApplyRiverForces = false;

	Buoyancy->BuoyancyData.MaxBuoyantForce = 3.0e12f;



	const auto AddP = [this](FVector Loc, float R)

	{

		FSphericalPontoon P;

		P.RelativeLocation = Loc;

		P.Radius = R;

		Buoyancy->BuoyancyData.Pontoons.Add(P);

	};



	// RelativeLocation is unscaled component-local space; TransformPosition applies mesh scale (100).
	// World cm targets: corners ±3700, Z layers −4850…−1120; radii are world cm and not scaled.
	const float XY = 37.f;
	const float RCorner = 1780.f;
	const float RInner = 1520.f;
	const float Zs[] = {-48.5f, -39.2f, -29.8f, -20.5f, -11.2f};

	for (float Z : Zs)
	{
		for (int32 Sx = -1; Sx <= 1; Sx += 2)
		{
			for (int32 Sy = -1; Sy <= 1; Sy += 2)
			{
				AddP(FVector(XY * Sx, XY * Sy, Z), RCorner);
			}
		}
	}

	AddP(FVector(0.f, 0.f, -36.f), RInner);
	AddP(FVector(0.f, 0.f, -22.f), RInner);
	AddP(FVector(0.f, 0.f, -9.f), RInner);

	// Waterline's BP_Shore_Manager_Gen4 does Get Components by Tag(Component Class=Sphere Collision,
	// Tag="Ocean_POV") to find where to reposition its capture; no actor in the project carries this
	// tag today, which is the suspected root cause of the shore manager never tracking a real location.
	OceanPovSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OceanPovSphere"));
	OceanPovSphere->SetupAttachment(Mesh);
	OceanPovSphere->SetSphereRadius(100.f);
	OceanPovSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OceanPovSphere->SetGenerateOverlapEvents(false);
	OceanPovSphere->ComponentTags.Add(FName(TEXT("Ocean_POV")));

}



void AIH_P1C07_BuoyantCubeActor::BeginPlay()

{

	if (Mesh)

	{

		Mesh->OnComponentBeginOverlap.AddDynamic(this, &AIH_P1C07_BuoyantCubeActor::HandleMeshBeginOverlap);

		Mesh->OnComponentEndOverlap.AddDynamic(this, &AIH_P1C07_BuoyantCubeActor::HandleMeshEndOverlap);

	}



	Super::BeginPlay();



	if (!Mesh || HasAnyFlags(RF_ClassDefaultObject))

	{

		return;

	}



	if (UPhysicalMaterial* HullPM = NewObject<UPhysicalMaterial>(this, TEXT("BuoyantCubeHullPM")))

	{

		HullPM->Friction = 0.35f;

		HullPM->Restitution = 0.f;

		Mesh->SetPhysMaterialOverride(HullPM);

	}



	if (UMaterialInterface* Parent =

			LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))

	{

		if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, this))

		{

			static const FName ColorNames[] = {

				FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector"))};

			const FLinearColor Red(0.92f, 0.06f, 0.04f, 1.f);

			for (const FName& N : ColorNames)

			{

				Mid->SetVectorParameterValue(N, Red);

			}

			Mesh->SetMaterial(0, Mid);

		}

	}



	SyncWaterBodiesFromOverlaps();



	// Deferred zone rebuild leaves Water collision / overlap lists stale for the first ticks.

	if (UWorld* W = GetWorld())

	{

		WaterBuoyancyPollRemaining = 52;

		W->GetTimerManager().ClearTimer(WaterBuoyancyPollTimerHandle);

		W->GetTimerManager().SetTimer(

			WaterBuoyancyPollTimerHandle,

			this,

			&AIH_P1C07_BuoyantCubeActor::RetryRegisterWaterBodiesForBuoyancy,

			0.22f,

			true);

	}

}



void AIH_P1C07_BuoyantCubeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(WaterBuoyancyPollTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

namespace IH_P1C07Buoyancy
{
	static constexpr float GravityCmPerSec2 = 980.f;
	/** 1000 kg/m³ → kg/cm³ */
	static constexpr float WaterDensityKgPerCm3 = 1e-3f;

	static bool QueryWaterSurfaceZ(UWaterBodyComponent* Wbc, const FVector& AtLocation, float& OutSurfaceZ)
	{
		if (!Wbc)
		{
			return false;
		}

		FVector SurfaceLoc;
		FVector Normal;
		FVector Velocity;
		float Depth = 0.f;
		if (!Wbc->GetWaterSurfaceInfoAtLocation(AtLocation, SurfaceLoc, Normal, Velocity, Depth, true))
		{
			return false;
		}

		OutSurfaceZ = SurfaceLoc.Z;
		return true;
	}
}

void AIH_P1C07_BuoyantCubeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ApplyArchimedesBuoyancy(DeltaTime);
}

void AIH_P1C07_BuoyantCubeActor::ApplyArchimedesBuoyancy(float DeltaTime)
{
	(void)DeltaTime;

	if (!Mesh || !Mesh->IsSimulatingPhysics())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform Xform = Mesh->GetComponentTransform();
	const FBoxSphereBounds Bounds = Mesh->CalcBounds(Xform);
	const float HalfX = Bounds.BoxExtent.X;
	const float HalfY = Bounds.BoxExtent.Y;
	const float HalfZ = Bounds.BoxExtent.Z;
	const FVector Origin = Bounds.Origin;
	const float BottomZ = Origin.Z - HalfZ;

	float WaterSurfaceZ = -TNumericLimits<float>::Max();
	bool bFoundSurface = false;

	for (TActorIterator<AWaterBody> It(World); It; ++It)
	{
		AWaterBody* WB = *It;
		if (!IsValid(WB))
		{
			continue;
		}

		float SurfaceZ = 0.f;
		if (IH_P1C07Buoyancy::QueryWaterSurfaceZ(WB->GetWaterBodyComponent(), Origin, SurfaceZ))
		{
			WaterSurfaceZ = FMath::Max(WaterSurfaceZ, SurfaceZ);
			bFoundSurface = true;
		}
	}

	if (!bFoundSurface || BottomZ >= WaterSurfaceZ)
	{
		return;
	}

	const float SubDepth = FMath::Clamp(WaterSurfaceZ - BottomZ, 0.f, 2.f * HalfZ);
	if (SubDepth <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float SubVolumeCm3 = (2.f * HalfX) * (2.f * HalfY) * SubDepth;
	const float BuoyancyForce = SubVolumeCm3 * IH_P1C07Buoyancy::WaterDensityKgPerCm3 * IH_P1C07Buoyancy::GravityCmPerSec2;

	const float VelZ = Mesh->GetPhysicsLinearVelocity().Z;
	const float DampForce = -Buoyancy->BuoyancyData.BuoyancyDamp * VelZ;

	Mesh->AddForce(FVector(0.f, 0.f, BuoyancyForce + DampForce));
}



void AIH_P1C07_BuoyantCubeActor::RetryRegisterWaterBodiesForBuoyancy()

{

	SyncWaterBodiesFromOverlaps();



	UWorld* W = GetWorld();

	if (!W)

	{

		return;

	}



	if (Buoyancy && Buoyancy->GetCurrentWaterBodyComponents().Num() > 0)

	{

		W->GetTimerManager().ClearTimer(WaterBuoyancyPollTimerHandle);

		WaterBuoyancyPollRemaining = 0;

		return;

	}



	if (--WaterBuoyancyPollRemaining <= 0)

	{

		W->GetTimerManager().ClearTimer(WaterBuoyancyPollTimerHandle);

	}

}



void AIH_P1C07_BuoyantCubeActor::SyncWaterBodiesFromOverlaps()

{

	if (!Mesh || !Buoyancy)

	{

		return;

	}



	UWorld* W = Mesh->GetWorld();

	if (!W)

	{

		return;

	}



	TArray<AActor*> OverlappingActors;

	Mesh->GetOverlappingActors(OverlappingActors, AWaterBody::StaticClass());



	TSet<UWaterBodyComponent*> Present;

	for (AActor* Act : OverlappingActors)

	{

		if (AWaterBody* WB = Cast<AWaterBody>(Act))

		{

			if (UWaterBodyComponent* Wbc = WB->GetWaterBodyComponent())

			{

				Present.Add(Wbc);

			}

		}

	}



	{

		EWaterBodyQueryFlags QueryFlags = EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::ComputeNormal |

										  EWaterBodyQueryFlags::ComputeImmersionDepth | EWaterBodyQueryFlags::ComputeVelocity;

		QueryFlags |= EWaterBodyQueryFlags::IncludeWaves;



		const FVector CenterLoc = Mesh->GetComponentTransform().GetTranslation();

		const FVector Ext = Mesh->CalcBounds(Mesh->GetComponentTransform()).BoxExtent;

		const float BottomZ = CenterLoc.Z - Ext.Z;

		const float TopZ = CenterLoc.Z + Ext.Z;

		const FVector ProbesWorld[] = {

			CenterLoc,

			CenterLoc + FVector(0.f, 0.f, -Ext.Z),

			CenterLoc + FVector(0.f, 0.f, -Ext.Z * 0.55f),

		};



		for (TActorIterator<AWaterBody> It(W); It; ++It)

		{

			AWaterBody* WB = *It;

			if (!IsValid(WB))

			{

				continue;

			}

			UWaterBodyComponent* Wbc = WB->GetWaterBodyComponent();

			if (!Wbc)

			{

				continue;

			}

			if (Present.Contains(Wbc))

			{

				continue;

			}



			float SurfaceZ = 0.f;

			if (IH_P1C07Buoyancy::QueryWaterSurfaceZ(Wbc, CenterLoc, SurfaceZ))

			{

				if (BottomZ < SurfaceZ && TopZ > SurfaceZ - Ext.Z)

				{

					Present.Add(Wbc);

					continue;

				}

			}



			for (const FVector& ProbeWorldLoc : ProbesWorld)

			{

				TValueOrError<FWaterBodyQueryResult, EWaterBodyQueryError> QueryResult =

					Wbc->TryQueryWaterInfoClosestToWorldLocation(ProbeWorldLoc - FVector::UpVector * 50.f, QueryFlags);



				if (QueryResult.HasValue() && QueryResult.GetValue().IsInWater())

				{

					Present.Add(Wbc);

					break;

				}

			}

		}

	}



	TArray<UWaterBodyComponent*> Prev;

	const TArray<TObjectPtr<UWaterBodyComponent>>& Cur = Buoyancy->GetCurrentWaterBodyComponents();

	Prev.Reserve(Cur.Num());

	for (const TObjectPtr<UWaterBodyComponent>& Ptr : Cur)

	{

		Prev.Add(Ptr.Get());

	}



	for (UWaterBodyComponent* Wbc : Prev)

	{

		if (Wbc && !Present.Contains(Wbc))

		{

			const FVector CenterLoc = Mesh->GetComponentTransform().GetTranslation();

			const float BottomZ = CenterLoc.Z - Mesh->CalcBounds(Mesh->GetComponentTransform()).BoxExtent.Z;

			float SurfaceZ = 0.f;

			const bool bStillSubmerged = IH_P1C07Buoyancy::QueryWaterSurfaceZ(Wbc, CenterLoc, SurfaceZ) && BottomZ < SurfaceZ;

			if (!bStillSubmerged)

			{

				Buoyancy->ExitedWaterBody(Wbc);

			}

		}

	}



	for (UWaterBodyComponent* Wbc : Present)

	{

		if (Wbc)

		{

			Buoyancy->EnteredWaterBody(Wbc);

		}

	}

}



void AIH_P1C07_BuoyantCubeActor::HandleMeshBeginOverlap(

	UPrimitiveComponent* OverlappedComponent,

	AActor* OtherActor,

	UPrimitiveComponent* OtherComp,

	int32 OtherBodyIndex,

	bool bFromSweep,

	const FHitResult& SweepResult)

{

	(void)OverlappedComponent;

	(void)OtherActor;

	(void)OtherComp;

	(void)OtherBodyIndex;

	(void)bFromSweep;

	(void)SweepResult;

	SyncWaterBodiesFromOverlaps();

}



void AIH_P1C07_BuoyantCubeActor::HandleMeshEndOverlap(

	UPrimitiveComponent* OverlappedComponent,

	AActor* OtherActor,

	UPrimitiveComponent* OtherComp,

	int32 OtherBodyIndex)

{

	(void)OverlappedComponent;

	(void)OtherActor;

	(void)OtherComp;

	(void)OtherBodyIndex;

	SyncWaterBodiesFromOverlaps();

}


