#include "SimBeacon.h"
#include "SimConfig.h"
#include "SimSubsystem.h"
#include "SimBridge.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASimBeacon::ASimBeacon()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.45f));
	Mesh->SetCollisionProfileName(TEXT("RadioTransparent"));
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> A(TEXT("/Game/Sim/Materials/M_BeaconAlive"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> L(TEXT("/Game/Sim/Materials/M_BeaconLow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> D(TEXT("/Game/Sim/Materials/M_BeaconDead"));
	MatAlive = A.Object;
	MatLow = L.Object;
	MatDead = D.Object;

	Tags.Add(TEXT("Beacon"));
}

void ASimBeacon::InitFromRow(const FBeaconRow& Row)
{
	BeaconId = Row.BeaconId;
	Floor = Row.Floor;
	TxPowerDbm = Row.TxPowerDbm;
	AdvIntervalMs = Row.AdvIntervalMs;
	Battery = Row.Battery;
	bAlive = Row.Alive;
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("Beacon_%s"), *BeaconId.ToString()));
#endif
	RefreshVisual();
}

EBeaconState ASimBeacon::GetState() const
{
	if (!bAlive || Battery <= 0.f) return EBeaconState::Dead;
	if (Battery < 0.2f)            return EBeaconState::LowBattery;
	return EBeaconState::Alive;
}

void ASimBeacon::RefreshVisual()
{
	UMaterialInterface* M = MatAlive;
	switch (GetState())
	{
	case EBeaconState::LowBattery: M = MatLow; break;
	case EBeaconState::Dead:       M = MatDead; break;
	default: break;
	}
	if (M)
	{
		Mesh->SetMaterial(0, M);
	}
}

void ASimBeacon::Announce(const FString& EventName) const
{
	if (USimSubsystem* S = USimSubsystem::Get(this))
	{
		const FVector L = GetActorLocation() / 100.f;
		S->GetBridge()->SendEvent(EventName, FString::Printf(
			TEXT("{\"b\":\"%s\",\"battery\":%.2f,\"alive\":%s,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}"),
			*BeaconId.ToString(), Battery, bAlive ? TEXT("true") : TEXT("false"), L.X, L.Y, L.Z));
	}
}

void ASimBeacon::Kill()            { bAlive = false; RefreshVisual(); Announce(TEXT("beaconKilled")); }
void ASimBeacon::Revive()          { bAlive = true; Battery = FMath::Max(Battery, 0.05f); RefreshVisual(); Announce(TEXT("beaconRevived")); }
void ASimBeacon::SetBattery(float B){ Battery = FMath::Clamp(B, 0.f, 1.f); RefreshVisual(); Announce(TEXT("beaconBattery")); }

void ASimBeacon::Nudge(float Metres)
{
	USimSubsystem* S = USimSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	const float Angle = S->GetConfig()->RandUnit() * 2.f * PI;
	AddActorWorldOffset(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Metres * 100.f);
	Announce(TEXT("beaconMoved"));
}

// ---------------------------------------------------------------- reference station

ASimReferenceStation::ASimReferenceStation()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Sim/Materials/M_Reference"));
	if (CubeMesh.Succeeded()) Mesh->SetStaticMesh(CubeMesh.Object);
	if (Mat.Succeeded())      Mesh->SetMaterial(0, Mat.Object);
	Mesh->SetRelativeScale3D(FVector(0.3f));
	Mesh->SetCollisionProfileName(TEXT("RadioTransparent"));
	Tags.Add(TEXT("ReferenceStation"));
}

float ASimReferenceStation::ReadPressure() const
{
	USimSubsystem* S = USimSubsystem::Get(this);
	if (!S)
	{
		return 1013.25f;
	}
	USimConfig* C = S->GetConfig();
	return C->SitePressure + C->WeatherDriftHpa + C->RandNormal(C->BaroSigmaHpa);
}
