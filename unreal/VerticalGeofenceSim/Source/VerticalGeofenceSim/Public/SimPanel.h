// On-screen control / sabotage panel (Slate, no UMG assets). Every button routes through
// ASimBuilding::RunCommand so the panel, the console and scenario files behave identically.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ASimBuilding;

class SSimPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ASimBuilding>, Building)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply Run(FString Command);
	FReply RunBeacon(const TCHAR* Verb, const TCHAR* Extra = nullptr);
	FReply RunWorker(const TCHAR* Verb, const TCHAR* Extra = nullptr);
	TSharedRef<SWidget> Button(const FText& Label, TFunction<FReply()> OnClick);
	TSharedRef<SWidget> Row(const FText& Label, TSharedRef<SWidget> Content);

	TWeakObjectPtr<ASimBuilding> Building;
	TArray<TSharedPtr<FString>> BeaconIds;
	TArray<TSharedPtr<FString>> WorkerIds;
	TSharedPtr<FString> SelectedBeacon;
	TSharedPtr<FString> SelectedWorker;
	int32 SlamFloor = 12;
	float NudgeMetres = 5.f;
	float BatteryLevel = 0.1f;
};
