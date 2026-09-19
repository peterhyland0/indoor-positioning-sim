#include "SimGameMode.h"
#include "SimBuilding.h"
#include "SimConfig.h"
#include "SimSubsystem.h"
#include "SimBridge.h"
#include "SimWorker.h"
#include "SimHoist.h"
#include "PhoneSensorsComponent.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "Misc/CommandLine.h"
#include "SimPanel.h"
#include "Widgets/SOverlay.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "HAL/PlatformTime.h"

// ---------------------------------------------------------------- controller

ASimBuilding* ASimPlayerController::Building() const
{
	return Cast<ASimBuilding>(UGameplayStatics::GetActorOfClass(this, ASimBuilding::StaticClass()));
}

void ASimPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;
	// Clicks go to the panel, keys keep going to the game.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	if (GEngine && GEngine->GameViewport)
	{
		// The building spawns its actors in its own BeginPlay; build the panel on the next tick so the lists are full.
		GetWorldTimerManager().SetTimerForNextTick([this]()
		{
			if (!GEngine || !GEngine->GameViewport) return;
			Panel = SNew(SSimPanel).Building(Building());
			PanelHost = SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16.f) [ Panel.ToSharedRef() ];
			GEngine->GameViewport->AddViewportWidgetContent(PanelHost.ToSharedRef(), 10);
		});
	}
	FString Clip;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimClip="), Clip))
	{
		TArray<FString> Parts;
		Clip.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 2)
		{
			ClipStart = FCString::Atof(*Parts[0]);
			ClipEnd = FCString::Atof(*Parts[1]);
			if (Parts.Num() >= 3) ClipFps = FMath::Clamp(FCString::Atof(*Parts[2]), 1.f, 60.f);
		}
	}
	FString Cam;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCamera="), Cam) && Cam.Equals(TEXT("follow"), ESearchCase::IgnoreCase))
	{
		UseFollowCamera();
	}
	else
	{
		UseCutawayCamera();
	}
}

void ASimPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}
	InputComponent->BindAction(TEXT("Sim_TogglePause"), IE_Pressed, this, &ASimPlayerController::OnTogglePause);
	InputComponent->BindAction(TEXT("Sim_Reset"), IE_Pressed, this, &ASimPlayerController::OnReset);
	InputComponent->BindAction(TEXT("Sim_ToggleDebugTraces"), IE_Pressed, this, &ASimPlayerController::OnToggleDebug);
	InputComponent->BindAction(TEXT("Sim_NextWorker"), IE_Pressed, this, &ASimPlayerController::OnNextWorker);
	InputComponent->BindAction(TEXT("Sim_CutawayCamera"), IE_Pressed, this, &ASimPlayerController::UseCutawayCamera);
	InputComponent->BindAction(TEXT("Sim_FreeCamera"), IE_Pressed, this, &ASimPlayerController::UseFreeCamera);
	InputComponent->BindAction(TEXT("Sim_FollowCamera"), IE_Pressed, this, &ASimPlayerController::UseFollowCamera);
	InputComponent->BindAction(TEXT("Sim_TogglePanel"), IE_Pressed, this, &ASimPlayerController::TogglePanel);
}

void ASimPlayerController::TogglePanel()
{
	if (PanelHost.IsValid())
	{
		SetPanelVisible(PanelHost->GetVisibility() != EVisibility::Visible);
	}
}

void ASimPlayerController::SetPanelVisible(bool bVisible)
{
	if (PanelHost.IsValid())
	{
		PanelHost->SetVisibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

void ASimPlayerController::UseFollowCamera()
{
	bFollowSelected = true;
	if (!FollowCam)
	{
		FollowCam = GetWorld()->SpawnActor<ACameraActor>();
		FollowCam->GetCameraComponent()->SetFieldOfView(55.f);
	}
	Tick(0.f); // place it before the blend starts
	SetViewTargetWithBlend(FollowCam, 0.4f);
}

void ASimPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ASimBuilding* B = Building();

	// Frame dump for clips: real-time paced so the video plays at the sim's apparent speed.
	if (ClipEnd > 0.f && B)
	{
		const float T = B->GetSimTime();
		const double Now = FPlatformTime::Seconds();
		if (T >= ClipStart && T <= ClipEnd && Now >= NextClipShotAt)
		{
			NextClipShotAt = Now + 1.0 / ClipFps;
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("clip/frame_%05d"), ClipFrame++), true, false);
		}
	}

	if (!bFollowSelected || !FollowCam)
	{
		return;
	}
	ASimWorker* W = B ? B->GetSelectedWorker() : nullptr;
	if (!W)
	{
		return;
	}
	// Sit 14 m south of the worker (the open face), 3 m above, looking at their head.
	const FVector Head = W->GetActorLocation() + FVector(0.f, 0.f, 80.f);
	const FVector Want = Head + FVector(0.f, -1400.f, 300.f);
	const FVector Loc = FMath::VInterpTo(FollowCam->GetActorLocation(), Want, DeltaSeconds, 4.f);
	FollowCam->SetActorLocation(Loc);
	FollowCam->SetActorRotation(FRotationMatrix::MakeFromX(Head - Loc).Rotator());
}

