#include "SimBuilding.h"
#include "SimConfig.h"
#include "SimSubsystem.h"
#include "SimBridge.h"
#include "SimBeacon.h"
#include "SimWorker.h"
#include "SimHoist.h"
#include "PhoneSensorsComponent.h"
#include "VerticalGeofenceSim.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/Parse.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float CapsuleHalfHeightCm = 90.f;
}

ASimBuilding::ASimBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UDataTable> DTBeacons(TEXT("/Game/Data/DT_BeaconLayout"));
	static ConstructorHelpers::FObjectFinder<UDataTable> DTShifts(TEXT("/Game/Data/DT_Shifts"));
	static ConstructorHelpers::FObjectFinder<UDataTable> DTRoutes(TEXT("/Game/Data/DT_Routes"));
	static ConstructorHelpers::FObjectFinder<UDataTable> DTWaypoints(TEXT("/Game/Data/DT_Waypoints"));
	BeaconLayout = DTBeacons.Object;
	Shifts = DTShifts.Object;
	Routes = DTRoutes.Object;
	Waypoints = DTWaypoints.Object;
	Tags.Add(TEXT("SimBuilding"));
}

void ASimBuilding::BeginPlay()
{
	Super::BeginPlay();
	USimSubsystem* S = USimSubsystem::Get(this);
	if (!S)
	{
		UE_LOG(LogSim, Error, TEXT("SimSubsystem missing"));
		return;
	}
	S->ResetConfig();
	Config = S->GetConfig();

	// Headless / scripted runs: -SimSeed=N reseeds, -SimRunSeconds=N quits after that much sim time.
	int32 SeedOverride = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimSeed="), SeedOverride))
	{
		Config->Seed = SeedOverride;
		Config->ResetRun();
	}
	FParse::Value(FCommandLine::Get(), TEXT("SimRunSeconds="), RunSecondsLimit);
	FParse::Value(FCommandLine::Get(), TEXT("SimShotAt="), ShotAt);
	FString ScenarioPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimScenario="), ScenarioPath))
	{
		LoadScenario(ScenarioPath);
	}

	SpawnFixtures();
	SpawnBeacons();
	SpawnWorkers();

	S->GetBridge()->Start(Config, this);
	S->GetBridge()->SendSession();

	GetWorldTimerManager().SetTimer(StepTimer, this, &ASimBuilding::FixedStep, Config->StepSeconds, true);
	UE_LOG(LogSim, Log, TEXT("sim started: seed %d, %d beacons, %d workers, step %.3fs"),
	       Config->Seed, Beacons.Num(), Workers.Num(), Config->StepSeconds);
}

void ASimBuilding::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(StepTimer);
	if (USimSubsystem* S = USimSubsystem::Get(this))
	{
		S->GetBridge()->Shutdown();
	}
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------- spawning

void ASimBuilding::SpawnFixtures()
{
	UWorld* World = GetWorld();
	const FVector4& Sh = Config->HoistShaft;
	Hoist = World->SpawnActor<ASimHoist>(FVector((Sh.X + Sh.Z / 2.f) * 100.f, (Sh.Y + Sh.W / 2.f) * 100.f, -10.f), FRotator::ZeroRotator);
	Hoist->CarXY = FVector2D(Sh.X + Sh.Z / 2.f, Sh.Y + Sh.W / 2.f);
#if WITH_EDITOR
	Hoist->SetActorLabel(TEXT("Hoist"));
#endif
	Reference = World->SpawnActor<ASimReferenceStation>(FVector(100.f, 1900.f, 120.f), FRotator::ZeroRotator);
#if WITH_EDITOR
	Reference->SetActorLabel(TEXT("ReferenceStation"));
#endif
	// Editor-only markers placed by bootstrap.py are replaced by the live actors.
	TArray<AActor*> Markers;
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("BeaconMarker"), Markers);
	for (AActor* M : Markers)
	{
		M->Destroy();
	}
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("ReferenceStation"), Markers);
	for (AActor* M : Markers)
	{
		if (M != Reference)
		{
			M->Destroy();
		}
	}
}

void ASimBuilding::SpawnBeacons()
{
	if (!BeaconLayout)
	{
		return;
	}
	TArray<FBeaconRow*> Rows;
	BeaconLayout->GetAllRows<FBeaconRow>(TEXT("SpawnBeacons"), Rows);
	for (const FBeaconRow* Row : Rows)
	{
		if (Row->Floor > Config->NumFloors)
		{
			continue;
		}
		// Mounted on the ceiling: 0.3 m under the slab above.
		const float Z = (Row->Floor + 1) * Config->FloorHeight - Config->SlabThickness - 0.3f;
		ASimBeacon* B = GetWorld()->SpawnActor<ASimBeacon>(FVector(Row->X * 100.f, Row->Y * 100.f, Z * 100.f), FRotator::ZeroRotator);
		B->InitFromRow(*Row);
		Beacons.Add(B);
		BeaconsById.Add(B->BeaconId, B);
	}
}

