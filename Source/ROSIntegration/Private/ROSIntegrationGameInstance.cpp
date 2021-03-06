#include "ROSIntegrationGameInstance.h"
#include "RI/Topic.h"
#include "RI/Service.h"
#include "ROSTime.h"
#include "rosgraph_msgs/Clock.h"
#include "Misc/App.h"
#include <netdb.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void MarkAllROSObjectsAsDisconnected()
{
	for (TObjectIterator<UTopic> It; It; ++It)
	{
		UTopic* Topic = *It;

		Topic->MarkAsDisconnected();
	}
	for (TObjectIterator<UService> It; It; ++It)
	{
		UService* Service = *It;

		Service->MarkAsDisconnected();
	}
}

void UROSIntegrationGameInstance::Init()
{
	if (bConnectToROS)
	{
		FROSTime::SetUseSimTime(false);

		ROSIntegrationCore = NewObject<UROSIntegrationCore>(UROSIntegrationCore::StaticClass());

		char* pNEPTUNE_MASTER_IP ;
		pNEPTUNE_MASTER_IP = getenv("NEPTUNE_MASTER_IP") ;
		if (pNEPTUNE_MASTER_IP != nullptr ){
			FString fstr_pNEPTUNE_MASTER_IP(pNEPTUNE_MASTER_IP) ;
			UE_LOG(LogROS, Warning, TEXT(" NEPTUNE_MASTER_IP = %s"), *fstr_pNEPTUNE_MASTER_IP );
			ROSBridgeServerHost = fstr_pNEPTUNE_MASTER_IP ;
		}else{
			ROSBridgeServerHost = "127.0.0.1" ;
		}


		auto getIpFromDNS = [](std::string dns)->std::string{
			hostent * record = gethostbyname(dns.c_str() );
			if(record == NULL)
			{
				printf("%s is unavailable\n", dns.c_str() );
				exit(1);
			}
			in_addr * address = (in_addr * )record->h_addr;
			std::string ip_address = inet_ntoa(* address);
			return ip_address ;
		};

		if (getenv("NeptuneUE4Kubernetes")){
			std::string neptune_host_ip = "neptune-stateset-0.tssim-srv.default.svc.cluster.local" ; 
			std::cout << "[NeptuneUE4Kubernetes] exist: " << neptune_host_ip ;
			neptune_host_ip = getIpFromDNS(neptune_host_ip) ;
			std::cout << "[NeptuneUE4Kubernetes] ip : " << neptune_host_ip ;
			FString fstr_neptune(neptune_host_ip.c_str()) ;
			ROSBridgeServerHost	= fstr_neptune ;
		}else{
			std::cout << "[NeptuneUE4Kubernetes] does  NOT exist. " ;
		}

		UE_LOG(LogROS, Warning, TEXT("UROSIntegrationGameInstance::Init NEPTUNE_MASTER_ROS_BRIDGE_SERVER_IP:Port = %s:%d"), *ROSBridgeServerHost, ROSBridgeServerPort);

		bIsConnected = ROSIntegrationCore->Init(ROSBridgeServerHost, ROSBridgeServerPort);

		GetTimerManager().SetTimer(TimerHandle_CheckHealth, this, &UROSIntegrationGameInstance::CheckROSBridgeHealth, 1.0f, true, 5.0f);

		if (bIsConnected)
		{
			UWorld* CurrentWorld = GetWorld();
			if (CurrentWorld)
			{
				ROSIntegrationCore->SetWorld(CurrentWorld);
				ROSIntegrationCore->InitSpawnManager();
			}
			else
			{
				UE_LOG(LogROS, Error, TEXT("World not available in UROSIntegrationGameInstance::Init()!"));
			}
		}
		else if (!bReconnect)
		{
			UE_LOG(LogROS, Error, TEXT("Failed to connect to server %s:%u. Please make sure that your rosbridge is running."), *ROSBridgeServerHost, ROSBridgeServerPort);
		}

		if (bSimulateTime)
		{
			FApp::SetFixedDeltaTime(FixedUpdateInterval);
			FApp::SetUseFixedTimeStep(bUseFixedUpdateInterval);

			// tell ROSIntegration to use simulated time
			FROSTime now = FROSTime::Now();
			FROSTime::SetUseSimTime(true);
			FROSTime::SetSimTime(now);

			FWorldDelegates::OnWorldTickStart.AddUObject(this, &UROSIntegrationGameInstance::OnWorldTickStart);

			ClockTopic = NewObject<UTopic>(UTopic::StaticClass());
			ClockTopic->Init(ROSIntegrationCore, FString(TEXT("/clock")), FString(TEXT("rosgraph_msgs/Clock")), 3);

			ClockTopic->Advertise();
		}
	}
}

