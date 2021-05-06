void Initialize() {
	printf("Initializing...");

	srand(Seed);
	double MaxMinPowerConsumption = MaxPowerConsumption-MinPowerConsumption;

	NumReceivingClients = 0;//Onのノード数
	NumReceivedClients = 0;//全部受信してOnのノード数
	AverageNumInterrupt = 0;
	AverageInterruptDuration = 0.0;
	MaximumInterruptDuration = 0.0;
	MinimumInterruptDuration = 1.0e32;
	for(int i=0; i<NumEdges; i++){
		for(int j=0; j<CPUCORE+1; j++){
			if(j==0) NormalizeEdgePowerConsumption[i][j] = 0;
			else NormalizeEdgePowerConsumption[i][j] = ( EdgePowerConsumption[i][j] - EdgePowerConsumption[i][j-1] - MinPowerConsumption ) / MaxMinPowerConsumption;
		}
	}
	for(int j=0; j<CPUCORE+1; j++){
			if(j==0) NormalizeCloudPowerConsumption[j] = 0;
			else NormalizeCloudPowerConsumption[j] = ( CloudPowerConsumption[j] - CloudPowerConsumption[j-1] - MinPowerConsumption ) / MaxMinPowerConsumption;
	}

	TopEvent = NULL;
	NextEventNum = 0;
	NumPieces=(int)(BitRate * Duration / PieceSize / 8.0);
	if (MAXNUMPIECES < NumPieces)
		printf("Error Exceed MAXNUMPIECES\n");
	else
		printf("%d pieces\n", NumPieces);

	if( MAXNUMVIDEOS<NumVideos)
		printf("Error Exceed MAXNUMVIDEOS\n");

#ifdef LOG
	char FileName[64];
	sprintf(FileName, "log%d.txt", DistributionMethod);
	LogFile = myfopen(FileName, "w");
#endif

	printf("Done\n");
}
void InitializeCloudNode(double CloudEdgeBandwidth) {
	CloudNode.CloudEdgeWaitingList = NULL;
	CloudNode.State = ONSTATE;
	CloudNode.CloudEdgeReadBytes= 0.0;
	CloudNode.CloudEdgeBandwidth = CloudEdgeBandwidth;
	CloudNode.CloudEdgeSendClient = NULL;
	CloudNode.CloudEdgeSendPieceID = -1;
	CloudNode.CloudEdgeSendVideoID = -1;
	CloudNode.CloudEdgeSendEdgeID = -1;
	CloudNode.NumPreviousSending = 0;
	CloudNode.NumSending = 0;

	for(int i=0; i<NumEdges; i++){
		for(int j=0; j<NumVideos; j++){
			for(int k=0; k<NumPieces; k++){
				CloudServer.ExsistPiece[i][j][k]=-1;
			}
		}
	}
	for(int i=0; i<NumEdges; i++){
		CloudServer.EdgeDiskIORead[i]=0;
		CloudServer.EdgeDiskIOWrite[i]=0;
		CloudServer.EdgeNetworkIORead[i]=0;
		CloudServer.EdgeNetworkIOWrite[i]=0;
		CloudServer.EdgeResponceTime[i]=0;
		CloudServer.EdgePowerConsumption[i]=0;
		CloudServer.EdgePreviousTime[i]=0;
	}
	CloudServer.CloudDiskIORead=0;
	CloudServer.CloudNetworkIORead=0;
	CloudServer.CloudPowerConsumption=0;
	CloudServer.CloudResponceTime=0;
	CloudServer.CloudPreviousTime=0;
}