void ASimBuilding::SpawnWorkers()
{
	if (!Shifts)
	{
		return;
	}
	TArray<FShiftRow*> Rows;
	Shifts->GetAllRows<FShiftRow>(TEXT("SpawnWorkers"), Rows);
	const FVector Entrance = WaypointWorld(TEXT("LobbyEntrance"), 0);
	int32 i = 0;
	for (const FShiftRow* Row : Rows)
	{
		const FVector Jitter(FMath::Cos(i * 1.3f) * 80.f, FMath::Sin(i * 1.3f) * 80.f, 0.f);
		ASimWorker* W = GetWorld()->SpawnActor<ASimWorker>(Entrance + Jitter, FRotator::ZeroRotator);
		W->InitFromShift(*Row, this, Hoist);
		W->GetSensors()->ResetCadence(Config);
		Workers.Add(W);
		WorkersById.Add(W->WorkerId, W);
		i++;
	}
	SelectedWorker = Workers.Num() > 0 ? Workers[0] : nullptr;
}

// ---------------------------------------------------------------- stepping

void ASimBuilding::FixedStep()
{
	if (!Config || Config->bPaused)
	{
		return;
	}
	const float Dt = Config->StepSeconds;
	Config->SimTime += Dt;
	Config->StepWeather(Dt);
	while (NextScenarioIndex < Scenario.Num() && Scenario[NextScenarioIndex].T <= Config->SimTime)
	{
		RunCommand(Scenario[NextScenarioIndex++].Cmd);
	}
	if (Hoist)
	{
		Hoist->Step(Dt, Config);
	}
	for (ASimWorker* W : Workers)
	{
		W->Step(Dt, Config);
	}
	StepCount++;

	if (ShotAt > 0.f && Config->SimTime >= ShotAt)
	{
		ShotAt = 0.f;
		FScreenshotRequest::RequestScreenshot(TEXT("sim_shot"), true, false);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			const AActor* VT = PC->GetViewTarget();
			const FVector CamLoc = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
			const FRotator CamRot = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;
			UE_LOG(LogSim, Log, TEXT("screenshot at t=%.1f: view target %s (%s), camera at %s rot %s, fov %.1f"),
			       Config->SimTime, VT ? *VT->GetName() : TEXT("none"), VT ? *VT->GetClass()->GetName() : TEXT(""),
			       *CamLoc.ToString(), *CamRot.ToString(), PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 0.f);
		}
	}

	if (RunSecondsLimit > 0.f && Config->SimTime >= RunSecondsLimit)
	{
		UE_LOG(LogSim, Log, TEXT("SimRunSeconds reached (%.1f s, %d steps); quitting"), Config->SimTime, StepCount);
		GetWorldTimerManager().ClearTimer(StepTimer);
		if (USimSubsystem* S = USimSubsystem::Get(this))
		{
			S->GetBridge()->Shutdown();
		}
		FGenericPlatformMisc::RequestExit(false);
	}
}

float ASimBuilding::GetSimTime() const
{
	return Config ? Config->SimTime : 0.f;
}

// ---------------------------------------------------------------- queries

ASimBeacon* ASimBuilding::FindBeacon(FName Id) const
{
	ASimBeacon* const* B = BeaconsById.Find(Id);
	return B ? *B : nullptr;
}

ASimWorker* ASimBuilding::FindWorker(FName Id) const
{
	ASimWorker* const* W = WorkersById.Find(Id);
	return W ? *W : nullptr;
}

FVector ASimBuilding::WaypointWorld(FName Name, int32 Floor) const
{
	float X = 15.f, Y = 10.f;
	if (Waypoints)
	{
		if (const FWaypointRow* Row = Waypoints->FindRow<FWaypointRow>(Name, TEXT("WaypointWorld"), false))
		{
			X = Row->X;
			Y = Row->Y;
		}
		else
		{
			UE_LOG(LogSim, Warning, TEXT("unknown waypoint %s"), *Name.ToString());
		}
	}
	return FVector(X * 100.f, Y * 100.f, Config->FloorZCm(Floor) + CapsuleHalfHeightCm);
}

FVector ASimBuilding::StairwellCentreCm() const
{
	const FVector4& St = Config->Stairwell;
	return FVector((St.X + St.Z / 2.f) * 100.f, (St.Y + St.W / 2.f) * 100.f, 0.f);
}

