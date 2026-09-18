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
	UFUNCTION(BlueprintCallable, Category = "Sim") void TogglePanel();
	UFUNCTION(BlueprintCallable, Category = "Sim") void SetPanelVisible(bool bVisible);

private:
	/** -SimClip=start,end,fps: dump numbered screenshots between two sim times (for making videos). */
	float ClipStart = -1.f, ClipEnd = -1.f, ClipFps = 15.f;
	double NextClipShotAt = 0.0;
	int32 ClipFrame = 0;
	UPROPERTY() TObjectPtr<class ACameraActor> FollowCam;
	TSharedPtr<class SSimPanel> Panel;
	TSharedPtr<class SWidget> PanelHost;
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
