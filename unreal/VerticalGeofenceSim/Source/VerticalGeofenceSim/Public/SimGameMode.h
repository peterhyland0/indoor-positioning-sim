// Spectator game mode + a text HUD + key bindings for the demo controls.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "SimGameMode.generated.h"

class ASimBuilding;

UCLASS()
class VERTICALGEOFENCESIM_API ASimPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category = "Sim") void UseCutawayCamera();
	UFUNCTION(BlueprintCallable, Category = "Sim") void UseFreeCamera();

private:
	ASimBuilding* Building() const;
	void OnTogglePause();
	void OnReset();
	void OnToggleDebug();
	void OnNextWorker();
};

UCLASS()
class VERTICALGEOFENCESIM_API ASimHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};

UCLASS()
class VERTICALGEOFENCESIM_API ASimGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASimGameMode();
};
