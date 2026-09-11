// The simulation runner. One placed in L_Tower. Spawns beacons, workers, hoist and reference
// station from the data tables, then advances everything in fixed steps.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimTypes.h"
#include "SimBuilding.generated.h"

class UDataTable;
class USimConfig;
class ASimBeacon;
class ASimWorker;
class ASimHoist;
class ASimReferenceStation;

UCLASS()
class VERTICALGEOFENCESIM_API ASimBuilding : public AActor
{
	GENERATED_BODY()

public:
	ASimBuilding();

	UPROPERTY(EditAnywhere, Category = "Data") TObjectPtr<UDataTable> BeaconLayout;
	UPROPERTY(EditAnywhere, Category = "Data") TObjectPtr<UDataTable> Shifts;
	UPROPERTY(EditAnywhere, Category = "Data") TObjectPtr<UDataTable> Routes;
	UPROPERTY(EditAnywhere, Category = "Data") TObjectPtr<UDataTable> Waypoints;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---- queries used by workers / sensors / bridge --------------------------------------
	UFUNCTION(BlueprintPure, Category = "Sim") USimConfig* GetConfig() const { return Config; }
	const TArray<ASimBeacon*>& GetBeacons() const { return Beacons; }
	const TArray<ASimWorker*>& GetWorkers() const { return Workers; }
	UFUNCTION(BlueprintPure, Category = "Sim") ASimBeacon* FindBeacon(FName Id) const;
	UFUNCTION(BlueprintPure, Category = "Sim") ASimWorker* FindWorker(FName Id) const;
	UFUNCTION(BlueprintPure, Category = "Sim") ASimHoist* GetHoist() const { return Hoist; }
	UFUNCTION(BlueprintPure, Category = "Sim") ASimWorker* GetSelectedWorker() const { return SelectedWorker; }

	/** World cm of a named waypoint on a floor (capsule centre height). */
	FVector WaypointWorld(FName Name, int32 Floor) const;
	FVector StairwellCentreCm() const;
	/** Keep a wander target inside the slab and out of the shaft/stair holes. */
	FVector ClampToFloorPlate(const FVector& Cm) const;
	TArray<FRouteRow> GetRoute(FName RouteName) const;
	float ReadReferencePressure() const;
	float GustOffsetFor(int32 Floor) const;

	// ---- controls (HUD / sabotage panel) ---------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Sim") void TogglePause();
	UFUNCTION(BlueprintCallable, Category = "Sim") void ToggleDebugTraces();
	UFUNCTION(BlueprintCallable, Category = "Sim") void SelectNextWorker();
	UFUNCTION(BlueprintCallable, Category = "Sim") void DoorSlam(int32 Floor);
	UFUNCTION(BlueprintCallable, Category = "Sim") void SetStorm(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Sim") float GetSimTime() const;
	UFUNCTION(BlueprintPure, Category = "Sim") int32 GetStepCount() const { return StepCount; }

private:
	void SpawnBeacons();
	void SpawnWorkers();
	void SpawnFixtures();
	void FixedStep();

	UPROPERTY() TObjectPtr<USimConfig> Config;
	UPROPERTY() TArray<TObjectPtr<ASimBeacon>> Beacons;
	UPROPERTY() TArray<TObjectPtr<ASimWorker>> Workers;
	UPROPERTY() TObjectPtr<ASimHoist> Hoist;
	UPROPERTY() TObjectPtr<ASimReferenceStation> Reference;
	UPROPERTY() TObjectPtr<ASimWorker> SelectedWorker;

	TMap<FName, ASimBeacon*> BeaconsById;
	TMap<FName, ASimWorker*> WorkersById;
	FTimerHandle StepTimer;
	int32 StepCount = 0;
	int32 GustFloor = -1;
	float GustUntil = 0.f;
	float RunSecondsLimit = 0.f;
};