void UROSIntegrationGameInstance::CheckROSBridgeHealth()
{
	if (bIsConnected && ROSIntegrationCore->IsHealthy())
	{
		return;
	}

	if (bIsConnected)
	{
		UE_LOG(LogROS, Error, TEXT("Connection to rosbridge %s:%u was interrupted."), *ROSBridgeServerHost, ROSBridgeServerPort);
	}

	// reconnect again
	bIsConnected = false;
	bReconnect = true;
	Init();
	bReconnect = false;

	// tell everyone (Topics, Services, etc.) they lost connection and should stop any interaction with ROS for now.
	MarkAllROSObjectsAsDisconnected();

	if (!bIsConnected)
	{
		return; // Let timer call this method again to retry connection attempt
	}

	// tell everyone (Topics, Services, etc.) they can try to reconnect (subscribe and advertise)
	{
		for (TObjectIterator<UTopic> It; It; ++It)
		{
			UTopic* Topic = *It;

			bool success = Topic->Reconnect(ROSIntegrationCore);
			if (!success)
			{
				bIsConnected = false;
				UE_LOG(LogROS, Error, TEXT("Unable to re-establish topic %s."), *Topic->GetDetailedInfo());
			}
		}
		for (TObjectIterator<UService> It; It; ++It)
		{
			UService* Service = *It;

			bool success = Service->Reconnect(ROSIntegrationCore);
			if (!success)
			{
				bIsConnected = false;
				UE_LOG(LogROS, Error, TEXT("Unable to re-establish service %s."), *Service->GetDetailedInfo());
			}
		}
	}

	UE_LOG(LogROS, Display, TEXT("Successfully reconnected to rosbridge %s:%u."), *ROSBridgeServerHost, ROSBridgeServerPort);
}

void UROSIntegrationGameInstance::Shutdown()
{
	GetTimerManager().ClearTimer(TimerHandle_CheckHealth);

	FWorldDelegates::OnWorldTickStart.RemoveAll(this);
}

void UROSIntegrationGameInstance::BeginDestroy()
{
	// tell everyone (Topics, Services, etc.) they should stop any interaction with ROS.
	MarkAllROSObjectsAsDisconnected();

	Super::BeginDestroy();
}

void UROSIntegrationGameInstance::OnWorldTickStart(ELevelTick TickType, float DeltaTime)
{
	if (bSimulateTime)
	{
		FApp::SetFixedDeltaTime(FixedUpdateInterval);
		FApp::SetUseFixedTimeStep(bUseFixedUpdateInterval);

		FROSTime now = FROSTime::Now();

		// advance ROS time
		unsigned long seconds = (unsigned long)DeltaTime;
		unsigned long long nanoseconds = (unsigned long long)(DeltaTime * 1000000000ul);
		unsigned long nanoseconds_only = nanoseconds - (seconds * 1000000000ul);

		now._Sec += seconds;
		now._NSec += nanoseconds_only;

		if (now._NSec >= 1000000000ul)
		{
			now._Sec += 1;
			now._NSec -= 1000000000ul;
		}

		// internal update for ROSIntegration
		FROSTime::SetSimTime(now);

		// send /clock topic to let everyone know what time it is...
		TSharedPtr<ROSMessages::rosgraph_msgs::Clock> ClockMessage(new ROSMessages::rosgraph_msgs::Clock(now));
		ClockTopic->Publish(ClockMessage);
	}
}