FVector ASimBuilding::ClampToFloorPlate(const FVector& Cm) const
{
	// Slab is 30 x 20 m (see tower_geometry.py); keep 1 m margin and avoid the two holes.
	FVector Out = Cm;
	Out.X = FMath::Clamp(Out.X, 100.f, 2900.f);
	Out.Y = FMath::Clamp(Out.Y, 100.f, 1900.f);
	auto Avoid = [&Out](const FVector4& R)
	{
		const float X0 = R.X * 100.f - 50.f, X1 = (R.X + R.Z) * 100.f + 50.f;
		const float Y0 = R.Y * 100.f - 50.f, Y1 = (R.Y + R.W) * 100.f + 50.f;
		if (Out.X > X0 && Out.X < X1 && Out.Y > Y0 && Out.Y < Y1)
		{
			// push out along the shortest axis
			const float DX = FMath::Min(Out.X - X0, X1 - Out.X);
			const float DY = FMath::Min(Out.Y - Y0, Y1 - Out.Y);
			if (DX < DY) Out.X = (Out.X - X0 < X1 - Out.X) ? X0 : X1;
			else         Out.Y = (Out.Y - Y0 < Y1 - Out.Y) ? Y0 : Y1;
		}
	};
	Avoid(Config->HoistShaft);
	Avoid(Config->Stairwell);
	return Out;
}

TArray<FRouteRow> ASimBuilding::GetRoute(FName RouteName) const
{
	TArray<FRouteRow> Out;
	if (!Routes)
	{
		return Out;
	}
	TArray<FRouteRow*> Rows;
	Routes->GetAllRows<FRouteRow>(TEXT("GetRoute"), Rows);
	for (const FRouteRow* R : Rows)
	{
		if (R->RouteName == RouteName)
		{
			Out.Add(*R);
		}
	}
	Out.Sort([](const FRouteRow& A, const FRouteRow& B) { return A.Order < B.Order; });
	return Out;
}

float ASimBuilding::ReadReferencePressure() const
{
	return Reference ? Reference->ReadPressure() : Config->SitePressure;
}

float ASimBuilding::GustOffsetFor(int32 Floor) const
{
	return (Floor == GustFloor && Config->SimTime < GustUntil) ? Config->GustHpa : 0.f;
}

// ---------------------------------------------------------------- controls

void ASimBuilding::TogglePause()
{
	if (!Config)
	{
		return;
	}
	Config->bPaused = !Config->bPaused;
	if (USimSubsystem* S = USimSubsystem::Get(this))
	{
		S->GetBridge()->SendEvent(Config->bPaused ? TEXT("paused") : TEXT("resumed"));
	}
}

void ASimBuilding::ToggleDebugTraces()
{
	if (Config)
	{
		Config->bDebugTraces = !Config->bDebugTraces;
	}
}

void ASimBuilding::SelectNextWorker()
{
	if (Workers.Num() == 0)
	{
		return;
	}
	const int32 Idx = Workers.IndexOfByKey(SelectedWorker);
	SelectedWorker = Workers[(Idx + 1) % Workers.Num()];
}

void ASimBuilding::DoorSlam(int32 Floor)
{
	GustFloor = Floor;
	GustUntil = Config->SimTime + Config->GustSec;
	if (USimSubsystem* S = USimSubsystem::Get(this))
	{
		S->GetBridge()->SendEvent(TEXT("gust"), FString::Printf(TEXT("{\"floor\":%d}"), Floor));
	}
}

void ASimBuilding::SetStorm(bool bOn)
{
	Config->bWeatherDrift = bOn;
	if (USimSubsystem* S = USimSubsystem::Get(this))
	{
		S->GetBridge()->SendEvent(bOn ? TEXT("stormStart") : TEXT("stormStop"));
	}
}

// ---------------------------------------------------------------- commands

