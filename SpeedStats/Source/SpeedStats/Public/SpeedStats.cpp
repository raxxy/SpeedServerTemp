#include "SpeedStats.h"
//#include "SpeedJsonSerializer.h"
#include "Json.h"

DEFINE_LOG_CATEGORY(SpeedStatsLog);

ASpeedStats* G_SpeedStats = nullptr;

ASpeedStats::ASpeedStats(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UpdateInterval = 5.0f;
	ServerURL = TEXT("http://localhost:3000");
	SpecialKey = TEXT("K3Y");
	NumLeaderEntries = 25;
}

void ASpeedStats::BeginPlay()
{
	//Super::Init_Implementation(Options);
	Super::BeginPlay();

	UE_LOG(SpeedStatsLog, Log, TEXT("==========================="));
	UE_LOG(SpeedStatsLog, Log, TEXT("SpeedStats"));
	UE_LOG(SpeedStatsLog, Log, TEXT("==========================="));

	//check and force configs. 
	if (!GConfig->GetString(TEXT("/Script/SpeedStats.SpeedStats"), TEXT("ServerURL"), ServerURL, GGameIni))
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("[/Script/SpeedStats.SpeedStats] ServerURL not set"));
		return;
	}
	if (!GConfig->GetFloat(TEXT("/Script/SpeedStats.SpeedStats"), TEXT("UpdateInterval"), UpdateInterval, GGameIni))
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("[/Script/SpeedStats.SpeedStats] UpdateInterval not set"));
		return;
	}
	if (!GConfig->GetString(TEXT("/Script/SpeedStats.SpeedStats"), TEXT("SpecialKey"), SpecialKey, GGameIni))
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("[/Script/SpeedStats.SpeedStats] SpecialKey not set"));
		return;
	}
	if (!GConfig->GetInt(TEXT("/Script/SpeedStats.SpeedStats"), TEXT("NumLeaderEntries"), NumLeaderEntries, GGameIni))
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("[/Script/SpeedStats.SpeedStats] NumLeaderEntries not set"));
		return;
	}
	ServerURL.RemoveFromEnd(FString(TEXT("/")));
	NumLeaderEntries = FMath::Max(NumLeaderEntries, 0);

	//sanity check
	AGameMode* GM = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode() : nullptr;
	if (GM == nullptr || GM->GetClass() == nullptr)
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("GetWorld() == nullptr || GetWorld()->GetAuthGameMode() == nullptr || GM->GetClass() == nullptr"));
		return;
	}

	//check for speed gamemode
	FString GameName = GM->GetClass()->GetName();
	if (!GameName.Contains(TEXT("BP_SpeedGameMode_C")))
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("Not a BP_SpeedGameMode_C"));
		return;
	}

	//check for speed game state
	GameState = GetWorld()->GameState;
	UClass* GSClass = GameState != nullptr ? GameState->GetClass() : nullptr;
	if (GameState == nullptr || !GSClass->GetName().Contains(TEXT("BP_SpeedGameState_C")))
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("Not a BP_SpeedGameState_C"));
		return;
	}
	
	//Get the leaderboard array from the gamestate
	static const FName LeaderBoard(TEXT("LeaderBoard"));
	for (UProperty* Property = GSClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
		if (ArrayProp != nullptr && Property->GetFName() == LeaderBoard)
		{
			LeaderBoardProp = ArrayProp;
			break;
		}
	}
	if (LeaderBoardProp == nullptr)
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("LeaderBoard array property not in BP_SpeedGameState_C"));
		return;
	}

	//Ensure that the highscore is an array of S_HiScores
	HighScoreStruct = Cast<UStructProperty>(LeaderBoardProp->Inner);
	if (HighScoreStruct == nullptr || HighScoreStruct->Struct == nullptr || !HighScoreStruct->Struct->GetName().Contains(TEXT("S_HiScore")))
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("LeaderBoard is not an array of S_HiScores"));
		return;
	}

	//check the size of the array
	if (sizeof(FHiScore) != HighScoreStruct->Struct->PropertiesSize)
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("S_HiScore is not the same size as FHiScore"));
		return;
	}

	//look for our function that sends the players newscore
	static const FName OnlineLeaderBoard(TEXT("OnNewScore"));
	OLB_Func = GameState->FindFunction(OnlineLeaderBoard);
	if (OLB_Func == nullptr)
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("OnNewScore blueprint func is missing"));
		return;
	}
	OLB_Func->FunctionFlags |= FUNC_Native;
	OLB_Func->SetNativeFunc((Native)&ASpeedStats::OnNewScore);

	//Shits about to get crazy...
	G_SpeedStats = this;

	//store the map name
	MapName = GetWorld()->GetMapName();

	//Everything is A OK. start the UpdateLeaderBoard timer
	GetWorld()->GetTimerManager().SetTimer(UpdateLeaderBoardHandle, this, &ASpeedStats::UpdateLeaderBoard, UpdateInterval, true);
	UpdateLeaderBoard();
}

