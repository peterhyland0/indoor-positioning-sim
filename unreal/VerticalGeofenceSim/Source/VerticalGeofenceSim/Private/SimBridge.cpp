#include "SimBridge.h"
#include "SimConfig.h"
#include "SimBuilding.h"
#include "SimWorker.h"
#include "SimBeacon.h"
#include "VerticalGeofenceSim.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static FString ToCompactJson(const TSharedRef<FJsonObject>& Obj)
{
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Obj, Writer);
	return Out;
}

static double Round2(double V) { return FMath::RoundToDouble(V * 100.0) / 100.0; }

static const TCHAR* PlatformStr(EPlatform P)   { return P == EPlatform::iOS ? TEXT("ios") : TEXT("android"); }
static const TCHAR* AppStateStr(EAppState A)   { return A == EAppState::Foreground ? TEXT("fg") : TEXT("bg"); }
static const TCHAR* PhoneStateStr(EPhoneState S){ return S == EPhoneState::InHand ? TEXT("inHand") : TEXT("inPocket"); }

void USimBridge::Start(USimConfig* InConfig, ASimBuilding* InBuilding)
{
	Config = InConfig;
	Building = InBuilding;
	MessageCount = 0;
	Backoff = 1.f;
	SessionLine.Empty();

	// -SimSessionsDir=<abs path> overrides; default is <ProjectSavedDir>/Sessions.
	FString Dir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SimSessionsDir="), Dir) || Dir.IsEmpty())
	{
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sessions"));
	}
	IFileManager::Get().MakeDirectory(*Dir, true);
	LogPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Dir, FString::Printf(TEXT("%d-%s.jsonl"), Config->Seed, *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")))));

	if (!bStarted)
	{
		FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");
		bStarted = true;
	}
	Connect();
	UE_LOG(LogSim, Log, TEXT("bridge: recording to %s, socket %s"), *LogPath, *Config->BridgeUrl);
}

void USimBridge::Shutdown()
{
	if (ReconnectHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReconnectHandle);
		ReconnectHandle.Reset();
	}
	if (Socket.IsValid())
	{
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnMessage().Clear();
		if (Socket->IsConnected())
		{
			Socket->Close();
		}
		Socket.Reset();
	}
	bConnected = false;
}

void USimBridge::Connect()
{
	if (!Config)
	{
		return;
	}
	if (Socket.IsValid())
	{
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnMessage().Clear();
		Socket.Reset();
	}
	Socket = FWebSocketsModule::Get().CreateWebSocket(Config->BridgeUrl, TEXT(""));
	TWeakObjectPtr<USimBridge> WeakThis(this);

	auto ScheduleReconnect = [WeakThis]()
	{
		USimBridge* Self = WeakThis.Get();
		if (!Self || Self->ReconnectHandle.IsValid())
		{
			return;
		}
		const float Delay = Self->Backoff;
		Self->Backoff = FMath::Min(Self->Backoff * 2.f, 30.f);
		Self->ReconnectHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (USimBridge* S = WeakThis.Get())
			{
				S->ReconnectHandle.Reset();
				S->Connect();
			}
			return false;
		}), Delay);
	};

	Socket->OnConnected().AddLambda([WeakThis]()
	{
		if (USimBridge* Self = WeakThis.Get())
		{
			Self->bConnected = true;
			Self->Backoff = 1.f;
			UE_LOG(LogSim, Log, TEXT("bridge: connected"));
			// The session header is what tells the server a new run began; (re)send it on every connect.
			if (!Self->SessionLine.IsEmpty() && Self->Socket.IsValid())
			{
				Self->Socket->Send(Self->SessionLine);
			}
		}
	});
	Socket->OnConnectionError().AddLambda([WeakThis, ScheduleReconnect](const FString& Err)
	{
		if (USimBridge* Self = WeakThis.Get())
		{
			Self->bConnected = false;
			UE_LOG(LogSim, Verbose, TEXT("bridge: connection error: %s"), *Err);
			ScheduleReconnect();
		}
	});
	Socket->OnClosed().AddLambda([WeakThis, ScheduleReconnect](int32 Code, const FString& Reason, bool)
	{
		if (USimBridge* Self = WeakThis.Get())
		{
			Self->bConnected = false;
			UE_LOG(LogSim, Log, TEXT("bridge: closed (%d %s)"), Code, *Reason);
			ScheduleReconnect();
		}
	});
	Socket->OnMessage().AddLambda([WeakThis](const FString& Msg)
	{
		if (USimBridge* Self = WeakThis.Get())
		{
			Self->HandleInbound(Msg);
		}
	});
	Socket->Connect();
}

