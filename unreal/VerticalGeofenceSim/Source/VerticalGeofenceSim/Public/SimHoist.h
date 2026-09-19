// The construction hoist: a platform that serves floor requests in order, carrying attached workers.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimHoist.generated.h"

class UStaticMeshComponent;
class ASimWorker;
class USimConfig;

UENUM(BlueprintType)
enum class EHoistState : uint8 { Idle, Moving, Dwell };

UCLASS()
class VERTICALGEOFENCESIM_API ASimHoist : public AActor
{
	GENERATED_BODY()

public:
	ASimHoist();

	/** A worker standing at the hoist door on their current floor wants to go to Floor. */
	UFUNCTION(BlueprintCallable, Category = "Hoist") void Request(ASimWorker* Worker, int32 Floor);

	/** Advance the state machine by one fixed step. Called by ASimBuilding. */
	void Step(float Dt, USimConfig* Config);

	UFUNCTION(BlueprintPure, Category = "Hoist") int32 GetCurrentFloor() const { return CurrentFloor; }
	UFUNCTION(BlueprintPure, Category = "Hoist") EHoistState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "Hoist") FVector GetCarLocation() const { return GetActorLocation(); }

	/** World XY of the car centre (metres), set by the building from the shaft rectangle. */
	FVector2D CarXY = FVector2D::ZeroVector;
	/** Shaft depth (metres) so the door position can be derived. */
	float ShaftDepthM = 3.f;
	/** Where riders wait / step off: just south of the shaft, at floor level (world cm, XY only). */
	FVector2D DoorXYcm() const { return FVector2D(CarXY.X * 100.f, (CarXY.Y - ShaftDepthM / 2.f - 1.0f) * 100.f); }

private:
	void BoardWaitingWorkers();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Car;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> CageParts;

	EHoistState State = EHoistState::Idle;
	int32 CurrentFloor = 0;
	int32 TargetFloor = 0;
	float DwellRemaining = 0.f;
	float ZCm = 0.f;

	TArray<int32> Queue;
	/** Waiting or riding workers -> destination floor. */
	TMap<TWeakObjectPtr<ASimWorker>, int32> Waiting;
	TArray<TWeakObjectPtr<ASimWorker>> Riders;
};