void ASpeedStats::PostPlayerInit_Implementation(AController* C)
{
	Super::PostPlayerInit_Implementation(C);

	//upddate on player join
	if (G_SpeedStats != nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(UpdateLeaderBoardHandle, this, &ASpeedStats::UpdateLeaderBoard, UpdateInterval, true);
		UpdateLeaderBoard();
	}
}

void ASpeedStats::Destroyed()
{
	Super::Destroyed();
	G_SpeedStats = nullptr;
}

//basicaly UObject::SkipFunction()
void ASpeedStats::OnNewScore(FFrame& Stack, RESULT_DECL)
{
	if (G_SpeedStats == nullptr)
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("OnNewScore this shouldn't be happenening. "));
		SkipFunction(Stack, Result, Stack.CurrentNativeFunction);
		return;
	}
	// allocate temporary memory on the stack for evaluating parameters
	uint8* Frame = (uint8*)FMemory_Alloca(G_SpeedStats->OLB_Func->PropertiesSize);
	FMemory::Memzero(Frame, G_SpeedStats->OLB_Func->PropertiesSize);
	for (UProperty* Property = (UProperty*)G_SpeedStats->OLB_Func->Children; *Stack.Code != EX_EndFunctionParms; Property = (UProperty*)Property->Next)
	{
		Stack.MostRecentPropertyAddress = NULL;

		static const FName NewScore(TEXT("NewScore"));
		UStructProperty* StructProp = Cast<UStructProperty>(Property);
		if (StructProp != nullptr && StructProp->GetFName() == NewScore)
		{
			FHiScore* Score = StructProp->ContainerPtrToValuePtr<FHiScore>(Stack.Locals);
			G_SpeedStats->UploadNewScore(*Score);
		}
		// evaluate the expression into our temporary memory space
		Stack.Step(Stack.Object, (Property->PropertyFlags & CPF_OutParm) ? NULL : Property->ContainerPtrToValuePtr<uint8>(Frame));
	}

	// advance the code past EX_EndFunctionParms
	Stack.Code++;

	// destruct properties requiring it for which we had to use our temporary memory 
	// @warning: conditions for skipping DestroyValue() here must match conditions for passing NULL to Stack.Step() above
	for (UProperty* Destruct = G_SpeedStats->OLB_Func->DestructorLink; Destruct; Destruct = Destruct->DestructorLinkNext)
	{
		if (!Destruct->HasAnyPropertyFlags(CPF_OutParm))
		{
			Destruct->DestroyValue_InContainer(Frame);
		}
	}

	UProperty* ReturnProp = G_SpeedStats->OLB_Func->GetReturnProperty();
	if (ReturnProp != NULL)
	{
		// destroy old value if necessary
		ReturnProp->DestroyValue(Result);
		// copy zero value for return property into Result
		FMemory::Memzero(Result, ReturnProp->ArrayDim * ReturnProp->ElementSize);
	}
}

void ASpeedStats::SetLeaderBoard(TArray<FHiScore>& Scores)
{
	if (LeaderBoardProp != nullptr && GameState != nullptr)
	{
		FScriptArrayHelper_InContainer ArrayHelper(LeaderBoardProp, GameState);
		ArrayHelper.EmptyAndAddValues(Scores.Num());
		for (int32 i = 0; i < Scores.Num(); i++)
		{
			*(FHiScore*)ArrayHelper.GetRawPtr(i) = Scores[i];
		}
	}
}

void ASpeedStats::GetLeaderBoard(TArray<FHiScore>& Scores)
{
	Scores.Empty();
	if (LeaderBoardProp != nullptr && GameState != nullptr)
	{
		FScriptArrayHelper_InContainer ArrayHelper(LeaderBoardProp, GameState);
		for (int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			Scores.Add(*(FHiScore*)ArrayHelper.GetRawPtr(i));
		}
	}
}

void ASpeedStats::UploadNewScore(FHiScore& NewScore)
{
	FString DataStr = FString(TEXT("/save")) +
		TEXT("/") + MapName +
		TEXT("/") + NewScore.Player +
		TEXT("/") + FString::SanitizeFloat(NewScore.Time) + 
		TEXT("/") + SpecialKey;

	HttpRequest = FHttpModule::Get().CreateRequest();
	if (HttpRequest.IsValid())
	{
		HttpRequest->SetURL(ServerURL + DataStr);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &ASpeedStats::UploadNewScore_HttpComplete);
		HttpRequest->OnRequestProgress().BindUObject(this, &ASpeedStats::UploadNewScore_HttpProgress);
		HttpRequest->SetVerb("POST");
		if (HttpRequest->ProcessRequest())
		{
			HttpRequest->Tick(1.0f);
			return;
		}
	}
	UE_LOG(SpeedStatsLog, Warning, TEXT("UploadNewScore() fail  Player:%s Time:%f"), *NewScore.Player, NewScore.Time);
}

