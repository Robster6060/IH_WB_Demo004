// Copyright Invisible Hand. All Rights Reserved.

#include "IH_WB_Demo004.h"
#include "GameMapsSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "IH_WB_Demo004GameMode.h"
#include "IH_WB_Demo004GameInstance.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogIH_WB_Demo004);

class FIH_WB_Demo004Module final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		if (UClass* GMClass = AIH_WB_Demo004GameMode::StaticClass())
		{
			UGameMapsSettings::SetGlobalDefaultGameMode(FSoftClassPath(GMClass).ToString());
		}
		if (UClass* GIClass = UIH_WB_Demo004GameInstance::StaticClass())
		{
			if (UGameMapsSettings* MapsSettings = GetMutableDefault<UGameMapsSettings>())
			{
				MapsSettings->GameInstanceClass = FSoftClassPath(GIClass);
			}
		}

		WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(
			this, &FIH_WB_Demo004Module::OnPostWorldInitialization);
	}

	virtual void ShutdownModule() override
	{
		if (WorldInitHandle.IsValid())
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
			WorldInitHandle.Reset();
		}
	}

private:
	void OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
	{
		(void)IVS;
		if (!World || !World->IsGameWorld())
		{
			return;
		}
		AWorldSettings* WS = World->GetWorldSettings();
		if (!WS)
		{
			return;
		}
		if (WS->DefaultGameMode == AGameModeBase::StaticClass())
		{
			WS->DefaultGameMode = AIH_WB_Demo004GameMode::StaticClass();
		}
	}

	FDelegateHandle WorldInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FIH_WB_Demo004Module, IH_WB_Demo004, "IH_WB_Demo004");
