// Copyright Epic Games, Inc. All Rights Reserved.
// IH P1C12 — Custom Gerstner-wave ocean plane (endless camera-follow tile + horizon skirt).
// No AWaterBodyOcean / AWaterZone. No Water Plugin runtime configuration.

#include "IH_P1C12_OceanPlane.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogIH_OceanPlane, Log, All);

static constexpr float G_Gravity = 981.f; // cm/s²

AIH_P1C12_OceanPlane::AIH_P1C12_OceanPlane()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	OceanMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("OceanMesh"));
	OceanMesh->bUseAsyncCooking = false;
	SetRootComponent(OceanMesh);

	HorizonSkirtMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HorizonSkirtMesh"));
	HorizonSkirtMesh->SetupAttachment(OceanMesh);
	HorizonSkirtMesh->bUseAsyncCooking = false;
	HorizonSkirtMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SetActorLocation(FVector::ZeroVector);
	SetActorRotation(FRotator::ZeroRotator);
}

void AIH_P1C12_OceanPlane::BeginPlay()
{
	Super::BeginPlay();
	ConfigureEndlessSea();
}

void AIH_P1C12_OceanPlane::ConfigureEndlessSea()
{
	bEndlessSeaFollowCamera = true;
	// Locked hull-proportioned Gerstner: 8×8 km @ 256 div (cell ≈ 31.3 m).
	PlaneHalfExtentCm = 400000.f;
	GridDivisions = 256;
	HorizonSkirtHalfExtentCm = FMath::Max(HorizonSkirtHalfExtentCm, 15000000.f);
	InitWaveTrains();
	RebuildOceanMesh();
}

void AIH_P1C12_OceanPlane::ApplyDevOceanVisibility(bool bOceanVisible)
{
	SetActorHiddenInGame(!bOceanVisible);
	if (OceanMesh)
	{
		OceanMesh->SetVisibility(bOceanVisible, true);
		OceanMesh->SetHiddenInGame(!bOceanVisible, true);
	}
	if (HorizonSkirtMesh)
	{
		HorizonSkirtMesh->SetVisibility(bOceanVisible, true);
		HorizonSkirtMesh->SetHiddenInGame(!bOceanVisible, true);
	}
	SetActorTickEnabled(bOceanVisible);
}

void AIH_P1C12_OceanPlane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bEndlessSeaFollowCamera)
	{
		FollowCameraXY();
	}

	if (!OceanMesh || WaveTrains.IsEmpty() || BaseXY.IsEmpty())
	{
		return;
	}

	const float WorldT = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float T = WorldT * WaveTimeScale;
	FadeInAlpha = FMath::Min(FadeInAlpha + DeltaTime / 3.f, 1.f);
	TickWaveDisplacement(T, FadeInAlpha);
}

void AIH_P1C12_OceanPlane::FollowCameraXY()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector TargetXY = GetActorLocation();
	bool bFound = false;

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Cam = PC->PlayerCameraManager)
		{
			TargetXY = Cam->GetCameraLocation();
			bFound = true;
		}
		else if (APawn* Pawn = PC->GetPawn())
		{
			TargetXY = Pawn->GetActorLocation();
			bFound = true;
		}
	}

	if (!bFound)
	{
		return;
	}

	SetActorLocation(FVector(TargetXY.X, TargetXY.Y, 0.f));
}

float AIH_P1C12_OceanPlane::GetGridCellCm() const
{
	const int32 N = FMath::Max(8, GridDivisions);
	return (2.f * PlaneHalfExtentCm) / static_cast<float>(N);
}

// UE cm. Merchantman hull = 37.8 m. Gerstner 8×8 km + 300×300 km flat skirt.
// Hull-proportioned λ ≈ 1.7–2.5× hull; amps sum ~66 cm; periods ~1.5–3.2 s.