void ASpeedStats::UploadNewScore_HttpProgress(FHttpRequestPtr HttpRequest, int32 NumBytes){}
void ASpeedStats::UploadNewScore_HttpComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid() && (HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok || HttpResponse->GetResponseCode() == EHttpResponseCodes::Created))
	{
		//Reset the timer and update
		if (GetWorld() != nullptr)
		{
			GetWorld()->GetTimerManager().SetTimer(UpdateLeaderBoardHandle, this, &ASpeedStats::UpdateLeaderBoard, UpdateInterval, true);
			UpdateLeaderBoard();
		}
		return;
	}

	if (HttpRequest.IsValid() && HttpResponse.IsValid())
	{
		FString Response = FString(ANSI_TO_TCHAR((const ANSICHAR*)HttpResponse->GetContent().GetData()));
		UE_LOG(SpeedStatsLog, Warning, TEXT("UploadNewScore_HttpComplete() fail.  URL:%s Status:%d Response:%s"), *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *Response);
	}
	else
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("UploadNewScore_HttpComplete() fail"));
	}
}

void ASpeedStats::UpdateLeaderBoard()
{
	FString DataStr = 
		TEXT("/map/") + MapName + 
		TEXT("/") + FString::FromInt(NumLeaderEntries);	

	HttpRequest = FHttpModule::Get().CreateRequest();
	if (HttpRequest.IsValid())
	{
		HttpRequest->SetURL(ServerURL + DataStr);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &ASpeedStats::UpdateLeaderBoard_HttpComplete);
		HttpRequest->OnRequestProgress().BindUObject(this, &ASpeedStats::UpdateLeaderBoard_HttpProgress);
		HttpRequest->SetVerb("GET");
		if (HttpRequest->ProcessRequest())
		{
			return;
		}
	}
	UE_LOG(SpeedStatsLog, Warning, TEXT("UpdateLeaderBoard() fail"));
}

void ASpeedStats::UpdateLeaderBoard_HttpProgress(FHttpRequestPtr HttpRequest, int32 NumBytes){}
void ASpeedStats::UpdateLeaderBoard_HttpComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok && HttpResponse->GetContent().Num() > 0)
	{
		TArray<uint8> Content = HttpResponse->GetContent(); Content.Add(0);
		FString DataStr = FString(TEXT("{\"List\":{\"Entry\":")) + FString(ANSI_TO_TCHAR((const ANSICHAR*)Content.GetData())) + FString(TEXT("}}"));
		
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(DataStr);
		TSharedPtr<FJsonObject> JSONObject = MakeShareable(new FJsonObject());
		if (FJsonSerializer::Deserialize(Reader, JSONObject) && JSONObject.IsValid())
		{
			TSharedPtr<FJsonObject> jList = JSONObject->GetObjectField(FString(TEXT("List")));

			if (!jList.IsValid())
			{
				UE_LOG(SpeedStatsLog, Warning, TEXT("UpdateLeaderBoard_HttpComplete() Bad JSON '%s'"), *DataStr);
				return;
			}

			TArray<FHiScore> Scores;
			auto jEntries = jList->GetArrayField(FString(TEXT("Entry")));
			for (auto jEntry : jEntries)
			{
				FHiScore NewScore;
				double dTime = 0;
				if (!jEntry->AsObject().IsValid() || 
					!jEntry->AsObject()->TryGetStringField(FString(TEXT("Player")), NewScore.Player) ||
					!jEntry->AsObject()->TryGetNumberField(FString(TEXT("Time")), dTime))
				{
					UE_LOG(SpeedStatsLog, Warning, TEXT("UpdateLeaderBoard_HttpComplete() Bad JSON '%s'"), *DataStr);
					return;
				}
				NewScore.Time = dTime;
				Scores.Add(NewScore);
			}
			SetLeaderBoard(Scores);
		}
		else
		{
			UE_LOG(SpeedStatsLog, Warning, TEXT("UpdateLeaderBoard_HttpComplete() not JSON '%s'"), *DataStr);
		}
		return;
	}
	//no map entry
	else if (bSucceeded && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == EHttpResponseCodes::NoContent)
	{
		TArray<FHiScore> Empty;
		SetLeaderBoard(Empty);
		return;
	}

	if (HttpRequest.IsValid() && HttpResponse.IsValid())
	{
		FString Response = FString(ANSI_TO_TCHAR((const ANSICHAR*)HttpResponse->GetContent().GetData()));
		UE_LOG(SpeedStatsLog, Warning, TEXT("UpdateLeaderBoard_HttpComplete() fail.  URL:%s Status:%d Response:%s"), *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *Response);
	}
	else
	{
		UE_LOG(SpeedStatsLog, Warning, TEXT("UpdateLeaderBoard_HttpComplete() fail"));
	}
}