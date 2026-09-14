// Unreal -> estimator bridge: one WebSocket client plus an always-on JSONL recording.
// Message shapes are documented in docs/bridge-protocol.md.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SimTypes.h"
#include "SimBridge.generated.h"

class IWebSocket;
class USimConfig;
class ASimBuilding;
class ASimWorker;

UCLASS()
class VERTICALGEOFENCESIM_API USimBridge : public UObject
{
	GENERATED_BODY()

public:
	/** Open the socket and the JSONL file for a new run. Safe to call again on reset. */
	void Start(USimConfig* InConfig, ASimBuilding* InBuilding);
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "Bridge") bool IsConnected() const { return bConnected; }
	UFUNCTION(BlueprintPure, Category = "Bridge") int32 GetMessageCount() const { return MessageCount; }
	UFUNCTION(BlueprintPure, Category = "Bridge") FString GetLogPath() const { return LogPath; }

	void SendSession();
	void SendScan(const ASimWorker* Worker, const TArray<FScan>& Scans, const TArray<FRegionEvent>& RegionEvents,
	              float Pressure, float RefPressure);
	UFUNCTION(BlueprintCallable, Category = "Bridge")
	void SendEvent(const FString& Name, const FString& PayloadJson = TEXT("{}"));

private:
	void Connect();
	void Emit(const TSharedRef<FJsonObject>& Obj);
	void HandleInbound(const FString& Text);

	TSharedPtr<IWebSocket> Socket;
	UPROPERTY() TObjectPtr<USimConfig> Config;
	UPROPERTY() TObjectPtr<ASimBuilding> Building;
	FString LogPath;
	/** Cached session header, re-sent on every (re)connect so the server always sees a run begin. */
	FString SessionLine;
	bool bConnected = false;
	bool bStarted = false;
	int32 MessageCount = 0;
	float Backoff = 1.f;
	FTSTicker::FDelegateHandle ReconnectHandle;
};
