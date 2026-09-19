#include "SimHoist.h"
#include "SimWorker.h"
#include "SimConfig.h"
#include "SimSubsystem.h"
#include "SimBridge.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ASimHoist::ASimHoist()
{
	PrimaryActorTick.bCanEverTick = false;
	Car = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Car"));
	SetRootComponent(Car);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Sim/Materials/M_ShaftCage"));
	if (CubeMesh.Succeeded()) Car->SetStaticMesh(CubeMesh.Object);
	if (Mat.Succeeded())      Car->SetMaterial(0, Mat.Object);
	Car->SetRelativeScale3D(FVector(2.8f, 2.8f, 0.2f));
	Car->SetCollisionProfileName(TEXT("RadioTransparent"));
	Tags.Add(TEXT("Hoist"));

	// Cage: four corner posts, a roof plate, three mesh side panels (south left open for the camera).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Steel(TEXT("/Game/Sim/Materials/M_Steel"));
	auto AddPart = [&](const TCHAR* Name, FVector RelLoc, FVector Scale)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		P->SetupAttachment(Car);
		if (CubeMesh.Succeeded()) P->SetStaticMesh(CubeMesh.Object);
		if (Steel.Succeeded()) P->SetMaterial(0, Steel.Object);
		// Car is scaled (2.8, 2.8, 0.2); children inherit, so counter-scale.
		P->SetRelativeScale3D(FVector(Scale.X / 2.8f, Scale.Y / 2.8f, Scale.Z / 0.2f));
		P->SetRelativeLocation(FVector(RelLoc.X / 2.8f, RelLoc.Y / 2.8f, RelLoc.Z / 0.2f));
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
		CageParts.Add(P);
	};
	const float H = 250.f;
	for (int32 i = 0; i < 4; ++i)
	{
		const float sx = (i & 1) ? 1.f : -1.f, sy = (i & 2) ? 1.f : -1.f;
		AddPart(*FString::Printf(TEXT("Post%d"), i), FVector(sx * 130.f, sy * 130.f, H / 2.f + 10.f), FVector(8.f, 8.f, H));
	}
	AddPart(TEXT("Roof"), FVector(0.f, 0.f, H + 14.f), FVector(280.f, 280.f, 6.f));
	AddPart(TEXT("PanelN"), FVector(0.f, 138.f, H / 2.f + 10.f), FVector(280.f, 3.f, H));
	AddPart(TEXT("PanelW"), FVector(-138.f, 0.f, H / 2.f + 10.f), FVector(3.f, 280.f, H));
	AddPart(TEXT("PanelE"), FVector(138.f, 0.f, H / 2.f + 10.f), FVector(3.f, 280.f, H));
}

void ASimHoist::Request(ASimWorker* Worker, int32 Floor)
{
	if (!Worker)
	{
		return;
	}
	Waiting.Add(Worker, Floor);
	// The car must first come to the worker's floor, then go to the destination.
	const int32 From = Worker->TruthFloor;
	if (From != CurrentFloor || State == EHoistState::Moving)
	{
		Queue.AddUnique(From);
	}
	Queue.AddUnique(Floor);
}

void ASimHoist::BoardWaitingWorkers()
{
	for (auto It = Waiting.CreateIterator(); It; ++It)
	{
		ASimWorker* W = It.Key().Get();
		if (!W)
		{
			It.RemoveCurrent();
			continue;
		}
		if (W->bOnHoist || W->TruthFloor != CurrentFloor)
		{
			continue;
		}
		const float DistCm = FVector::Dist2D(W->GetActorLocation(), GetActorLocation());
		if (DistCm > 400.f)
		{
			continue; // not at the door yet
		}
		W->OnBoardedHoist(this);
		Riders.AddUnique(W);
		Queue.AddUnique(It.Value());
	}
}

void ASimHoist::Step(float Dt, USimConfig* Config)
{
	switch (State)
	{
	case EHoistState::Idle:
		BoardWaitingWorkers();
		if (Queue.Num() > 0)
		{
			TargetFloor = Queue[0];
			if (TargetFloor == CurrentFloor)
			{
				Queue.RemoveAt(0);
				State = EHoistState::Dwell;
				DwellRemaining = Config->HoistDwellSec * 0.25f;
			}
			else
			{
				State = EHoistState::Moving;
				if (USimSubsystem* S = USimSubsystem::Get(this))
				{
					S->GetBridge()->SendEvent(TEXT("hoistDepart"), FString::Printf(TEXT("{\"from\":%d,\"to\":%d,\"riders\":%d}"), CurrentFloor, TargetFloor, Riders.Num()));
				}
			}
		}
		break;

	case EHoistState::Moving:
	{
		const float TargetZ = Config->FloorZCm(TargetFloor);
		const float StepCm = Config->HoistSpeed * 100.f * Dt;
		ZCm = FMath::FInterpConstantTo(ZCm, TargetZ, 1.f, StepCm);
		SetActorLocation(FVector(CarXY.X * 100.f, CarXY.Y * 100.f, ZCm - 10.f));
		for (const TWeakObjectPtr<ASimWorker>& R : Riders)
		{
			if (ASimWorker* W = R.Get())
			{
				W->FollowHoist(GetActorLocation());
			}
		}
		if (FMath::IsNearlyEqual(ZCm, TargetZ))
		{
			CurrentFloor = TargetFloor;
			Queue.Remove(CurrentFloor);
			State = EHoistState::Dwell;
			DwellRemaining = Config->HoistDwellSec;
			if (USimSubsystem* S = USimSubsystem::Get(this))
			{
				S->GetBridge()->SendEvent(TEXT("hoistArrive"), FString::Printf(TEXT("{\"floor\":%d,\"riders\":%d}"), CurrentFloor, Riders.Num()));
			}
		}
		break;
	}

	case EHoistState::Dwell:
		// Let riders off whose destination is this floor.
		for (int32 i = Riders.Num() - 1; i >= 0; --i)
		{
			ASimWorker* W = Riders[i].Get();
			if (!W)
			{
				Riders.RemoveAt(i);
				continue;
			}
			const int32* Dest = Waiting.Find(W);
			if (Dest && *Dest == CurrentFloor)
			{
				W->OnHoistArrived(CurrentFloor);
				Waiting.Remove(W);
				Riders.RemoveAt(i);
			}
		}
		BoardWaitingWorkers();
		DwellRemaining -= Dt;
		if (DwellRemaining <= 0.f)
		{
			State = EHoistState::Idle;
		}
		break;
	}
}
