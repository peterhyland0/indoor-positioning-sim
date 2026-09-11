// Every tunable of the simulation plus the single seeded random stream.
// One asset (Content/Sim/Bridge/DA_SimConfig) is referenced by the game instance; everything reads from it.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SimTypes.h"
#include "SimConfig.generated.h"

UCLASS(BlueprintType)
class VERTICALGEOFENCESIM_API USimConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Building -------------------------------------------------------------------------
	/** Floors above the lobby (lobby = 0). Must match the generated L_Tower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") int32 NumFloors = 15;
	/** Slab-to-slab, metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") float FloorHeight = 3.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") float SlabThickness = 0.25f;

	// ---- RF model ---------------------------------------------------------------------------
	/** Reinforced concrete slab at 2.4 GHz; published 15-25 dB. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float SlabLossDb = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float DrywallLossDb = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float GlassLossDb = 2.f;
	/** Phone in pocket, body between phone and beacon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float BodyLossDb = 8.f;
	/** Log-distance exponent n; ~2.2 for near-LOS open slabs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float PathLossExponent = 2.2f;
	/** Per-scan Gaussian RSSI noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float RssiSigmaDb = 5.f;
	/** Bound of the slow per-(worker,beacon) fade random walk (multipath stand-in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float FadeClampDb = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float FadeStepDb = 0.5f;
	/** Probability a heard advert is dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF", meta = (ClampMin = 0, ClampMax = 1)) float PacketLoss = 0.10f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float MaxRangeM = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF") float RxSensitivityDbm = -95.f;

	// ---- Barometer --------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barometer") float SitePressure = 1013.25f;
	/** hPa per metre of altitude (~1/8.3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barometer") float PressurePerMetre = 0.1205f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barometer") float BaroSigmaHpa = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barometer") bool bWeatherDrift = true;
	/** Random-walk step per second of the shared weather drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barometer") float WeatherStepHpa = 0.005f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barometer") float GustHpa = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barometer") float GustSec = 5.f;

	// ---- Movement ---------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float HoistSpeed = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float HoistDwellSec = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float WalkSpeed = 1.4f;

	// ---- Scan cadence (seconds; 0 = no periodic scan, region events only) --------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan") float ScanAndroidFg = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan") float ScanAndroidBg = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan") float ScanIosFg = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan") float ScanIosBg = 0.f;

	// ---- Run --------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run") int32 Seed = 42;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run") FString BridgeUrl = TEXT("ws://localhost:8080");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run") bool bDebugTraces = false;

	// ---- Live state (not saved; reset by ResetRun) -------------------------------------------
	UPROPERTY(Transient, BlueprintReadOnly, Category = "State") FRandomStream Rng;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "State") float WeatherDriftHpa = 0.f;
	UPROPERTY(Transient, BlueprintReadWrite, Category = "State") float SimTime = 0.f;
	UPROPERTY(Transient, BlueprintReadWrite, Category = "State") bool bPaused = false;

	/** Reseed the stream and zero the live state. Call once per run. */
	UFUNCTION(BlueprintCallable, Category = "Sim") void ResetRun();

	/** Gaussian sample from the shared stream (Box-Muller). */
	UFUNCTION(BlueprintCallable, Category = "Sim") float RandNormal(float Sigma);
	/** Bernoulli sample from the shared stream. */
	UFUNCTION(BlueprintCallable, Category = "Sim") bool RandBool(float P);
	/** Uniform [0,1) from the shared stream. */
	UFUNCTION(BlueprintCallable, Category = "Sim") float RandUnit();

	/** Advance the shared weather random walk by Dt seconds. */
	UFUNCTION(BlueprintCallable, Category = "Sim") void StepWeather(float Dt);

	/** Scan period for a platform/app-state pair; 0 means region events only. */
	UFUNCTION(BlueprintPure, Category = "Sim") float ScanPeriod(EPlatform Platform, EAppState AppState) const;

	/** Walking surface height of a floor, in world cm. */
	UFUNCTION(BlueprintPure, Category = "Sim") float FloorZCm(int32 Floor) const { return Floor * FloorHeight * 100.f; }
	/** Nearest floor for a world Z in cm. */
	UFUNCTION(BlueprintPure, Category = "Sim") int32 FloorFromZCm(float ZCm) const { return FMath::RoundToInt(ZCm / 100.f / FloorHeight); }
};
