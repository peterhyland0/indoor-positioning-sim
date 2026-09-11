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
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"

// ---------------------------------------------------------------- controller

ASimBuilding* ASimPlayerController::Building() const
{
	return Cast<ASimBuilding>(UGameplayStatics::GetActorOfClass(this, ASimBuilding::StaticClass()));
}

void ASimPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;
	UseCutawayCamera();
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
}

void ASimPlayerController::UseCutawayCamera()
{
	TArray<AActor*> Cams;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("CutawayCamera"), Cams);
	if (Cams.Num() > 0)
	{
		SetViewTargetWithBlend(Cams[0], 0.5f);
	}
}

void ASimPlayerController::UseFreeCamera()
{
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
	Line(FString::Printf(TEXT("weather drift %+.2f hPa   hoist F%d %s"), C->WeatherDriftHpa, B->GetHoist() ? B->GetHoist()->GetCurrentFloor() : -1,
	     B->GetHoist() ? (B->GetHoist()->GetState() == EHoistState::Moving ? TEXT("moving") : TEXT("stopped")) : TEXT("")));
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
	Line(TEXT("Space pause  R reset  T debug traces  Tab next worker  1 cutaway  2 free cam"), FColor(160, 160, 160));
}

// ---------------------------------------------------------------- game mode

ASimGameMode::ASimGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerControllerClass = ASimPlayerController::StaticClass();
	HUDClass = ASimHUD::StaticClass();
}
