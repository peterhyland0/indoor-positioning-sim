#include "SimConfig.h"
#include "SimTypes.h"

void USimConfig::ResetRun()
{
	Rng.Initialize(Seed);
	WeatherDriftHpa = 0.f;
	SimTime = 0.f;
	bPaused = false;
}

float USimConfig::RandUnit()
{
	return Rng.FRand();
}

float USimConfig::RandNormal(float Sigma)
{
	const float U1 = FMath::Max(Rng.FRand(), 1e-9f);
	const float U2 = Rng.FRand();
	return FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2) * Sigma;
}

bool USimConfig::RandBool(float P)
{
	return Rng.FRand() < P;
}

void USimConfig::StepWeather(float Dt)
{
	if (!bWeatherDrift || Dt <= 0.f)
	{
		return;
	}
	WeatherDriftHpa = FMath::Clamp(WeatherDriftHpa + RandNormal(WeatherStepHpa * FMath::Sqrt(Dt)), -3.f, 3.f);
}

float USimConfig::ScanPeriod(EPlatform Platform, EAppState AppState) const
{
	const bool bFg = AppState == EAppState::Foreground;
	switch (Platform)
	{
	case EPlatform::iOS:     return bFg ? ScanIosFg : ScanIosBg;
	case EPlatform::Android: return bFg ? ScanAndroidFg : ScanAndroidBg;
	default:                 return 1.f;
	}
}