void USimBridge::Emit(const TSharedRef<FJsonObject>& Obj)
{
	Obj->SetNumberField(TEXT("v"), 1);
	const FString Line = ToCompactJson(Obj);
	// The session line is sent from OnConnected (the socket is never open yet when SendSession runs).
	if (bConnected && Socket.IsValid() && Line != SessionLine)
	{
		Socket->Send(Line);
	}
	FFileHelper::SaveStringToFile(Line + TEXT("\n"), *LogPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
	                              &IFileManager::Get(), FILEWRITE_Append);
	MessageCount++;
}

void USimBridge::SendSession()
{
	if (!Config || !Building)
	{
		return;
	}
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("type"), TEXT("session"));
	O->SetNumberField(TEXT("seed"), Config->Seed);
	O->SetNumberField(TEXT("floorHeight"), Config->FloorHeight);
	O->SetNumberField(TEXT("numFloors"), Config->NumFloors);

	TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
	C->SetNumberField(TEXT("slabLossDb"), Config->SlabLossDb);
	C->SetNumberField(TEXT("drywallLossDb"), Config->DrywallLossDb);
	C->SetNumberField(TEXT("glassLossDb"), Config->GlassLossDb);
	C->SetNumberField(TEXT("bodyLossDb"), Config->BodyLossDb);
	C->SetNumberField(TEXT("pathLossExponent"), Config->PathLossExponent);
	C->SetNumberField(TEXT("rssiSigmaDb"), Config->RssiSigmaDb);
	C->SetNumberField(TEXT("packetLoss"), Config->PacketLoss);
	C->SetNumberField(TEXT("maxRangeM"), Config->MaxRangeM);
	C->SetNumberField(TEXT("rxSensitivityDbm"), Config->RxSensitivityDbm);
	C->SetBoolField(TEXT("weatherDrift"), Config->bWeatherDrift);
	C->SetNumberField(TEXT("hoistSpeed"), Config->HoistSpeed);
	C->SetNumberField(TEXT("hoistDwellSec"), Config->HoistDwellSec);
	O->SetObjectField(TEXT("config"), C);

	TArray<TSharedPtr<FJsonValue>> Beacons;
	for (const ASimBeacon* B : Building->GetBeacons())
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		const FVector L = B->GetActorLocation() / 100.f;
		J->SetStringField(TEXT("id"), B->BeaconId.ToString());
		J->SetNumberField(TEXT("floor"), B->Floor);
		J->SetNumberField(TEXT("x"), L.X);
		J->SetNumberField(TEXT("y"), L.Y);
		J->SetNumberField(TEXT("z"), L.Z);
		J->SetNumberField(TEXT("txPowerDbm"), B->TxPowerDbm);
		Beacons.Add(MakeShared<FJsonValueObject>(J));
	}
	O->SetArrayField(TEXT("beacons"), Beacons);

	TArray<TSharedPtr<FJsonValue>> Workers;
	for (const ASimWorker* W : Building->GetWorkers())
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("id"), W->WorkerId.ToString());
		J->SetStringField(TEXT("platform"), PlatformStr(W->Platform));
		J->SetStringField(TEXT("route"), W->RouteName.ToString());
		J->SetNumberField(TEXT("targetFloor"), W->TargetFloor);
		J->SetStringField(TEXT("trade"), W->Trade.ToString());
		Workers.Add(MakeShared<FJsonValueObject>(J));
	}
	O->SetArrayField(TEXT("workers"), Workers);
	O->SetNumberField(TEXT("v"), 1);
	SessionLine = ToCompactJson(O);
	Emit(O);
}

