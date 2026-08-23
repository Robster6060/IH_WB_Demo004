// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "Blueprint/UserWidget.h"

#include "Components/EditableTextBox.h"

#include "IH_P1C08_DevSeedPanelWidget.generated.h"



class UBorder;

class UButton;

class UEditableTextBox;

class UTextBlock;



/** PIE/dev-only seed panel — sculpt or random realm, regenerate islands in-place. */

UCLASS()

class IH_WB_DEMO004_API UIH_P1C08_DevSeedPanelWidget : public UUserWidget

{

	GENERATED_BODY()



protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeConstruct() override;



public:

	bool IsConsumingKeyboard() const;



	bool ProcessPanelPointerDown(const FVector2D& ScreenAbsolute);
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const { return IsPointOverPanel(ScreenAbsolute); }
	void UpdatePanelLayout();
	void ApplyDevPanelStackPosition(float TopY, float ContentHeight);

private:
	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;
	bool DispatchButtonClickIfUnder(UButton* Button, const FVector2D& ScreenAbsolute);

	UFUNCTION()

	void HandleSeedTextChanged(const FText& Text);



	UFUNCTION()

	void HandleSeedTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);



	UFUNCTION()

	void HandleSculptClicked();



	UFUNCTION()

	void HandleRandomRealmClicked();



	UFUNCTION()

	void HandleIslandMinusClicked();



	UFUNCTION()

	void HandleIslandPlusClicked();



	void EnsureWidgetTree();

	void SyncFromGameInstance();

	void RefreshIslandHint();

	void RefreshTankInfoLabel();

	void SetStatusMessage(const FString& Message, bool bIsError);

	void ApplyValidatedSeedAndRegenerate(const FString& RawSeed);

	void StepRandomRealmIslandCountAndSyncUi(int32 Delta);

	static FString SeedWithIslandCountDigit(const FString& Seed, int32 IslandCount);

	UButton* MakePanelButton(const FName& Name, const FString& Label, const FName& ClickHandler);



	static FString DefaultSeedWordsCsvPath() { return TEXT("InvisibleHand/Data/IH_PRNG_2400_Seed_Words.csv"); }



	UPROPERTY(Transient)

	TObjectPtr<UBorder> PanelBorder;



	UPROPERTY(Transient)

	TObjectPtr<UTextBlock> TitleText;



	UPROPERTY(Transient)

	TObjectPtr<UTextBlock> IslandHintText;



	UPROPERTY(Transient)

	TObjectPtr<UEditableTextBox> SeedTextBox;



	UPROPERTY(Transient)

	TObjectPtr<UTextBlock> StatusText;



	UPROPERTY(Transient)

	TObjectPtr<UTextBlock> RandomRealmCountText;



	UPROPERTY(Transient)

	TObjectPtr<UButton> SculptButton;



	UPROPERTY(Transient)

	TObjectPtr<UButton> RandomRealmButton;



	UPROPERTY(Transient)

	TObjectPtr<UButton> IslandMinusButton;



	UPROPERTY(Transient)

	TObjectPtr<UButton> IslandPlusButton;



	UPROPERTY(Transient)

	TObjectPtr<UTextBlock> TankInfoLabel;

	bool bSuppressSeedTextChanged = false;

};