void ASimPlayerController::UseCutawayCamera()
{
	bFollowSelected = false;
	TArray<AActor*> Cams;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("CutawayCamera"), Cams);
	if (Cams.Num() > 0)
	{
		// Place and aim the camera from the config so the whole tower fits with the lobby/hoist area
		// (where most of the action is) closest to the lens. Ignores how the actor was placed.
		if (ASimBuilding* B = Building())
		{
			const USimConfig* C = B->GetConfig();
			const float H = C ? C->FloorZCm(C->NumFloors + 1) : 6000.f;
			const FVector Loc(1500.f, -1.62f * H, 0.42f * H);
			const FVector Target(1500.f, 1000.f, 0.5f * H);
			Cams[0]->SetActorLocation(Loc);
			Cams[0]->SetActorRotation(FRotationMatrix::MakeFromX(Target - Loc).Rotator());
			if (ACameraActor* CamActor = Cast<ACameraActor>(Cams[0]))
			{
				CamActor->GetCameraComponent()->SetFieldOfView(62.f);
			}
		}
		SetViewTargetWithBlend(Cams[0], 0.5f);
	}
}

void ASimPlayerController::UseFreeCamera()
{
	bFollowSelected = false;
	if (APawn* P = GetPawn())
	{
		SetViewTargetWithBlend(P, 0.5f);
	}
}

void ASimPlayerController::OnTogglePause()  { if (ASimBuilding* B = Building()) B->TogglePause(); }
void ASimPlayerController::OnToggleDebug()  { if (ASimBuilding* B = Building()) B->ToggleDebugTraces(); }
void ASimPlayerController::OnNextWorker()   { if (ASimBuilding* B = Building()) B->SelectNextWorker(); }
void ASimPlayerController::OnReset()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

// ---------------------------------------------------------------- HUD

void ASimHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	ASimBuilding* B = Cast<ASimBuilding>(UGameplayStatics::GetActorOfClass(this, ASimBuilding::StaticClass()));
	USimSubsystem* S = USimSubsystem::Get(this);
	if (!B || !S)
	{
		return;
	}
	USimConfig* C = B->GetConfig();
	USimBridge* Br = S->GetBridge();
	UFont* Font = GEngine->GetMediumFont();
	float Y = 20.f;
	auto Line = [&](const FString& Text, FColor Colour = FColor::White)
	{
		DrawText(Text, Colour, 20.f, Y, Font, 1.1f);
		Y += 20.f;
	};
	Line(FString::Printf(TEXT("t = %.1f s   seed %d   %s"), C->SimTime, C->Seed, C->bPaused ? TEXT("PAUSED") : TEXT("running")), C->bPaused ? FColor::Yellow : FColor::White);
	Line(FString::Printf(TEXT("bridge: %s   %d msgs   %s"), Br->IsConnected() ? TEXT("connected") : TEXT("no server (recording only)"), Br->GetMessageCount(), *FPaths::GetCleanFilename(Br->GetLogPath())),
	     Br->IsConnected() ? FColor::Green : FColor::Orange);
	const ASimHoist* Hoist = B->GetHoist();
	const float HoistZ = Hoist ? Hoist->GetActorLocation().Z + 10.f : 0.f;
	Line(FString::Printf(TEXT("weather drift %+.2f hPa   hoist at %.1f m (F%d) %s"), C->WeatherDriftHpa, HoistZ / 100.f, C->FloorFromZCm(HoistZ),
	     Hoist ? (Hoist->GetState() == EHoistState::Moving ? TEXT("moving") : TEXT("stopped")) : TEXT("")));
	if (ASimWorker* W = B->GetSelectedWorker())
	{
		Line(FString::Printf(TEXT("selected %s  truth F%d%s  est %s  %s/%s/%s"), *W->WorkerId.ToString(), W->TruthFloor,
		     W->bOnHoist ? TEXT(" (hoist)") : TEXT(""), W->bHasEstimate ? *FString::Printf(TEXT("F%d"), W->EstFloor) : TEXT("-"),
		     W->Platform == EPlatform::iOS ? TEXT("iOS") : TEXT("Android"), W->AppState == EAppState::Foreground ? TEXT("fg") : TEXT("bg"),
		     W->PhoneState == EPhoneState::InHand ? TEXT("hand") : TEXT("pocket")), FColor::Cyan);
		FString Scans;
		for (const FScan& Sc : W->GetSensors()->LastScans)
		{
			Scans += FString::Printf(TEXT("%s:%d  "), *Sc.B.ToString(), Sc.Rssi);
		}
		Line(FString::Printf(TEXT("  last scan: %s"), Scans.IsEmpty() ? TEXT("(nothing heard)") : *Scans));
	}
	// Key hints along the bottom edge so they never sit over the tower.
	DrawText(TEXT("Space pause   R reset   T traces   Tab next worker   1 cutaway   2 free cam   3 follow   P panel"), FColor(200, 200, 200), 20.f, Canvas->SizeY - 28.f, Font, 1.0f);
}

// ---------------------------------------------------------------- game mode

ASimGameMode::ASimGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerControllerClass = ASimPlayerController::StaticClass();
	HUDClass = ASimHUD::StaticClass();
}