void USimBridge::SendScan(const ASimWorker* W, const TArray<FScan>& Scans, const TArray<FRegionEvent>& RegionEvents,
                          float Pressure, float RefPressure)
{
	if (!Config || !W)
	{
		return;
	}
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("type"), TEXT("scan"));
	O->SetNumberField(TEXT("t"), Round2(Config->SimTime));
	O->SetStringField(TEXT("worker"), W->WorkerId.ToString());
	O->SetStringField(TEXT("platform"), PlatformStr(W->Platform));
	O->SetStringField(TEXT("appState"), AppStateStr(W->AppState));
	O->SetStringField(TEXT("phoneState"), PhoneStateStr(W->PhoneState));

	TArray<TSharedPtr<FJsonValue>> S;
	for (const FScan& Sc : Scans)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("b"), Sc.B.ToString());
		J->SetNumberField(TEXT("rssi"), Sc.Rssi);
		S.Add(MakeShared<FJsonValueObject>(J));
	}
	O->SetArrayField(TEXT("scans"), S);

	TArray<TSharedPtr<FJsonValue>> R;
	for (const FRegionEvent& Ev : RegionEvents)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("b"), Ev.B.ToString());
		J->SetStringField(TEXT("event"), Ev.Event);
		R.Add(MakeShared<FJsonValueObject>(J));
	}
	O->SetArrayField(TEXT("regionEvents"), R);

	O->SetNumberField(TEXT("pressure"), Round2(Pressure));
	O->SetNumberField(TEXT("refPressure"), Round2(RefPressure));

	TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
	T->SetNumberField(TEXT("floor"), W->TruthFloor);
	T->SetBoolField(TEXT("onHoist"), W->bOnHoist);
	T->SetNumberField(TEXT("z"), Round2(W->GetActorLocation().Z / 100.0));
	O->SetObjectField(TEXT("truth"), T);
	Emit(O);
}

void USimBridge::SendEvent(const FString& Name, const FString& PayloadJson)
{
	if (!Config)
	{
		return;
	}
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("type"), TEXT("event"));
	O->SetNumberField(TEXT("t"), Round2(Config->SimTime));
	O->SetStringField(TEXT("name"), Name);
	TSharedPtr<FJsonObject> Payload;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PayloadJson), Payload) || !Payload.IsValid())
	{
		Payload = MakeShared<FJsonObject>();
	}
	O->SetObjectField(TEXT("payload"), Payload);
	Emit(O);
}

void USimBridge::HandleInbound(const FString& Text)
{
	TSharedPtr<FJsonObject> Obj;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Obj) || !Obj.IsValid() || !Building)
	{
		return;
	}
	const FString Type = Obj->GetStringField(TEXT("type"));
	if (Type == TEXT("command"))
	{
		// Dashboard sabotage bar (via the server): same verbs as the console and the panel.
		FString Cmd;
		if (Obj->TryGetStringField(TEXT("cmd"), Cmd))
		{
			Building->RunCommand(Cmd);
		}
		return;
	}
	if (Type != TEXT("estimate"))
	{
		return;
	}
	const FName WorkerId(*Obj->GetStringField(TEXT("worker")));
	ASimWorker* W = Building->FindWorker(WorkerId);
	if (!W)
	{
		return;
	}
	int32 EstFloor = -1;
	if (Obj->HasTypedField<EJson::Number>(TEXT("estFloor")))
	{
		EstFloor = static_cast<int32>(Obj->GetNumberField(TEXT("estFloor")));
	}
	FString Punch;
	Obj->TryGetStringField(TEXT("punch"), Punch);
	W->ApplyEstimate(EstFloor, Punch);
}
