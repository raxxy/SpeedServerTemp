// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "Engine.h"
#include "UTMutator.h"
#include "UTWeapon.h"
#include "Http.h"

#include "SpeedStats.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(SpeedStatsLog, Log, All);

struct FHiScore
{
	float Time; //seconds
	FString Player;
};

UCLASS(Blueprintable, Meta = (ChildCanTick))
class ASpeedStats : public AUTMutator
{
	GENERATED_UCLASS_BODY()

public:

	//Config vars
	float UpdateInterval;
	FString ServerURL;
	FString SpecialKey;
	int32 NumLeaderEntries;

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	virtual void PostPlayerInit_Implementation(AController* C) override;

	FString MapName;

	AGameState* GameState;
	UArrayProperty* LeaderBoardProp;
	UStructProperty* HighScoreStruct;
	UFunction* OLB_Func;

	//sets the LeaderBoard array in the SpeedGameState
	void SetLeaderBoard(TArray<FHiScore>& Scores);
	//gets the LeaderBoard array in the SpeedGameState
	void GetLeaderBoard(TArray<FHiScore>& Scores);

	//Sends a new score to the server
	void UploadNewScore(FHiScore& NewScore);

	//retrieves the leaderboard for the current map
	void UpdateLeaderBoard();
	FTimerHandle UpdateLeaderBoardHandle;

	//the blueprint function we hook to get new scores
	void OnNewScore(FFrame& Stack, RESULT_DECL);

	//http stuff
	FHttpRequestPtr HttpRequest;
	void UploadNewScore_HttpProgress(FHttpRequestPtr HttpRequest, int32 NumBytes, int32 NumBytesRecv);
	void UploadNewScore_HttpComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void UpdateLeaderBoard_HttpProgress(FHttpRequestPtr HttpRequest, int32 NumBytes);
	void UpdateLeaderBoard_HttpComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	
};