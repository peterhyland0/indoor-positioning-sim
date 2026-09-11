#include "SimWorker.h"
#include "SimBuilding.h"
#include "SimHoist.h"
#include "SimConfig.h"
#include "PhoneSensorsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CapsuleHalfHeightCm = 90.f;
	constexpr float StairsSpeedMps = 0.5f;   // vertical speed on stairs
	constexpr float ArriveToleranceCm = 30.f;
}

ASimWorker::ASimWorker()
{
	PrimaryActorTick.bCanEverTick = false;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(30.f, CapsuleHalfHeightCm);
	Capsule->SetCollisionProfileName(TEXT("RadioTransparent"));
	SetRootComponent(Capsule);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Sim/Materials/M_Worker"));
	if (Cyl.Succeeded()) Body->SetStaticMesh(Cyl.Object);
	if (Mat.Succeeded()) Body->SetMaterial(0, Mat.Object);
	Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.7f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -5.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Capsule);
	Label->SetRelativeLocation(FVector(0.f, -40.f, CapsuleHalfHeightCm + 40.f));
	Label->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // readable from the cutaway camera (looking +Y)
	Label->SetWorldSize(45.f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetTextRenderColor(FColor::White);

	Sensors = CreateDefaultSubobject<UPhoneSensorsComponent>(TEXT("Sensors"));

	Tags.Add(TEXT("Worker"));
}

void ASimWorker::InitFromShift(const FShiftRow& Row, ASimBuilding* InBuilding, ASimHoist* InHoist)
{
	WorkerId = Row.WorkerId;
	Platform = Row.Platform;
	PhoneState = Row.PhoneState;
	AppState = Row.AppState;
	RouteName = Row.RouteName;
	TargetFloor = Row.TargetFloor;
	Building = InBuilding;
	Hoist = InHoist;
	Segments = Building->GetRoute(RouteName);
	SegIndex = -1;
	Mode = EWorkerMode::Waiting;
	Timer = Row.StartOffsetSec; // hold at the entrance until the shift starts
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("Worker_%s"), *WorkerId.ToString()));
#endif
	TruthFloor = 0;
	RefreshLabel();
}

void ASimWorker::RefreshLabel()
{
	FString Text = FString::Printf(TEXT("%s  F%d"), *WorkerId.ToString(), TruthFloor);
	if (bHasEstimate)
	{
		Text += FString::Printf(TEXT("\nest F%d"), EstFloor);
	}
	if (PunchFlashRemaining > 0.f)
	{
		Text += TEXT("\n") + PunchFlash;
	}
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(!bHasEstimate ? FColor::White : (EstFloor == TruthFloor ? FColor::Green : FColor::Red));
}

void ASimWorker::ApplyEstimate(int32 InEstFloor, const FString& Punch)
{
	EstFloor = InEstFloor;
	bHasEstimate = InEstFloor >= 0;
	if (!Punch.IsEmpty() && Punch != TEXT("null"))
	{
		PunchFlash = Punch == TEXT("in") ? TEXT("CLOCK IN") : TEXT("CLOCK OUT");
		PunchFlashRemaining = 1.5f;
	}
	RefreshLabel();
}

void ASimWorker::SetPhoneState(EPhoneState NewState)
{
	PhoneState = NewState;
}

void ASimWorker::SetAppState(EAppState NewState)
{
	AppState = NewState;
	if (Building)
	{
		Sensors->ResetCadence(Building->GetConfig());
	}
}

// ---------------------------------------------------------------- route execution

void ASimWorker::BeginWalkTo(const FVector& TargetCm)
{
	WalkTarget = TargetCm;
	Mode = EWorkerMode::Walking;
}

bool ASimWorker::StepWalk(float Dt, USimConfig* Config)
{
	const FVector Loc = GetActorLocation();
	const FVector ToTarget = FVector(WalkTarget.X - Loc.X, WalkTarget.Y - Loc.Y, 0.f);
	const float Dist = ToTarget.Size();
	if (Dist <= ArriveToleranceCm)
	{
		return true;
	}
	const float StepCm = FMath::Min(Config->WalkSpeed * 100.f * Dt, Dist);
	SetActorLocation(Loc + ToTarget / Dist * StepCm);
	return false;
}

