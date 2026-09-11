// A worker carrying a phone. Executes a data-driven route in fixed steps; the phone is a component.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimTypes.h"
#include "SimWorker.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPhoneSensorsComponent;
class ASimBuilding;
class ASimHoist;
class USimConfig;

UENUM()
enum class EWorkerMode : uint8 { Idle, Walking, Waiting, Wandering, WaitingHoist, RidingHoist, Stairs };

UCLASS()
class VERTICALGEOFENCESIM_API ASimWorker : public AActor
{
	GENERATED_BODY()

public:
	ASimWorker();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Worker") FName WorkerId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Worker") EPlatform Platform = EPlatform::Android;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Worker") EPhoneState PhoneState = EPhoneState::InHand;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Worker") EAppState AppState = EAppState::Foreground;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Worker") FName RouteName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Worker") int32 TargetFloor = 1;

	/** Ground truth, recomputed every step. Scoring only - estimators must not read it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truth") int32 TruthFloor = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Truth") bool bOnHoist = false;

	/** Last estimate received over the bridge; -1 = none yet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estimate") int32 EstFloor = -1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Estimate") bool bHasEstimate = false;

	void InitFromShift(const FShiftRow& Row, ASimBuilding* InBuilding, ASimHoist* InHoist);
	void Step(float Dt, USimConfig* Config);

	/** Hoist callbacks. */
	void OnBoardedHoist(ASimHoist* InHoist);
	void FollowHoist(const FVector& CarLocation);
	void OnHoistArrived(int32 Floor);

	UFUNCTION(BlueprintCallable, Category = "Estimate") void ApplyEstimate(int32 InEstFloor, const FString& Punch);
	UFUNCTION(BlueprintCallable, Category = "Worker") void SetPhoneState(EPhoneState NewState);
	UFUNCTION(BlueprintCallable, Category = "Worker") void SetAppState(EAppState NewState);

	UFUNCTION(BlueprintPure, Category = "Worker") UPhoneSensorsComponent* GetSensors() const { return Sensors; }
	ASimBuilding* GetBuilding() const { return Building; }

private:
	void NextSegment(USimConfig* Config);
	void BeginWalkTo(const FVector& TargetCm);
	bool StepWalk(float Dt, USimConfig* Config);
	void RefreshLabel();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Capsule;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPhoneSensorsComponent> Sensors;

	UPROPERTY() TObjectPtr<ASimBuilding> Building;
	UPROPERTY() TObjectPtr<ASimHoist> Hoist;

	TArray<FRouteRow> Segments;
	int32 SegIndex = -1;
	EWorkerMode Mode = EWorkerMode::Idle;
	float Timer = 0.f;          // WaitFor countdown / wander budget / start offset
	float SubTimer = 0.f;       // wander pause between hops
	FVector WalkTarget = FVector::ZeroVector;
	float StairsStartZ = 0.f, StairsEndZ = 0.f, StairsProgress = 0.f;
	FString PunchFlash;
	float PunchFlashRemaining = 0.f;
};
