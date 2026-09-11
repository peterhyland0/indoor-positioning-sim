#include "SimPanel.h"
#include "SimBuilding.h"
#include "SimBeacon.h"
#include "SimWorker.h"
#include "SimConfig.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SimPanel"

void SSimPanel::Construct(const FArguments& InArgs)
{
	Building = InArgs._Building;
	if (ASimBuilding* B = Building.Get())
	{
		for (const ASimBeacon* Bc : B->GetBeacons()) BeaconIds.Add(MakeShared<FString>(Bc->BeaconId.ToString()));
		for (const ASimWorker* W : B->GetWorkers()) WorkerIds.Add(MakeShared<FString>(W->WorkerId.ToString()));
	}
	// Default to a mid-tower beacon so "kill" is visible from the cutaway right away.
	for (const TSharedPtr<FString>& Id : BeaconIds) if (*Id == TEXT("F12-A")) SelectedBeacon = Id;
	if (!SelectedBeacon && BeaconIds.Num()) SelectedBeacon = BeaconIds[0];
	if (WorkerIds.Num()) SelectedWorker = WorkerIds[0];

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	const FSlateFontInfo Bold = FCoreStyle::GetDefaultFontStyle("Bold", 11);

	ChildSlot
	[
		SNew(SBox).WidthOverride(430.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.78f))
			.Padding(10.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[ SNew(STextBlock).Text(LOCTEXT("Title", "SABOTAGE  (P hides this panel)")).Font(Bold).ColorAndOpacity(FLinearColor(1, 0.75f, 0.2f)) ]

				// ---- beacon
				+ SVerticalBox::Slot().AutoHeight()
				[ Row(LOCTEXT("Beacon", "Beacon"),
					SNew(STextComboBox).OptionsSource(&BeaconIds).InitiallySelectedItem(SelectedBeacon).Font(Font)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { SelectedBeacon = V; })) ]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("Kill", "Kill"),   [this] { return RunBeacon(TEXT("killbeacon")); }) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("Revive", "Revive"), [this] { return RunBeacon(TEXT("revivebeacon")); }) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("LowBatt", "Low battery"), [this] { return RunBeacon(TEXT("battery"), TEXT("0.1")); }) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("Nudge", "Move 5 m"), [this] { return RunBeacon(TEXT("nudge"), TEXT("5")); }) ]
				]

				// ---- worker
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[ Row(LOCTEXT("Worker", "Worker"),
					SNew(STextComboBox).OptionsSource(&WorkerIds).InitiallySelectedItem(SelectedWorker).Font(Font)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { SelectedWorker = V; if (V) Run(TEXT("select ") + *V); })) ]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("Pocket", "Pocket"),  [this] { return RunWorker(TEXT("pocket"), TEXT("on")); }) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("Hand", "Hand"),      [this] { return RunWorker(TEXT("pocket"), TEXT("off")); }) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("Bg", "Background"),  [this] { return RunWorker(TEXT("background"), TEXT("on")); }) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2) [ Button(LOCTEXT("Fg", "Foreground"),  [this] { return RunWorker(TEXT("background"), TEXT("off")); }) ]
				]

				// ---- environment
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2)
					[ SNew(STextBlock).Text(LOCTEXT("Storm", "Storm (weather drift)")).Font(Font) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this] { ASimBuilding* B = Building.Get(); return B && B->GetConfig() && B->GetConfig()->bWeatherDrift ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { Run(S == ECheckBoxState::Checked ? TEXT("storm on") : TEXT("storm off")); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 2, 2, 2)
					[ SNew(STextBlock).Text(LOCTEXT("Slam", "Door slam on floor")).Font(Font) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[
						SNew(SBox).WidthOverride(56.f)
						[
							SNew(SSpinBox<int32>).MinValue(0).MaxValue(30).Value_Lambda([this] { return SlamFloor; })
							.OnValueChanged_Lambda([this](int32 V) { SlamFloor = V; }).Font(Font)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[ Button(LOCTEXT("SlamGo", "Slam"), [this] { return Run(FString::Printf(TEXT("slam %d"), SlamFloor)); }) ]
				]

				// ---- run control
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[ Button(LOCTEXT("PauseResume", "Pause / Resume"), [this] { ASimBuilding* B = Building.Get(); return Run(B && B->GetConfig() && B->GetConfig()->bPaused ? TEXT("resume") : TEXT("pause")); }) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 2, 2, 2)
					[ SNew(STextBlock).Text(LOCTEXT("Debug", "Beacon traces")).Font(Font) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this] { ASimBuilding* B = Building.Get(); return B && B->GetConfig() && B->GetConfig()->bDebugTraces ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { Run(S == ECheckBoxState::Checked ? TEXT("debug on") : TEXT("debug off")); })
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SSimPanel::Row(const FText& Label, TSharedRef<SWidget> Content)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2)
		[ SNew(SBox).WidthOverride(52.f) [ SNew(STextBlock).Text(Label).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)) ] ]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2) [ Content ];
}

TSharedRef<SWidget> SSimPanel::Button(const FText& Label, TFunction<FReply()> OnClick)
{
	return SNew(SButton).Text(Label).OnClicked_Lambda(MoveTemp(OnClick));
}

FReply SSimPanel::Run(FString Command)
{
	if (ASimBuilding* B = Building.Get())
	{
		B->RunCommand(Command);
	}
	return FReply::Handled();
}

FReply SSimPanel::RunBeacon(const TCHAR* Verb, const TCHAR* Extra)
{
	if (!SelectedBeacon) return FReply::Handled();
	return Run(FString::Printf(TEXT("%s %s %s"), Verb, **SelectedBeacon, Extra ? Extra : TEXT("")));
}

FReply SSimPanel::RunWorker(const TCHAR* Verb, const TCHAR* Extra)
{
	if (!SelectedWorker) return FReply::Handled();
	return Run(FString::Printf(TEXT("%s %s %s"), Verb, **SelectedWorker, Extra ? Extra : TEXT("")));
}

#undef LOCTEXT_NAMESPACE
