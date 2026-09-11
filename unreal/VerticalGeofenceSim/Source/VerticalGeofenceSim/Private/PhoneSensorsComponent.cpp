#include "PhoneSensorsComponent.h"
#include "SimWorker.h"
#include "SimBeacon.h"
#include "SimBuilding.h"
#include "SimConfig.h"
#include "SimSubsystem.h"
#include "SimBridge.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

#define ECC_Radio ECC_GameTraceChannel1

UPhoneSensorsComponent::UPhoneSensorsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ASimWorker* UPhoneSensorsComponent::Owner() const
{
	return Cast<ASimWorker>(GetOwner());
}

void UPhoneSensorsComponent::ResetCadence(USimConfig* Config)
{
	const ASimWorker* W = Owner();
	if (!W || !Config)
	{
		return;
	}
	const float Period = Config->ScanPeriod(W->Platform, W->AppState);
	// Stagger the first scan by the worker's index so 8 phones don't all fire on the same step.
	NextScanAt = Period > 0.f ? Config->SimTime + Period : TNumericLimits<float>::Max();
	NextRegionAt = Period > 0.f ? TNumericLimits<float>::Max() : Config->SimTime + 1.f;
}

void UPhoneSensorsComponent::Step(float Dt, USimConfig* Config)
{
	if (Config->SimTime >= NextScanAt)
	{
		DoScan(Config);
		const float Period = Config->ScanPeriod(Owner()->Platform, Owner()->AppState);
		NextScanAt = Period > 0.f ? NextScanAt + Period : TNumericLimits<float>::Max();
	}
	if (Config->SimTime >= NextRegionAt)
	{
		DoRegionCheck(Config);
		NextRegionAt += 1.f;
	}
}

float UPhoneSensorsComponent::ComputeRssi(const ASimBeacon* Beacon, const FVector& Phone, USimConfig* Config, bool& bHeard, int32* OutCrossings)
{
	bHeard = false;
	if (!Beacon || !Beacon->IsTransmitting())
	{
		return -200.f;
	}
	const FVector BeaconLoc = Beacon->GetActorLocation();
	const float DistM = FMath::Max(FVector::Dist(BeaconLoc, Phone) / 100.f, 0.5f);
	if (DistM > Config->MaxRangeM)
	{
		return -200.f;
	}

	// Every tagged mesh the straight line crosses costs its material loss (once per actor).
	float MaterialLoss = 0.f;
	int32 Crossings = 0;
	if (UWorld* World = GetWorld())
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RadioTrace), false);
		Params.AddIgnoredActor(Beacon);
		Params.AddIgnoredActor(GetOwner());
		World->LineTraceMultiByChannel(Hits, BeaconLoc, Phone, ECC_Radio, Params);
		TSet<const AActor*> Seen;
		for (const FHitResult& Hit : Hits)
		{
			const AActor* A = Hit.GetActor();
			if (!A || Seen.Contains(A))
			{
				continue;
			}
			Seen.Add(A);
			for (const FName& Tag : A->Tags)
			{
				const float Loss = Config ? Config->SlabLossDb * (Tag == TEXT("Concrete")) +
				                            Config->DrywallLossDb * (Tag == TEXT("Drywall")) +
				                            Config->GlassLossDb * (Tag == TEXT("Glass")) : 0.f;
				if (Loss > 0.f)
				{
					MaterialLoss += Loss;
					Crossings++;
					break;
				}
			}
		}
	}
	if (OutCrossings)
	{
		*OutCrossings = Crossings;
	}

	float& F = Fade.FindOrAdd(Beacon->BeaconId);
	F = FMath::Clamp(F + Config->RandNormal(Config->FadeStepDb), -Config->FadeClampDb, Config->FadeClampDb);

	const float BodyLoss = Owner() && Owner()->PhoneState == EPhoneState::InPocket ? Config->BodyLossDb : 0.f;
	const float Rssi = Beacon->TxPowerDbm
	                 - 10.f * Config->PathLossExponent * FMath::LogX(10.f, DistM)
	                 - MaterialLoss - BodyLoss - F
	                 + Config->RandNormal(Config->RssiSigmaDb);

	if (Config->RandBool(Config->PacketLoss) || Rssi < Config->RxSensitivityDbm)
	{
		return Rssi;
	}
	bHeard = true;
	return Rssi;
}

float UPhoneSensorsComponent::ReadPressure(USimConfig* Config) const
{
	const ASimWorker* W = Owner();
	const float ZM = W ? W->GetActorLocation().Z / 100.f : 0.f;
	float Gust = 0.f;
	if (W && W->GetBuilding())
	{
		Gust = W->GetBuilding()->GustOffsetFor(W->TruthFloor);
	}
	return Config->SitePressure - ZM * Config->PressurePerMetre + Config->WeatherDriftHpa + Gust + Config->RandNormal(Config->BaroSigmaHpa);
}

void UPhoneSensorsComponent::DoScan(USimConfig* Config)
{
	ASimWorker* W = Owner();
	if (!W || !W->GetBuilding())
	{
		return;
	}
	USimSubsystem* S = USimSubsystem::Get(this);
	const FVector Phone = W->GetActorLocation();
	const bool bDebug = Config->bDebugTraces && W->GetBuilding()->GetSelectedWorker() == W;

	LastScans.Reset();
	for (const ASimBeacon* B : W->GetBuilding()->GetBeacons())
	{
		bool bHeard = false;
		int32 Crossings = 0;
		const float Rssi = ComputeRssi(B, Phone, Config, bHeard, &Crossings);
		if (bHeard)
		{
			LastScans.Add({B->BeaconId, FMath::RoundToInt(Rssi)});
		}
		if (bDebug && Rssi > -200.f)
		{
			DrawDebugLine(GetWorld(), B->GetActorLocation(), Phone, bHeard ? (Crossings > 0 ? FColor::Orange : FColor::Green) : FColor::Red,
			              false, Config->ScanPeriod(W->Platform, W->AppState), 0, bHeard ? 2.f : 0.5f);
		}
	}
	if (S)
	{
		const float Ref = W->GetBuilding()->ReadReferencePressure();
		S->GetBridge()->SendScan(W, LastScans, {}, ReadPressure(Config), Ref);
	}
}

void UPhoneSensorsComponent::DoRegionCheck(USimConfig* Config)
{
	ASimWorker* W = Owner();
	if (!W || !W->GetBuilding())
	{
		return;
	}
	const FVector Phone = W->GetActorLocation();
	TArray<FRegionEvent> Events;
	for (const ASimBeacon* B : W->GetBuilding()->GetBeacons())
	{
		bool bHeard = false;
		ComputeRssi(B, Phone, Config, bHeard);
		const bool bWas = InRegion.Contains(B->BeaconId);
		if (bHeard && !bWas)
		{
			InRegion.Add(B->BeaconId);
			Events.Add({B->BeaconId, TEXT("enter")});
		}
		else if (!bHeard && bWas)
		{
			InRegion.Remove(B->BeaconId);
			Events.Add({B->BeaconId, TEXT("exit")});
		}
	}
	if (Events.Num() > 0)
	{
		if (USimSubsystem* S = USimSubsystem::Get(this))
		{
			S->GetBridge()->SendScan(W, {}, Events, ReadPressure(Config), W->GetBuilding()->ReadReferencePressure());
		}
	}
}