void InitializeEdgeNodes(double EdgeEdgeBandwidth, double EdgeClientBandwidth) {
	int i, j, StartVideoID, NumVideosEachEdge, NumEdgesOneMoreVideo;

	if (MAXNUMEDGENODES < NumEdges)
		printf("Exceed MAXNUMEDGENODES\n");
	if (MAXHOTCACHE < HotCacheNumPieces)
		printf("Exceed MAXHOTCACHE\n");

	for (i = 0; i < NumEdges; i++) {
		EdgeNodes[i].ID = i;
		for (j = 0; j < HotCacheNumPieces; j++) {
			EdgeNodes[i].HotCache[j].PieceID = 0;
			EdgeNodes[i].HotCache[j].VideoID = -NumVideos;
			EdgeNodes[i].HotCache[j].Voted = 0;
		}
		EdgeNodes[i].HotCacheStart = -1;
		EdgeNodes[i].HotCacheEnd = -1;
		EdgeNodes[i].State = ONSTATE;
		EdgeNodes[i].EdgeEdgeWaitingList = NULL;
		EdgeNodes[i].EdgeClientWaitingList = NULL;
		EdgeNodes[i].OnClientList = NULL;
		EdgeNodes[i].CloudEdgeWriteBytes = 0.0;
		EdgeNodes[i].EdgeEdgeWriteBytes = 0.0;
		EdgeNodes[i].EdgeEdgeReadBytes = 0.0;
		EdgeNodes[i].EdgeClientReadBytes = 0.0;
		EdgeNodes[i].EdgeEdgeBandwidth=EdgeEdgeBandwidth;
		EdgeNodes[i].EdgeClientBandwidth=EdgeClientBandwidth;
		EdgeNodes[i].CloudEdgeReceivePieceID = -1;
		EdgeNodes[i].CloudEdgeReceiveVideoID = -1;
		EdgeNodes[i].EdgeEdgeReceivePieceID = -1;
		EdgeNodes[i].EdgeEdgeReceiveVideoID = -1;
		EdgeNodes[i].EdgeEdgeReceiveEdgeNode = NULL;
		EdgeNodes[i].EdgeEdgeSendClient = NULL;
		EdgeNodes[i].EdgeEdgeSendPieceID = -1;
		EdgeNodes[i].EdgeEdgeSendVideoID = -1;
		EdgeNodes[i].EdgeEdgeSendEdgeID = -1;
		EdgeNodes[i].NumReceiving = 0;
		EdgeNodes[i].NumPreviousSending = 0;
		EdgeNodes[i].NumSending = 0;
		EdgeNodes[i].EdgeClientSendClient = NULL;
		EdgeNodes[i].EdgeClientSendPieceID = -1;
		EdgeNodes[i].EdgeClientSendVideoID = -1;
		EdgeNodes[i].NumPreviousClientSending = 0;
		EdgeNodes[i].NumClientSending = 0;
		PredictDeleteHotCache[i] = 1;
	}

	//映像IDは若いEdgeIDから順番
	NumVideosEachEdge = NumVideos / NumEdges;
	NumEdgesOneMoreVideo = NumVideos - NumVideosEachEdge * NumEdges;
	StartVideoID = 0;
	for (i = 0; i < NumEdges; i++) {
		if (StartVideoID < NumVideos) {
			EdgeNodes[i].StartVideoID = StartVideoID;
			StartVideoID += NumVideosEachEdge;
			if (i < NumEdgesOneMoreVideo)
				StartVideoID++;
		}
		else {
			EdgeNodes[i].StartVideoID = -1;
		}
	}

}
void InitializeClientNodes() {
	int i, ConnectedEdgeCounter;
	int ClientID;
	struct event* NewEvent=NULL;
	double RequestTime;

	//到着イベント
	RequestTime = 0.0;
	for (ClientID = 0; ClientID < MAXNUMCLIENTNODES; ClientID++) {
		if (TopEvent == NULL) {
			TopEvent = new struct event;
			NewEvent = TopEvent;
		}
		else {
			NewEvent->Next = new struct event;
			NewEvent = NewEvent->Next;
		}
		addCount+=1;
		NewEvent->Time = RequestTime;
		NewEvent->EventNum = NextEventNum++;
		NewEvent->EventID = CLIENTONEVENT;
		NewEvent->ClientNode = &(ClientNodes[ClientID]);
		NewEvent->Data1 = 0;
		NewEvent->Data2 = 0;
		RequestTime += ClientOnRand();
		if (RequestTime > SimulationTime) {
			break;
		}
	}
	if (ClientID == MAXNUMCLIENTNODES) {
		printf("Exceed MAXNUMCLIENTNODES\n");
		NumClients = MAXNUMCLIENTNODES;
	}
	else {
		NumClients = ClientID + 1;
		printf("%d clients\n", NumClients);
	}
	NewEvent->Next = NULL;


	//人気はEdge毎。上位割当個数分を持つとする。それ以下は割当個数分ずつ+1へ。
	ConnectedEdgeCounter = 0;
	CalculateZipfVideo();
	for (i=0; i < NumClients; i++) {
		ClientNodes[i].ID = i;
		ClientNodes[i].State = OFFSTATE;
		ClientNodes[i].OnTime = -1.0;
		ClientNodes[i].RemainingDataSize = PieceSize * 8;//送っているデータ量の残り
		ClientNodes[i].PreviousTime = 0;
		ClientNodes[i].RemainingClientDataSize = PieceSize * 8;
		ClientNodes[i].PreviousClientTime = 0;
		ClientNodes[i].ConnectedEdgeNode = &(EdgeNodes[ConnectedEdgeCounter]);
		ClientNodes[i].VideoID = (GetZipfVideoID() + ClientNodes[i].ConnectedEdgeNode->StartVideoID) % NumVideos;//地域性を出している？Edge0は0~のvideoが見られやすい　Edge1は13~のvideoが見られやすい
		ClientNodes[i].NumInterrupt = 0;
		ClientNodes[i].Interrupts = NULL;
		ClientNodes[i].SumInterruptDuration = 0.0;
		ClientNodes[i].EdgeClientReceivedPieceID = -1;
		ClientNodes[i].VideoEdgeNode = NULL;
		ClientNodes[i].VotedHotCachePosition = -1;
		ClientNodes[i].EdgeClientSearchedHotCachePosition = -1;
		ClientNodes[i].EdgeEdgeSearchedHotCachePosition = -1;
		ClientNodes[i].CloudEdgeSearchedHotCachePosition = -1;
		ClientNodes[i].ConnectedEdgeID = ConnectedEdgeCounter;

		for(int j=0; j<NumPieces; j++) ClientNodes[i].VideoRequestsID[j] =-1;

		if (ConnectedEdgeCounter == NumEdges-1) {
			ConnectedEdgeCounter = 0;
		}
		else {
			ConnectedEdgeCounter++;
		}
	}
}