void AIH_P1C12_OceanPlane::InitWaveTrains()
{
	WaveTrains.Reset();
	FadeInAlpha = 0.f;

	const float CellCm = GetGridCellCm();
	const float MinLambdaCm = 2.0f * CellCm;
	// Locked hull-proportioned table (cm / sec).
	const float LambdaCm[4] = { 9500.f, 8000.f, 7000.f, 6500.f }; // 95/80/70/65 m
	const float PeriodsSec[4] = { 3.2f, 2.5f, 2.0f, 1.5f };
	const float AmpsCm[4] = { 28.f, 18.f, 12.f, 8.f };
	const float Steep[4] = { 0.18f, 0.15f, 0.12f, 0.08f };
	const FVector2D Dirs[4] = {
		FVector2D(0.707f, 0.707f),
		FVector2D(0.998f, 0.063f),
		FVector2D(-0.500f, 0.866f),
		FVector2D(0.342f, 0.940f),
	};

	for (int32 i = 0; i < 4; ++i)
	{
		FIHGerstnerWave W;
		W.Direction = Dirs[i];
		W.AmplitudeCm = AmpsCm[i];
		W.Steepness = Steep[i];
		W.WavelengthCm = FMath::Max(MinLambdaCm, LambdaCm[i]);

		const float k = 2.f * PI / W.WavelengthCm;
		const float CDeep = FMath::Sqrt(G_Gravity / k);
		const float CTarget = W.WavelengthCm / PeriodsSec[i];
		W.PhaseSpeed = CTarget / FMath::Max(CDeep, KINDA_SMALL_NUMBER);
		WaveTrains.Add(W);
	}
}

FVector AIH_P1C12_OceanPlane::SampleGerstner(
	const FIHGerstnerWave& W, float WorldX, float WorldY, float T) const
{
	const float k = 2.f * PI / W.WavelengthCm;
	const float c = W.PhaseSpeed * FMath::Sqrt(G_Gravity / k);
	const float om = k * c;
	const float ph = k * (W.Direction.X * WorldX + W.Direction.Y * WorldY) - om * T;
	const float S = FMath::Sin(ph);
	const float C = FMath::Cos(ph);
	const float Q = W.Steepness / (k * W.AmplitudeCm + SMALL_NUMBER);

	return FVector(
		-Q * W.AmplitudeCm * W.Direction.X * S,
		-Q * W.AmplitudeCm * W.Direction.Y * S,
		W.AmplitudeCm * C);
}

FVector AIH_P1C12_OceanPlane::ComputeNormal(float WorldX, float WorldY, float T) const
{
	const float Eps = 50.f;
	FVector dX = FVector::ZeroVector;
	FVector dY = FVector::ZeroVector;
	for (const FIHGerstnerWave& W : WaveTrains)
	{
		dX += SampleGerstner(W, WorldX + Eps, WorldY, T) - SampleGerstner(W, WorldX - Eps, WorldY, T);
		dY += SampleGerstner(W, WorldX, WorldY + Eps, T) - SampleGerstner(W, WorldX, WorldY - Eps, T);
	}
	const FVector TX = FVector(2.f * Eps, 0.f, dX.Z) / (2.f * Eps);
	const FVector TY = FVector(0.f, 2.f * Eps, dY.Z) / (2.f * Eps);
	return FVector::CrossProduct(TX, TY).GetSafeNormal();
}

void AIH_P1C12_OceanPlane::TickWaveDisplacement(float T, float FadeIn)
{
	const int32 N = CachedN;
	const int32 Num = (N + 1) * (N + 1);
	const FVector ActorLoc = GetActorLocation();

	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	Verts.SetNumUninitialized(Num);
	Normals.SetNumUninitialized(Num);
	UVs.SetNumUninitialized(Num);
	Colors.SetNumUninitialized(Num);
	Tangents.SetNumUninitialized(Num);

	for (int32 i = 0; i <= N; i++)
	{
		for (int32 j = 0; j <= N; j++)
		{
			const int32 Idx = i * (N + 1) + j;
			const FVector2D Local = BaseXY[Idx];
			// World-XY Gerstner: translating the tile does not slide the wave field.
			const float WorldX = ActorLoc.X + Local.X;
			const float WorldY = ActorLoc.Y + Local.Y;

			FVector Disp = FVector::ZeroVector;
			for (const FIHGerstnerWave& W : WaveTrains)
			{
				Disp += SampleGerstner(W, WorldX, WorldY, T);
			}
			Disp *= FadeIn;

			Verts[Idx] = FVector(Local.X + Disp.X, Local.Y + Disp.Y, Disp.Z);

			const FVector N3 = ComputeNormal(WorldX, WorldY, T);
			Normals[Idx] = FMath::Lerp(FVector(0.f, 0.f, 1.f), N3, FadeIn);

			UVs[Idx] = FVector2D(static_cast<float>(j) / N, static_cast<float>(i) / N);
			Colors[Idx] = FColor::White;
			Tangents[Idx] = FProcMeshTangent(1.f, 0.f, 0.f);
		}
	}

	OceanMesh->UpdateMeshSection(0, Verts, Normals, UVs, Colors, Tangents);
}

