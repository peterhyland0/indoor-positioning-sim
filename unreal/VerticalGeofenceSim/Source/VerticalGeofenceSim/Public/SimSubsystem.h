// Game-instance-lifetime owner of the live config copy and the bridge.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SimSubsystem.generated.h"

class USimConfig;
class USimBridge;

UCLASS()
class VERTICALGEOFENCESIM_API USimSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Runtime copy of DA_SimConfig (edits during play never dirty the asset). */
	UFUNCTION(BlueprintPure, Category = "Sim") USimConfig* GetConfig() const { return Config; }
	UFUNCTION(BlueprintPure, Category = "Sim") USimBridge* GetBridge() const { return Bridge; }

	/** Reload the asset into a fresh runtime copy and reseed. Called by ASimBuilding on BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Sim") void ResetConfig();

	/** Convenience for anything with a world context. */
	UFUNCTION(BlueprintPure, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static USimSubsystem* Get(const UObject* WorldContextObject);

private:
	UPROPERTY() TObjectPtr<USimConfig> Config;
	UPROPERTY() TObjectPtr<USimBridge> Bridge;
};
