// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_MerchantmanShipActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Modules/ModuleManager.h"

AIH_P1C07_MerchantmanShipActor::AIH_P1C07_MerchantmanShipActor()
{
	static const TCHAR* HullMeshPath =
		TEXT("/Game/InvisibleHand/Conveyances/Actors/Ships/Merchantman3UV_cleaned.Merchantman3UV_cleaned");
	if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, HullMeshPath))
	{
		if (Mesh)
		{
			Mesh->SetStaticMesh(SM);
		}
	}
}

void AIH_P1C07_MerchantmanShipActor::ResolveHullMesh()
{
	Super::ResolveHullMesh();

	if (Mesh && Mesh->GetStaticMesh())
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(TEXT("/Game/InvisibleHand/Conveyances/Actors/Ships")));
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);
	for (const FAssetData& AD : AssetList)
	{
		if (AD.IsValid() && AD.AssetName == FName(TEXT("Merchantman3UV_cleaned")))
		{
			if (UStaticMesh* SM = Cast<UStaticMesh>(AD.GetAsset()))
			{
				Mesh->SetStaticMesh(SM);
				return;
			}
		}
	}
}