void AIH_P1C12_OceanPlane::RebuildHorizonSkirt()
{
	if (!HorizonSkirtMesh)
	{
		return;
	}

	HorizonSkirtMesh->ClearAllMeshSections();

	const float E = FMath::Max(HorizonSkirtHalfExtentCm, PlaneHalfExtentCm * 2.f);
	const float Z = HorizonSkirtZOffsetCm;

	TArray<FVector> Verts = {
		FVector(-E, -E, Z),
		FVector(E, -E, Z),
		FVector(-E, E, Z),
		FVector(E, E, Z),
	};
	TArray<int32> Tris = { 0, 2, 1, 1, 2, 3 };
	TArray<int32> InvTris = { 0, 1, 2, 1, 3, 2 };
	TArray<FVector> Normals = {
		FVector(0.f, 0.f, 1.f), FVector(0.f, 0.f, 1.f),
		FVector(0.f, 0.f, 1.f), FVector(0.f, 0.f, 1.f),
	};
	TArray<FVector> InvNormals = {
		FVector(0.f, 0.f, -1.f), FVector(0.f, 0.f, -1.f),
		FVector(0.f, 0.f, -1.f), FVector(0.f, 0.f, -1.f),
	};
	TArray<FVector2D> UVs = {
		FVector2D(0.f, 0.f), FVector2D(1.f, 0.f),
		FVector2D(0.f, 1.f), FVector2D(1.f, 1.f),
	};
	TArray<FColor> Colors;
	Colors.Init(FColor::White, 4);
	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), 4);

	HorizonSkirtMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	HorizonSkirtMesh->CreateMeshSection(1, Verts, InvTris, InvNormals, UVs, Colors, Tangents, false);
}

void AIH_P1C12_OceanPlane::ApplyOceanMaterials()
{
	UMaterialInterface* Mat = OceanMaterial
		? OceanMaterial.Get()
		: (BuildPlaceholderOceanMaterial(), PlaceholderMID ? PlaceholderMID.Get() : nullptr);
	if (!Mat)
	{
		return;
	}

	if (OceanMesh)
	{
		OceanMesh->SetMaterial(0, Mat);
		OceanMesh->SetMaterial(1, Mat);
		OceanMesh->SetTranslucentSortPriority(10);
	}
	if (HorizonSkirtMesh)
	{
		HorizonSkirtMesh->SetMaterial(0, Mat);
		HorizonSkirtMesh->SetMaterial(1, Mat);
		HorizonSkirtMesh->SetTranslucentSortPriority(8);
	}
}

