/*
MIT License

Copyright (c) 2024 Ben Jolley

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "CoreMinimal.h"
#include "Logging/StructuredLog.h"
#include "UObject/NoExportTypes.h"
#include "Delegates/Delegate.h"
#include "IWebSocket.h"
#include "HeatAPI.generated.h"



DECLARE_LOG_CATEGORY_EXTERN(LogHeatAPI, Verbose, All);

#define HEAT_API_LOG(Lvl, Fmt, ...) UE_LOGFMT(LogHeatAPI, Lvl, Fmt, __VA_ARGS__)



class FWebSocketsModule;



UENUM(BlueprintType)
enum class EHeatConnectionState : uint8
{
	NotConnected,
	Connected,
	Connecting,
	Reconnecting
};



USTRUCT(BlueprintType, Blueprintable, Category = "Twitch|Extension|Heat")
struct FClickData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Twitch|Extension|Heat")
		FString UserID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Twitch|Extension|Heat")
		FVector2f Coordinates;
};



UDELEGATE(BlueprintAuthorityOnly)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeatClickDelegate, const FClickData&, Click);

UDELEGATE(BlueprintAuthorityOnly)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHeatConnectDelegate);

UDELEGATE(BlueprintAuthorityOnly)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeatDisconnectionDelegate, bool, Forced);

UDELEGATE(BlueprintAuthorityOnly)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeatErrorDelegate, const FString&, Error);

UDELEGATE(BlueprintAuthorityOnly)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeatConnectionStateDelegate, EHeatConnectionState, NewState);



UCLASS(Transient, BlueprintType, MinimalAPI, Category = "Twitch|Extension|Heat")
class UHeatAPI : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Twitch|Extension|Heat", meta=(ReturnDisplayName="Success"))
		bool Initialize(const FString& ChannelID);

	UFUNCTION(BlueprintCallable, Category="Twitch|Extension|Heat")
		void Connect();

	UFUNCTION(BlueprintCallable, Category="Twitch|Extension|Heat")
		void Disconnect();

	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="Twitch|Extension|Heat", meta=(ReturnDisplayName="Connected"))
		bool IsConnected() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, BlueprintInternalUseOnly)
		EHeatConnectionState GetConnectionState() const;

	UPROPERTY(BlueprintAssignable)
		FHeatClickDelegate OnClickReceived;

	UPROPERTY(BlueprintAssignable)
		FHeatConnectDelegate OnConnected;

	UPROPERTY(BlueprintAssignable)
		FHeatDisconnectionDelegate OnDisconnected;

	UPROPERTY(BlueprintAssignable)
		FHeatConnectionStateDelegate OnConnectionState;

	UPROPERTY(BlueprintAssignable)
		FHeatErrorDelegate OnConnectionError;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ExposeOnSpawn=true))
		bool bAutoReconnect = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ExposeOnSpawn=true, UIMin=1, UIMax=600, ClampMin=1, ClampMax=600))
		float MaximumReconnectDelay = 60;

private:
	void HandleMessage(const FString& Message);
	void HandleConnection();
	void HandleConnectionClosed(int32 StatusCode, const FString& Reason, bool WasClean);
	void HandleReconnction();
	void GetURLAndProtocol(const FString& ChannelID, FString &Out_URL, FString &Out_Protocol);
	void ChangeState(EHeatConnectionState NewState);

private:
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, BlueprintGetter = GetConnectionState, meta = (AllowPrivateAccess = "true"))
		EHeatConnectionState State = EHeatConnectionState::NotConnected;

	UPROPERTY()
		FTimerHandle ReconnectTimer;

	TSharedPtr<IWebSocket> Socket = nullptr;
	FWebSocketsModule* WebSocketModule = nullptr;
	float ReconnectDelay = 1;
	bool bDisconnecting = false;
	bool bReconnecting = false;
};
