// A BLE beacon: a position with radio properties. No ticking; workers query it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimTypes.h"
#include "SimBeacon.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

UCLASS()
class VERTICALGEOFENCESIM_API ASimBeacon : public AActor
{
	GENERATED_BODY()

public:
	ASimBeacon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beacon") FName BeaconId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Beacon") int32 Floor = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon") float TxPowerDbm = -59.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon") int32 AdvIntervalMs = 300;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon") float Battery = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon") bool bAlive = true;

	void InitFromRow(const FBeaconRow& Row);

	UFUNCTION(BlueprintPure, Category = "Beacon") EBeaconState GetState() const;
	/** True if the beacon is currently transmitting at all. */
	UFUNCTION(BlueprintPure, Category = "Beacon") bool IsTransmitting() const { return bAlive && Battery > 0.f; }

	UFUNCTION(BlueprintCallable, Category = "Beacon") void Kill();
	UFUNCTION(BlueprintCallable, Category = "Beacon") void Revive();
	UFUNCTION(BlueprintCallable, Category = "Beacon") void SetBattery(float NewBattery);
	/** Move by a random horizontal offset of the given length (someone kicked it down the corridor). */
	UFUNCTION(BlueprintCallable, Category = "Beacon") void Nudge(float Metres);

	UFUNCTION(BlueprintCallable, Category = "Beacon") void RefreshVisual();

private:
	void Announce(const FString& EventName) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatAlive;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatLow;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatDead;
};

/** The lobby barometer that the estimator uses for differential altimetry. */
UCLASS()
class VERTICALGEOFENCESIM_API ASimReferenceStation : public AActor
{
	GENERATED_BODY()

public:
	ASimReferenceStation();

	/** Site pressure + shared weather drift + sensor noise. */
	UFUNCTION(BlueprintCallable, Category = "Barometer") float ReadPressure() const;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
};