bool ASimBuilding::RunCommand(const FString& Command)
{
	TArray<FString> Args;
	Command.TrimStartAndEnd().ParseIntoArrayWS(Args);
	if (Args.Num() == 0 || !Config)
	{
		return false;
	}
	const FString Verb = Args[0].ToLower();
	auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	auto OnOff = [&Arg](int32 i) { const FString V = Arg(i).ToLower(); return V != TEXT("off") && V != TEXT("0") && V != TEXT("false"); };
	auto BeaconArg = [this, &Arg](int32 i) -> ASimBeacon* {
		ASimBeacon* B = FindBeacon(FName(*Arg(i)));
		if (!B) UE_LOG(LogSim, Warning, TEXT("no beacon '%s'"), *Arg(i));
		return B;
	};
	auto WorkerArg = [this, &Arg](int32 i) -> ASimWorker* {
		ASimWorker* W = FindWorker(FName(*Arg(i)));
		if (!W) UE_LOG(LogSim, Warning, TEXT("no worker '%s'"), *Arg(i));
		return W;
	};
	USimBridge* Br = USimSubsystem::Get(this) ? USimSubsystem::Get(this)->GetBridge() : nullptr;

	UE_LOG(LogSim, Log, TEXT("cmd @%.1f: %s"), Config->SimTime, *Command);
	bool bOk = true;
	if (Verb == TEXT("killbeacon"))        { if (ASimBeacon* B = BeaconArg(1)) B->Kill(); else bOk = false; }
	else if (Verb == TEXT("revivebeacon")) { if (ASimBeacon* B = BeaconArg(1)) B->Revive(); else bOk = false; }
	else if (Verb == TEXT("battery"))      { if (ASimBeacon* B = BeaconArg(1)) B->SetBattery(FCString::Atof(*Arg(2))); else bOk = false; }
	else if (Verb == TEXT("nudge"))        { if (ASimBeacon* B = BeaconArg(1)) B->Nudge(Args.IsValidIndex(2) ? FCString::Atof(*Arg(2)) : 5.f); else bOk = false; }
	else if (Verb == TEXT("storm"))        { SetStorm(OnOff(1)); }
	else if (Verb == TEXT("slam"))         { DoorSlam(FCString::Atoi(*Arg(1))); }
	else if (Verb == TEXT("pause"))        { if (!Config->bPaused) TogglePause(); }
	else if (Verb == TEXT("resume"))       { if (Config->bPaused) TogglePause(); }
	else if (Verb == TEXT("debug"))        { Config->bDebugTraces = OnOff(1); }
	else if (Verb == TEXT("pocket"))       { if (ASimWorker* W = WorkerArg(1)) { W->SetPhoneState(OnOff(2) ? EPhoneState::InPocket : EPhoneState::InHand); if (Br) Br->SendEvent(TEXT("workerPhoneState"), FString::Printf(TEXT("{\"worker\":\"%s\",\"phoneState\":\"%s\"}"), *W->WorkerId.ToString(), OnOff(2) ? TEXT("inPocket") : TEXT("inHand"))); } else bOk = false; }
	else if (Verb == TEXT("background"))   { if (ASimWorker* W = WorkerArg(1)) { W->SetAppState(OnOff(2) ? EAppState::Background : EAppState::Foreground); if (Br) Br->SendEvent(TEXT("workerAppState"), FString::Printf(TEXT("{\"worker\":\"%s\",\"appState\":\"%s\"}"), *W->WorkerId.ToString(), OnOff(2) ? TEXT("bg") : TEXT("fg"))); } else bOk = false; }
	else if (Verb == TEXT("select"))       { if (ASimWorker* W = WorkerArg(1)) SelectedWorker = W; else bOk = false; }
	else if (Verb == TEXT("list"))
	{
		for (const ASimBeacon* B : Beacons) UE_LOG(LogSim, Log, TEXT("  beacon %s F%d %s battery %.2f"), *B->BeaconId.ToString(), B->Floor, B->bAlive ? TEXT("alive") : TEXT("dead"), B->Battery);
		for (const ASimWorker* W : Workers) UE_LOG(LogSim, Log, TEXT("  worker %s F%d %s"), *W->WorkerId.ToString(), W->TruthFloor, *W->RouteName.ToString());
	}
	else
	{
		UE_LOG(LogSim, Warning, TEXT("unknown command '%s'"), *Verb);
		bOk = false;
	}
	return bOk;
}

bool ASimBuilding::LoadScenario(const FString& JsonPath)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath))
	{
		UE_LOG(LogSim, Error, TEXT("scenario file not found: %s"), *JsonPath);
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> Items;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Items))
	{
		UE_LOG(LogSim, Error, TEXT("scenario file is not a JSON array: %s"), *JsonPath);
		return false;
	}
	Scenario.Reset();
	for (const TSharedPtr<FJsonValue>& V : Items)
	{
		const TSharedPtr<FJsonObject>* Obj;
		if (V->TryGetObject(Obj))
		{
			Scenario.Add({static_cast<float>((*Obj)->GetNumberField(TEXT("t"))), (*Obj)->GetStringField(TEXT("cmd"))});
		}
	}
	Scenario.Sort([](const FScheduledCommand& A, const FScheduledCommand& B) { return A.T < B.T; });
	NextScenarioIndex = 0;
	UE_LOG(LogSim, Log, TEXT("scenario loaded: %d commands from %s"), Scenario.Num(), *JsonPath);
	return true;
}

// Console: `Sim killbeacon F12-A`, `Sim storm on`, ... (PIE console or -ExecCmds).
static FAutoConsoleCommandWithWorldAndArgs GSimConsoleCommand(
	TEXT("Sim"),
	TEXT("Run a simulation command. See ASimBuilding::RunCommand for the list."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ASimBuilding* B = Cast<ASimBuilding>(UGameplayStatics::GetActorOfClass(World, ASimBuilding::StaticClass()));
		if (B)
		{
			B->RunCommand(FString::Join(Args, TEXT(" ")));
		}
	}));
