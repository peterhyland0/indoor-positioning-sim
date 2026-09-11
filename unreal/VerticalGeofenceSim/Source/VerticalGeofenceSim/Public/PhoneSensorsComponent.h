// The phone: BLE scanner + barometer. Produces exactly what a real handset would report.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimTypes.h"
#include "PhoneSensorsComponent.generated.h"

class ASimWorker;
class ASimBeacon;
class USimConfig;

UCLASS(ClassGroup = (Sim), meta = (BlueprintSpawnableComponent))
class VERTICALGEOFENCESIM_API UPhoneSensorsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhoneSensorsComponent();

	/** Advance by one fixed step; fires scans when due. */
	void Step(float Dt, USimConfig* Config);
	/** Call after Platform/AppState change so the cadence updates. */
	void ResetCadence(USimConfig* Config);

	/** RSSI a phone at PhoneLocation would see from Beacon, and whether it is heard at all. */
	float ComputeRssi(const ASimBeacon* Beacon, const FVector& PhoneLocation, USimConfig* Config, bool& bHeard, int32* OutCrossings = nullptr);
	/** Barometer reading at the phone's current height. */
	float ReadPressure(USimConfig* Config) const;

	/** The last scan, for the HUD. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Phone") TArray<FScan> LastScans;

private:
	void DoScan(USimConfig* Config);
	void DoRegionCheck(USimConfig* Config);

	ASimWorker* Owner() const;

	float NextScanAt = 0.f;
	float NextRegionAt = 0.f;
	/** Slow random-walk fade per beacon (multipath stand-in). */
	TMap<FName, float> Fade;
	/** Beacons currently "in region" for iOS background emulation. */
	TSet<FName> InRegion;
};
