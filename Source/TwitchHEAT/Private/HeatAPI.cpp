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

#include "HeatAPI.h"
#include "WebSocketsModule.h"
#include "Json.h"



DEFINE_LOG_CATEGORY(LogHeatAPI);



void UHeatAPI::GetURLAndProtocol(const FString& ChannelID, FString& Out_URL, FString& Out_Protocol)
{
  Out_Protocol = "wss";
  Out_URL = FString::Printf(TEXT("wss://heat-api.j38.net/channel/%s"), *ChannelID);
}



EHeatConnectionState UHeatAPI::GetConnectionState() const
{
  return State;
}



void UHeatAPI::ChangeState(EHeatConnectionState NewState)
{
  State = NewState;
  OnConnectionState.Broadcast(State);
}



/*
* Initializes the Websocket connection to the HEAT API endpoint, and 
*/
bool UHeatAPI::Initialize(const FString& ChannelID)
{
  if (WebSocketModule == nullptr)
  {
    WebSocketModule = &FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));

    if (WebSocketModule == nullptr)
    {
      HEAT_API_LOG(Warning, "Failed to load FWebSocketModule, have you restricted or excluded that module? It is required for the HEAT API plugin to work.");
      return false;
    }
  }

  if (Socket.IsValid())
  {
    HEAT_API_LOG(Warning, "Trying to re-initialize the HEAT object when it has already been initialized");
    return false;
  }

  FString URL, Protocol;
  GetURLAndProtocol(ChannelID, URL, Protocol);

  HEAT_API_LOG(Verbose, "Creating socket to URL {0}", URL);

  Socket = WebSocketModule->CreateWebSocket(URL, Protocol);

  if (Socket == nullptr)
  {
    HEAT_API_LOG(Error, "Could not create a websocket connection to {0}", URL);
    return false;
  }

  Socket->OnConnected().AddUObject(this, &UHeatAPI::HandleConnection);
  Socket->OnClosed().AddUObject(this, &UHeatAPI::HandleConnectionClosed);
  Socket->OnMessage().AddUObject(this, &UHeatAPI::HandleMessage);
  Socket->OnConnectionError().AddLambda([this](const FString& Reason) -> void
    {
      HEAT_API_LOG(Log, "Connection Error: {0}", Reason);
      OnConnectionError.Broadcast(Reason);
      bReconnecting = false;
      HandleReconnction();
    });

  return true;
}


/*
* Attempts to connect to connect to the websocket endpoint, if it's not already connected
*/
void UHeatAPI::Connect()
{
  if (!IsConnected())
  {
    HEAT_API_LOG(Verbose, "Attemping connection...");
    ChangeState(EHeatConnectionState::Connecting);
    Socket->Connect();
  }
}



/*
* Forces a disconnection from the websocket endpoint
*/
void UHeatAPI::Disconnect()
{
  if (bDisconnecting)
  {
    return;
  }

  // Stop our auto reconnect timer if it's running
  if (ReconnectTimer.IsValid())
  {
    UWorld* World = GetWorld();

    if (World != nullptr)
    {
      World->GetTimerManager().ClearTimer(ReconnectTimer);
    }
  }

  HEAT_API_LOG(Verbose, "Disconnecting");
  bDisconnecting = true;

  if (Socket.IsValid())
  {
    Socket->Close(1000, "User requested disconnect");
  }
}



/*
* Returns true if the socket exists, and is connected
*/
bool UHeatAPI::IsConnected() const
{
  return Socket.IsValid() && Socket->IsConnected();
}



/*
* Main message pump, handles recieved JSON messages from the HEAT websocket endpoint
* Parses JSON message to XY coodinates and user ID, and broadcasts an the OnClickReceived event
*/
void UHeatAPI::HandleMessage(const FString& Message)
{
  TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
  TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Message);
  FJsonSerializer::Deserialize(JsonReader, Root);

  FString Type = Root->GetStringField(TEXT("type"));

  if (Type.Compare(TEXT("click"), ESearchCase::IgnoreCase) != 0)
  {
    return;
  }

  FString Id = Root->GetStringField(TEXT("id"));
  float X = Root->GetNumberField(TEXT("x"));
  float Y = Root->GetNumberField(TEXT("y"));

  FVector2f Coordinates(X, Y);

  HEAT_API_LOG(Verbose, "Received Click: {0} - ({1}, {2})", Id, X, Y);

  OnClickReceived.Broadcast({Id, Coordinates});
}



void UHeatAPI::HandleConnection()
{
  HEAT_API_LOG(Verbose, "Socket Connected");
  ReconnectDelay = 1;
  bReconnecting = false;
  ChangeState(EHeatConnectionState::Connected);
  OnConnected.Broadcast();
}



void UHeatAPI::HandleConnectionClosed(int32 StatusCode, const FString& Reason, bool WasClean)
{
  HEAT_API_LOG(Verbose, "Socket Closed: {0} - {0}", StatusCode, Reason);

  // Clear our state and let everyone know our connection was closed
  ChangeState(EHeatConnectionState::NotConnected);
  OnDisconnected.Broadcast(bDisconnecting);

  if (bDisconnecting)
  {
    bDisconnecting = false;
    return;
  }

  HandleReconnction();
}



void UHeatAPI::HandleReconnction()
{
  if (bAutoReconnect && !bReconnecting)
  {
    bReconnecting = true;
    UWorld* World = GetWorld();

    if (World != nullptr)
    {
      HEAT_API_LOG(Verbose, "Reconnecting in {0} second(s)", ReconnectDelay);
      ChangeState(EHeatConnectionState::Reconnecting);

      // We have a world to create a timer, call Connect after ReconnectDelay
      World->GetTimerManager().SetTimer(ReconnectTimer, this, &UHeatAPI::Connect, ReconnectDelay);

      // Exponential decay in the range [1, MaximumReconnectDelay]
      ReconnectDelay = FMath::Max(FMath::Min(ReconnectDelay * 2, MaximumReconnectDelay), 1);
    }
    else
    {
      HEAT_API_LOG(Verbose, "We do not have a world! Cannot set timer for exponential delay reconnect. Attempting reconnect immediately");
      Connect();
    }
  }
}
