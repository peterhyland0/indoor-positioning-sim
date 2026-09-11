#include "SimSubsystem.h"
#include "SimConfig.h"
#include "SimBridge.h"
#include "VerticalGeofenceSim.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

static const TCHAR* GConfigAssetPath = TEXT("/Game/Sim/Bridge/DA_SimConfig.DA_SimConfig");

void USimSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetConfig();
	Bridge = NewObject<USimBridge>(this);
}

void USimSubsystem::Deinitialize()
{
	if (Bridge)
	{
		Bridge->Shutdown();
	}
	Super::Deinitialize();
}

void USimSubsystem::ResetConfig()
{
	USimConfig* Asset = LoadObject<USimConfig>(nullptr, GConfigAssetPath);
	if (Asset)
	{
		Config = DuplicateObject<USimConfig>(Asset, this);
	}
	else
	{
		UE_LOG(LogSim, Warning, TEXT("%s not found; using built-in defaults"), GConfigAssetPath);
		Config = NewObject<USimConfig>(this);
	}
	Config->ResetRun();
}

USimSubsystem* USimSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<USimSubsystem>() : nullptr;
}