void ASimWorker::NextSegment(USimConfig* Config)
{
	if (Segments.Num() == 0)
	{
		Mode = EWorkerMode::Idle;
		return;
	}
	SegIndex = (SegIndex + 1) % Segments.Num();
	const FRouteRow& Seg = Segments[SegIndex];
	switch (Seg.SegmentType)
	{
	case ERouteSegment::WalkTo:
		BeginWalkTo(Building->WaypointWorld(FName(*Seg.Param), TruthFloor));
		break;
	case ERouteSegment::WaitFor:
		Timer = FCString::Atof(*Seg.Param);
		Mode = EWorkerMode::Waiting;
		break;
	case ERouteSegment::Wander:
		Timer = FCString::Atof(*Seg.Param);
		SubTimer = 0.f;
		Mode = EWorkerMode::Wandering;
		break;
	case ERouteSegment::BoardHoist:
	{
		const int32 Floor = Seg.Param.Equals(TEXT("Target"), ESearchCase::IgnoreCase) ? TargetFloor : FCString::Atoi(*Seg.Param);
		Mode = EWorkerMode::WaitingHoist;
		if (Hoist)
		{
			Hoist->Request(this, Floor);
		}
		break;
	}
	case ERouteSegment::TakeStairs:
	{
		const int32 Delta = FCString::Atoi(*Seg.Param);
		const int32 Dest = FMath::Clamp(TruthFloor + Delta, 0, Config->NumFloors);
		StairsStartZ = Config->FloorZCm(TruthFloor) + CapsuleHalfHeightCm;
		StairsEndZ = Config->FloorZCm(Dest) + CapsuleHalfHeightCm;
		StairsProgress = 0.f;
		// Walk into the stairwell centre first; the Z ramp runs while there.
		const FVector Centre = Building->StairwellCentreCm();
		SetActorLocation(FVector(Centre.X, Centre.Y, GetActorLocation().Z));
		Mode = EWorkerMode::Stairs;
		break;
	}
	}
}

void ASimWorker::Step(float Dt, USimConfig* Config)
{
	if (!Building || !Config)
	{
		return;
	}

	switch (Mode)
	{
	case EWorkerMode::Idle:
		NextSegment(Config);
		break;

	case EWorkerMode::Walking:
		if (StepWalk(Dt, Config))
		{
			NextSegment(Config);
		}
		break;

	case EWorkerMode::Waiting:
		Timer -= Dt;
		if (Timer <= 0.f)
		{
			NextSegment(Config);
		}
		break;

	case EWorkerMode::Wandering:
		Timer -= Dt;
		if (Timer <= 0.f)
		{
			NextSegment(Config);
			break;
		}
		if (SubTimer > 0.f)
		{
			SubTimer -= Dt;
		}
		else if (StepWalk(Dt, Config))
		{
			// Arrived at the hop target: pause 2-8 s, then pick another point within 6 m on this floor.
			SubTimer = 2.f + Config->RandUnit() * 6.f;
			const float Angle = Config->RandUnit() * 2.f * PI;
			const float R = Config->RandUnit() * 600.f;
			FVector T = GetActorLocation() + FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.f);
			WalkTarget = Building->ClampToFloorPlate(T);
		}
		break;

	case EWorkerMode::WaitingHoist:
	case EWorkerMode::RidingHoist:
		// Position is driven by the hoist; nothing to do until OnHoistArrived.
		break;

	case EWorkerMode::Stairs:
	{
		const float Total = FMath::Abs(StairsEndZ - StairsStartZ) / 100.f / StairsSpeedMps;
		StairsProgress += Dt;
		const float A = Total > 0.f ? FMath::Clamp(StairsProgress / Total, 0.f, 1.f) : 1.f;
		FVector L = GetActorLocation();
		L.Z = FMath::Lerp(StairsStartZ, StairsEndZ, A);
		SetActorLocation(L);
		if (A >= 1.f)
		{
			NextSegment(Config);
		}
		break;
	}
	}

	if (!bOnHoist)
	{
		TruthFloor = Config->FloorFromZCm(GetActorLocation().Z - CapsuleHalfHeightCm);
	}
	if (PunchFlashRemaining > 0.f)
	{
		PunchFlashRemaining -= Dt;
	}
	Sensors->Step(Dt, Config);
	RefreshLabel();
}

// ---------------------------------------------------------------- hoist callbacks

void ASimWorker::OnBoardedHoist(ASimHoist* InHoist)
{
	Hoist = InHoist;
	bOnHoist = true;
	Mode = EWorkerMode::RidingHoist;
	FollowHoist(InHoist->GetActorLocation());
}

void ASimWorker::FollowHoist(const FVector& CarLocation)
{
	SetActorLocation(FVector(CarLocation.X, CarLocation.Y, CarLocation.Z + 10.f + CapsuleHalfHeightCm));
	if (Building)
	{
		TruthFloor = Building->GetConfig()->FloorFromZCm(CarLocation.Z + 10.f);
	}
}

void ASimWorker::OnHoistArrived(int32 Floor)
{
	bOnHoist = false;
	TruthFloor = Floor;
	// Step off: 1.5 m out of the car toward the floor door, then continue the route.
	if (Building)
	{
		const FVector Door = Building->WaypointWorld(TEXT("FloorHoistDoor"), Floor);
		SetActorLocation(Door);
		NextSegment(Building->GetConfig());
	}
}
