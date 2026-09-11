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
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category = "Sim") void UseCutawayCamera();
	UFUNCTION(BlueprintCallable, Category = "Sim") void UseFreeCamera();
	/** Chase camera on the selected worker (Tab cycles). */
	UFUNCTION(BlueprintCallable, Category = "Sim") void UseFollowCamera();

private:
	UPROPERTY() TObjectPtr<class ACameraActor> FollowCam;
	bool bFollowSelected = false;
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