void AIH_P1C12_OceanPlane::RebuildOceanMesh()
{
	if (!OceanMesh)
	{
		return;
	}
	OceanMesh->ClearAllMeshSections();
	BaseXY.Reset();

	const int32 N = FMath::Max(8, GridDivisions);
	CachedN = N;
	const int32 Num = (N + 1) * (N + 1);
	const float Step = (2.f * PlaneHalfExtentCm) / N;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.SetNumUninitialized(Num);
	Normals.SetNumUninitialized(Num);
	UVs.SetNumUninitialized(Num);
	VertexColors.SetNumUninitialized(Num);
	Tangents.SetNumUninitialized(Num);
	BaseXY.SetNumUninitialized(Num);

	for (int32 i = 0; i <= N; i++)
	{
		for (int32 j = 0; j <= N; j++)
		{
			const int32 Idx = i * (N + 1) + j;
			const float X = -PlaneHalfExtentCm + j * Step;
			const float Y = -PlaneHalfExtentCm + i * Step;
			Vertices[Idx] = FVector(X, Y, 0.f);
			BaseXY[Idx] = FVector2D(X, Y);
			Normals[Idx] = FVector(0.f, 0.f, 1.f);
			UVs[Idx] = FVector2D(static_cast<float>(j) / N, static_cast<float>(i) / N);
			VertexColors[Idx] = FColor::White;
			Tangents[Idx] = FProcMeshTangent(1.f, 0.f, 0.f);
		}
	}

	Triangles.Reserve(N * N * 6);
	for (int32 i = 0; i < N; i++)
	{
		for (int32 j = 0; j < N; j++)
		{
			const int32 A = i * (N + 1) + j;
			const int32 B = A + 1;
			const int32 C = A + (N + 1);
			const int32 D = C + 1;
			Triangles.Append({ A, C, B, B, C, D });
		}
	}

	OceanMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs,
		VertexColors, Tangents, /*bCreateCollision=*/true);

	TArray<int32> InvTris;
	InvTris.Reserve(Triangles.Num());
	for (int32 t = 0; t < Triangles.Num(); t += 3)
	{
		InvTris.Add(Triangles[t]);
		InvTris.Add(Triangles[t + 2]);
		InvTris.Add(Triangles[t + 1]);
	}
	TArray<FVector> InvNormals;
	InvNormals.Reserve(Normals.Num());
	for (const FVector& Nrm : Normals)
	{
		InvNormals.Add(-Nrm);
	}

	OceanMesh->CreateMeshSection(1, Vertices, InvTris, InvNormals, UVs,
		VertexColors, Tangents, /*bCreateCollision=*/false);

	RebuildHorizonSkirt();
	ApplyOceanMaterials();

	const float CellCm = GetGridCellCm();
	const float NyquistCm = 2.f * CellCm;
	constexpr float HullLengthCm = 3780.f; // Merchantman TargetHullLengthCm
	const float PeriodsSec[4] = { 3.2f, 2.5f, 2.0f, 1.5f };
	float AmpSumCm = 0.f;
	FString WaveLog;
	for (int32 i = 0; i < WaveTrains.Num(); ++i)
	{
		const FIHGerstnerWave& W = WaveTrains[i];
		AmpSumCm += W.AmplitudeCm;
		const float Margin = W.WavelengthCm / FMath::Max(NyquistCm, 1.f);
		const float HullMul = W.WavelengthCm / HullLengthCm;
		const float Period = (i < 4) ? PeriodsSec[i] : 0.f;
		WaveLog += FString::Printf(
			TEXT(" λ%d=%.0fm(%.1f×hull/%.1f×Nyq T=%.1fs A=%.0fcm)"),
			i, W.WavelengthCm / 100.f, HullMul, Margin, Period, W.AmplitudeCm);
	}

	UE_LOG(LogIH_OceanPlane, Log,
		TEXT("IH_P1C12_OceanPlane: hullProp endless=%d half=%.1fkm skirt=%.0fkm opacity=%.2f cell=%.0fm div=%d verts=%d ampSum=%.0fcm%s"),
		bEndlessSeaFollowCamera ? 1 : 0,
		PlaneHalfExtentCm / 100000.f,
		HorizonSkirtHalfExtentCm / 100000.f,
		OceanOpacity,
		CellCm / 100.f,
		N,
		Num,
		AmpSumCm,
		*WaveLog);
}

void AIH_P1C12_OceanPlane::BuildPlaceholderOceanMaterial()
{
	if (PlaceholderMID)
	{
		const FLinearColor Tint(0.01f, 0.07f, 0.25f, OceanOpacity);
		PlaceholderMID->SetVectorParameterValue(FName("Color"), Tint);
		PlaceholderMID->SetVectorParameterValue(FName("BaseColor"), Tint);
		PlaceholderMID->SetScalarParameterValue(FName("Opacity"), OceanOpacity);
		return;
	}

	// FlattenMaterial supports Opacity (BasicShapeMaterial is opaque).
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"));
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (!Base)
	{
		return;
	}

	PlaceholderMID = UMaterialInstanceDynamic::Create(Base, this);
	if (!PlaceholderMID)
	{
		return;
	}

	const FLinearColor Tint(0.01f, 0.07f, 0.25f, OceanOpacity);
	PlaceholderMID->SetVectorParameterValue(FName("Color"), Tint);
	PlaceholderMID->SetVectorParameterValue(FName("BaseColor"), Tint);
	PlaceholderMID->SetVectorParameterValue(FName("TintColor"), Tint);
	PlaceholderMID->SetScalarParameterValue(FName("Opacity"), OceanOpacity);
	PlaceholderMID->SetScalarParameterValue(FName("Roughness"), 0.08f);
	PlaceholderMID->SetScalarParameterValue(FName("Metallic"), 0.0f);
}
