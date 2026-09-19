// Shared enums and data-table row types. Column names must match Data/*.csv headers exactly.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SimTypes.generated.h"

UENUM(BlueprintType)
enum class EPlatform : uint8
{
	iOS     UMETA(DisplayName = "iOS"),
	Android UMETA(DisplayName = "Android"),
};

UENUM(BlueprintType)
enum class EPhoneState : uint8
{
	InHand,
	InPocket,
};

UENUM(BlueprintType)
enum class EAppState : uint8
{
	Foreground,
	Background,
};

UENUM(BlueprintType)
enum class ERouteSegment : uint8
{
	WalkTo,
	WaitFor,
	BoardHoist,
	TakeStairs,
	Wander,
};

UENUM(BlueprintType)
enum class EBeaconState : uint8
{
	Alive,
	LowBattery,
	Dead,
};

/** One row of DT_BeaconLayout. */
USTRUCT(BlueprintType)
struct FBeaconRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BeaconId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Floor = 0;
	/** Building-local metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float X = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Y = 0.f;
	/** Measured power at 1 m, dBm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TxPowerDbm = -59.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AdvIntervalMs = 300;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Battery = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Alive = true;
};

/** One row of DT_Materials: RF loss per crossing of a tagged mesh. */
USTRUCT(BlueprintType)
struct FMaterialRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Tag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LossDb = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Colour = FLinearColor::White;
};

/** One row of DT_Shifts: a worker and the route they run. */
USTRUCT(BlueprintType)
struct FShiftRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WorkerId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EPlatform Platform = EPlatform::Android;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EPhoneState PhoneState = EPhoneState::InHand;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EAppState AppState = EAppState::Foreground;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RouteName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TargetFloor = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StartOffsetSec = 0.f;
	/** Cosmetic + future per-trade rules: Labourer, Electrician, Plumber, Ironworker, Carpenter, Supervisor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Trade = TEXT("Labourer");
};

/** One row of DT_Routes: a single segment of a named route. */
USTRUCT(BlueprintType)
struct FRouteRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RouteName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Order = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ERouteSegment SegmentType = ERouteSegment::WalkTo;
	/** Waypoint name, seconds, floor ("Target" = shift's TargetFloor) or +/-1, depending on SegmentType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Param;
};

/** One row of DT_Waypoints: floor-local metres. */
USTRUCT(BlueprintType)
struct FWaypointRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float X = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Y = 0.f;
};

/** One row of DT_SimConfigDefaults: documentation table, the live values are on USimConfig. */
USTRUCT(BlueprintType)
struct FConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Key;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Value;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Unit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Note;
};

/** A beacon heard during one scan. */
USTRUCT(BlueprintType)
struct FScan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName B;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Rssi = -100;
};

/** iOS-background style region enter/exit event. */
USTRUCT(BlueprintType)
struct FRegionEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName B;
	/** "enter" | "exit" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Event;
};
