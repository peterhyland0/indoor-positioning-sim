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
