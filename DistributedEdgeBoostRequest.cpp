int CloudServerRequest(double EventTime, struct clientnode* ClientNode, int VideoID, int PieceID) {
	int whichNode[MAXNUMPIECES][MAXNUMEDGENODES+1];
	int existCount = 0;
	int EdgeOrCloudFlag = 1;
	double CloudEdgeNumSending,EdgeEdgeNumSending,EdgeClientNumSending;
	double PredictEdgePowerConsumption[MAXNUMEDGENODES][CPUCORE+1];
	double PredictCloudPowerConsumption[CPUCORE+1];
	double PredictEdgeBandwidth[MAXNUMEDGENODES];
	double PredictCloudBandwidth;
	double tempAlpha, tempBeta, cost;
	double MinResponseTime,MaxResponseTime;
	int AccessNum;
	double minCost=100;
	double index=-1;
	int edgeCount=0;
	int i,j,k;

	printf("cloud DiskOut %.2fMB NICOut %.2fMB\n",CloudServer.CloudDiskIORead/1000000,CloudServer.CloudNetworkIORead/1000000);
	if(CloudServer.CloudDiskIORead<0||CloudServer.CloudNetworkIORead<0){
		int cloudServerGets=112;
	}
	for(int i=0;i<NumEdges;i++){
		printf("%d DiskOut %.2f NICOut %.2f DiskIn %.2f NICIn %.2f \n",i,CloudServer.EdgeDiskIORead[i]/1000000,CloudServer.EdgeNetworkIORead[i]/1000000,CloudServer.EdgeDiskIOWrite[i]/1000000,CloudServer.EdgeNetworkIOWrite[i]/1000000);
		if(CloudServer.EdgeDiskIORead[i] <0 || CloudServer.EdgeNetworkIORead[i] <0 || CloudServer.EdgeDiskIOWrite[i]<0 || CloudServer.EdgeNetworkIOWrite[i]<0){
			int cloudServerGet=111;
		}
	}


	if(CloudNode.NumPreviousSending==0) CloudEdgeNumSending=1.0;
	else CloudEdgeNumSending = (double)CloudNode.NumPreviousSending;
	AccessNum = CloudNode.NumPreviousSending;
	if(AccessNum>16) AccessNum=16;
	fprintf(ServerResultFile, "%.0lf\t%d\t%d\t%.2lf\t%.2lf\t%.2lf\t",AverageArrivalInterval, ClientNode->ID, CloudNode.NumSending, CloudNode.CloudEdgeBandwidth/CloudEdgeNumSending, CloudServer.CloudPowerConsumption,CloudPowerConsumption[AccessNum]);
	for(int k=0; k< NumEdges;k++){
		if(EdgeNodes[k].NumSending==0) EdgeEdgeNumSending=1;
		else  EdgeEdgeNumSending = (double)EdgeNodes[k].NumSending;
		if(EdgeNodes[k].NumClientSending==0) EdgeClientNumSending=1;
		else EdgeClientNumSending = (double)EdgeNodes[k].NumClientSending;
		AccessNum = EdgeNodes[k].NumReceiving + EdgeNodes[k].NumPreviousSending + EdgeNodes[k].NumPreviousClientSending;
		if(AccessNum>16) AccessNum=16;
		fprintf(ServerResultFile, "%d\t%d\t%d\t%.2lf\t%.2lf\t%.2lf\t%.2lf\t",EdgeNodes[k].NumSending, EdgeNodes[k].NumClientSending, EdgeNodes[k].NumReceiving, EdgeNodes[k].EdgeEdgeBandwidth/EdgeEdgeNumSending, EdgeNodes[k].EdgeClientBandwidth/EdgeClientNumSending, CloudServer.EdgePowerConsumption[k],EdgePowerConsumption[k][AccessNum]);
	}
	fprintf(ServerResultFile,"\n");
	fflush(ServerResultFile);

	if(numOfExsistPieceID == NumPieces) return EdgeOrCloudFlag;//エッジに全てのpieceがあるとき

	for(i=0;i<NumPieces; i++){
		for(int j=0;j<=NumEdges;j++){
			whichNode[i][j]=0;
		}
	}

	for(i=0; i<NumPieces; i++){//自エッジにpieceがあるかどうか
		if(CloudServer.ExsistPiece[ClientNode->ConnectedEdgeID][ClientNode->VideoID][i] == 1){
			ClientNode->VideoRequestsID[i] = ClientNode->ConnectedEdgeID;
			whichNode[i][ClientNode->ConnectedEdgeID] = 1;
		}
		else{//他エッジにpieceがあるかどうか
			for (int ReceiveEdgeNodeID = 0; ReceiveEdgeNodeID < NumEdges; ReceiveEdgeNodeID++) {
				if(CloudServer.ExsistPiece[ReceiveEdgeNodeID][ClientNode->VideoID][i] == 1){
					whichNode[i][ReceiveEdgeNodeID]= 1;
				}
			}
		}
		whichNode[i][NumEdges] = 1;//cloud has all pieces
	}

	for(j=0; j<NumPieces; j++){//各pieceの取得場所決定(ランダム)
		if(whichNode[j][ClientNode->ConnectedEdgeID] != 1){
			while(1){
				if(whichNode[j][existCount]==1){
					//if(existCount!=NumEdges){//他のエッジから取得 クラウドエッジ合わせてランダムの場合に使用
					ClientNode->VideoRequestsID[j] = existCount;
					if(existCount==NumEdges-1) existCount=0;
					else existCount++;
					//}else{//クラウドから取得 クラウドエッジ合わせてランダムの場合に使用
					//	ClientNode->VideoRequestsID[j] = existCount;
					//	existCount=0;
					//}
					break;
				}
				if(existCount==NumEdges-1) existCount=0;
				else existCount++;
				edgeCount+=1;
				if(edgeCount==NumEdges){
					if(j==0) EdgeOrCloudFlag = -1;//最初クラウドから取得&& existCount==NumEdges クラウドエッジ合わせてランダムの場合に使用
					ClientNode->VideoRequestsID[j] = NumEdges;
					break;
				}
			}
		}
		edgeCount = 0;
	}

	MinResponseTime = PieceSize*8/(CloudNode.CloudEdgeBandwidth/1);
	MaxResponseTime = PieceSize*8/(CloudNode.CloudEdgeBandwidth/200); 

	for(j=0; j<NumPieces; j++){//各pieceの取得場所決定(alpha, beta)
		if(whichNode[j][ClientNode->ConnectedEdgeID] != 1){
			tempAlpha = alpha;
			tempBeta = beta;
			if(j==0){//最初のpieceのみ
				tempAlpha = 1;
				tempBeta = 0;
			}

			for(k=0; k<NumEdges+1; k++) {
				if(whichNode[j][k]==1){
					if(k!=NumEdges){
						AccessNum = EdgeNodes[k].NumReceiving + EdgeNodes[k].NumPreviousSending + EdgeNodes[k].NumPreviousClientSending + 1;
						if(AccessNum>16) AccessNum=16;
						PredictEdgeBandwidth[k] = EdgeNodes[k].EdgeEdgeBandwidth/(EdgeNodes[k].NumSending+1);
						cost = tempAlpha * (PieceSize*8 / PredictEdgeBandwidth[k] - MinResponseTime)/(MaxResponseTime-MinResponseTime) + tempBeta * NormalizeEdgePowerConsumption[k][AccessNum];
						if(minCost>cost){
							minCost=cost;
							index=k;
						}
					}else{
						AccessNum = CloudNode.NumPreviousSending + 1;
						if(AccessNum>16) AccessNum=16;
						PredictCloudBandwidth = CloudNode.CloudEdgeBandwidth/(CloudNode.NumSending+1);
						cost = tempAlpha * (PieceSize*8 / PredictCloudBandwidth - MinResponseTime)/(MaxResponseTime-MinResponseTime) + tempBeta * NormalizeCloudPowerConsumption[AccessNum];
						if(minCost>cost){
							if(j==0) EdgeOrCloudFlag = -1;//最初クラウドから取得
							minCost=cost;
							index=k;
						}
						ClientNode->VideoRequestsID[j] = index;
						minCost=100;
					}
				}
			}
		}
	}
	
	return EdgeOrCloudFlag;//-1はcloud 1はedgeから最初のsegmentを取得
}

void CloudEdgeRequest(double EventTime, struct clientnode* ClientNode, int RequestPieceID, bool Stored) {
	double FinishTime;
	double r, Bandwidth;
	struct edgenode* EdgeNode = ClientNode->ConnectedEdgeNode;
	struct clientnodelist* WaitingList;
	struct clientnodelist* CurrentList;
	struct event* DeleteTopEvent;
	struct event* CurrentTopEvent;
	struct event* PreviousTopEvent;
	double RemainingDataSize;

	if (CloudNode.State & CLOUDEDGESENDSTATE) {//すでにどこかに送信中
		WaitingList = new struct clientnodelist;
		WaitingList->ClientNode = ClientNode;
		WaitingList->PieceID = RequestPieceID;
		WaitingList->Stored = Stored;
		WaitingList->Next = NULL;
		CurrentList = CloudNode.CloudEdgeWaitingList;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (CloudNode.CloudEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));
		if (CurrentList) {//waitinglistにリクエストを追加
			while (CurrentList->Next) {
				CurrentList->ClientNode->RemainingDataSize -= (EventTime - CurrentList->ClientNode->PreviousTime) * Bandwidth / CloudNode.NumPreviousSending;
				CurrentList->ClientNode->PreviousTime = EventTime;
				if(CurrentList->ClientNode->RemainingDataSize < 0){
					CurrentList->ClientNode->RemainingDataSize = 0;
				}
				CurrentList = CurrentList->Next;
			}
			CurrentList->ClientNode->RemainingDataSize -= (EventTime - CurrentList->ClientNode->PreviousTime) * Bandwidth / CloudNode.NumPreviousSending;
			CurrentList->ClientNode->PreviousTime = EventTime;
			if(CurrentList->ClientNode->RemainingDataSize < 0){
				CurrentList->ClientNode->RemainingDataSize = 0;
			}

			CurrentList->Next = WaitingList;
		}
		else {
			CloudNode.CloudEdgeWaitingList = WaitingList;
		}

		CurrentTopEvent = TopEvent;
		while(CurrentTopEvent){//cloudがsend中の処理を更新
			if(CurrentTopEvent->ClientNode==CloudNode.CloudEdgeSendClient
			&&CurrentTopEvent->ClientNode->VideoID==CloudNode.CloudEdgeSendVideoID
			&&CurrentTopEvent->Data1==CloudNode.CloudEdgeSendPieceID
			&&CurrentTopEvent->EventID==CLOUDEDGEFINISHEVENT) {
				DeleteTopEvent = CurrentTopEvent;
				RemainingDataSize = DeleteTopEvent->ClientNode->RemainingDataSize - (EventTime - DeleteTopEvent->ClientNode->PreviousTime) * Bandwidth / CloudNode.NumPreviousSending;
				FinishTime = EventTime + RemainingDataSize / (Bandwidth / CloudNode.NumSending);

				if(RemainingDataSize<1.0e-8 || (RemainingDataSize / (Bandwidth / CloudNode.NumSending)) < 1.0e-8) break;

				DeleteTopEvent->ClientNode->PreviousTime = EventTime;
				DeleteTopEvent->ClientNode->RemainingDataSize = RemainingDataSize;
				PreviousTopEvent->Next = DeleteTopEvent->Next;
				AddEvent(FinishTime, CLOUDEDGEFINISHEVENT, DeleteTopEvent->ClientNode,  DeleteTopEvent->Data1, DeleteTopEvent->Data2);
				delete DeleteTopEvent;
				break;
			}
			PreviousTopEvent = CurrentTopEvent;
			CurrentTopEvent = CurrentTopEvent->Next;
			
		}
	}
	else {
		CloudNode.State |= CLOUDEDGESENDSTATE;
		CloudNode.CloudEdgeSendClient = ClientNode;
		CloudNode.CloudEdgeSendPieceID = RequestPieceID;
		CloudNode.CloudEdgeSendVideoID = ClientNode->VideoID;
		CloudNode.CloudEdgeSendEdgeID = EdgeNode->ID;
		EdgeNode->State |= CLOUDEDGERECEIVESTATE;
		EdgeNode->CloudEdgeReceivePieceID = RequestPieceID;
		EdgeNode->CloudEdgeReceiveVideoID = ClientNode->VideoID;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (CloudNode.CloudEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		FinishTime = EventTime + ClientNode->RemainingDataSize / (Bandwidth / CloudNode.NumSending);
		AddEvent(FinishTime, CLOUDEDGEFINISHEVENT, ClientNode, RequestPieceID, Stored);
	}
	CloudNode.NumPreviousSending = CloudNode.NumSending;
}
void EdgeEdgeRequest(double EventTime, struct clientnode* ClientNode, int RequestPieceID, bool Stored) {
	struct edgenode* ToEdgeNode = ClientNode->ConnectedEdgeNode;
	struct edgenode* FromEdgeNode = ClientNode->VideoEdgeNode;
	double FinishTime;
	double r, Bandwidth;
	struct clientnodelist* WaitingList;
	struct clientnodelist* CurrentList;
	struct event* DeleteTopEvent;
	struct event* CurrentTopEvent;
	struct event* PreviousTopEvent;
	double RemainingDataSize;

	if (FromEdgeNode->State & EDGEEDGESENDSTATE) {
		WaitingList = new struct clientnodelist;
		WaitingList->PieceID = RequestPieceID;
		WaitingList->Stored = Stored;
		WaitingList->ClientNode = ClientNode;
		WaitingList->Next = NULL;
		CurrentList = FromEdgeNode->EdgeEdgeWaitingList;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (FromEdgeNode->EdgeEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		if (CurrentList) {
			while (CurrentList->Next) {
				CurrentList->ClientNode->RemainingDataSize -= (EventTime - CurrentList->ClientNode->PreviousTime) * Bandwidth / FromEdgeNode->NumPreviousSending;
				CurrentList->ClientNode->PreviousTime = EventTime;
				if(CurrentList->ClientNode->RemainingDataSize < 0){
					CurrentList->ClientNode->RemainingDataSize = 0;
				}
				CurrentList = CurrentList->Next;
			}
			CurrentList->ClientNode->RemainingDataSize -= (EventTime - CurrentList->ClientNode->PreviousTime) * Bandwidth / FromEdgeNode->NumPreviousSending;
			CurrentList->ClientNode->PreviousTime = EventTime;
			if(CurrentList->ClientNode->RemainingDataSize < 0){
				CurrentList->ClientNode->RemainingDataSize = 0;
			}

			CurrentList->Next = WaitingList;
		}
		else {
			FromEdgeNode->EdgeEdgeWaitingList = WaitingList;
		}

		CurrentTopEvent = TopEvent;
		while(CurrentTopEvent){//edgeがsend中の処理を更新
			if(CurrentTopEvent->ClientNode==FromEdgeNode->EdgeEdgeSendClient
			&&CurrentTopEvent->ClientNode->VideoID==FromEdgeNode->EdgeEdgeSendVideoID
			&&CurrentTopEvent->Data1==FromEdgeNode->EdgeEdgeSendPieceID
			&&CurrentTopEvent->EventID==EDGEEDGEFINISHEVENT) {
				DeleteTopEvent = CurrentTopEvent;
				RemainingDataSize = DeleteTopEvent->ClientNode->RemainingDataSize - (EventTime - DeleteTopEvent->ClientNode->PreviousTime) * Bandwidth / FromEdgeNode->NumPreviousSending;
				FinishTime = EventTime + RemainingDataSize / (Bandwidth / FromEdgeNode->NumSending);

				if(RemainingDataSize<1.0e-8 || (RemainingDataSize / (Bandwidth / FromEdgeNode->NumSending)) < 1.0e-8) break;

				DeleteTopEvent->ClientNode->PreviousTime = EventTime;
				DeleteTopEvent->ClientNode->RemainingDataSize = RemainingDataSize;
				PreviousTopEvent->Next = DeleteTopEvent->Next;
				AddEvent(FinishTime, EDGEEDGEFINISHEVENT, DeleteTopEvent->ClientNode,  DeleteTopEvent->Data1, DeleteTopEvent->Data2);
				delete DeleteTopEvent;
				break;
			}
			PreviousTopEvent = CurrentTopEvent;
			CurrentTopEvent = CurrentTopEvent->Next;
		}
	}
	else {
		FromEdgeNode->State |= EDGEEDGESENDSTATE;
		FromEdgeNode->EdgeEdgeSendClient = ClientNode;
		FromEdgeNode->EdgeEdgeSendPieceID = RequestPieceID;
		FromEdgeNode->EdgeEdgeSendVideoID = ClientNode->VideoID;
		FromEdgeNode->EdgeEdgeSendEdgeID = ToEdgeNode->ID;
		ToEdgeNode->State |= EDGEEDGERECEIVESTATE;
		ToEdgeNode->EdgeEdgeReceivePieceID = RequestPieceID;
		ToEdgeNode->EdgeEdgeReceiveVideoID = ClientNode->VideoID;
		ToEdgeNode->EdgeEdgeReceiveEdgeNode = FromEdgeNode;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (FromEdgeNode->EdgeEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		FinishTime = EventTime + ClientNode->RemainingDataSize / (Bandwidth / FromEdgeNode->NumSending);
		AddEvent(FinishTime, EDGEEDGEFINISHEVENT, ClientNode, RequestPieceID, Stored);
	}
	FromEdgeNode->NumPreviousSending = FromEdgeNode->NumSending;
}

void EdgeClientRequest(double EventTime, struct clientnode* ClientNode,bool Cached) {
	struct edgenode* EdgeNode = ClientNode->ConnectedEdgeNode;
	int RequestPieceID = ClientNode->EdgeClientReceivedPieceID + 1;
	double FinishTime;
	double r, Bandwidth;
	struct clientnodelist* WaitingList;
	struct clientnodelist* CurrentList;
	struct event* DeleteTopEvent;
	struct event* CurrentTopEvent;
	struct event* PreviousTopEvent;
	double RemainingDataSize;

	if (EdgeNode->State & EDGECLIENTSENDSTATE) {//edgeがsend中かどうか
		ClientNode->State |= EDGECLIENTWAITSTATE;//send中なら待ち行列に追加
		WaitingList = new struct clientnodelist;
		WaitingList->ClientNode = ClientNode;
		if(Cached)
			WaitingList->PieceID = RequestPieceID;
		else if (RequestPieceID == 0) 
			WaitingList->PieceID = -NumPieces;
		else
			WaitingList->PieceID = -RequestPieceID;
		WaitingList->Next = NULL;
		CurrentList = EdgeNode->EdgeClientWaitingList;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (EdgeNode->EdgeClientBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		if (CurrentList) {
			while (CurrentList->Next) {
				CurrentList->ClientNode->RemainingClientDataSize -= (EventTime - CurrentList->ClientNode->PreviousClientTime) * Bandwidth / EdgeNode->NumPreviousClientSending;
				CurrentList->ClientNode->PreviousClientTime = EventTime;
				if(CurrentList->ClientNode->RemainingClientDataSize < 0){
					CurrentList->ClientNode->RemainingClientDataSize = 0;
				}
				CurrentList = CurrentList->Next;
			}
			CurrentList->ClientNode->RemainingClientDataSize -= (EventTime - CurrentList->ClientNode->PreviousClientTime) * Bandwidth / EdgeNode->NumPreviousClientSending;
			CurrentList->ClientNode->PreviousClientTime = EventTime;
			if(CurrentList->ClientNode->RemainingClientDataSize < 0){
				CurrentList->ClientNode->RemainingClientDataSize = 0;
			}
			CurrentList->Next = WaitingList;//最後に今回のクライアントを追加
		}
		else {
			EdgeNode->EdgeClientWaitingList = WaitingList;
		}

		CurrentTopEvent = TopEvent;
		while(CurrentTopEvent){//edgeがsend中の処理を更新
			if(CurrentTopEvent->ClientNode==EdgeNode->EdgeClientSendClient
			&&CurrentTopEvent->ClientNode->VideoID==EdgeNode->EdgeClientSendVideoID
			&&CurrentTopEvent->Data1==EdgeNode->EdgeClientSendPieceID
			&&CurrentTopEvent->EventID==EDGECLIENTFINISHEVENT) {
				DeleteTopEvent = CurrentTopEvent;
				RemainingDataSize = DeleteTopEvent->ClientNode->RemainingClientDataSize - (EventTime - DeleteTopEvent->ClientNode->PreviousClientTime) * Bandwidth / EdgeNode->NumPreviousClientSending;
				FinishTime = EventTime + RemainingDataSize / (Bandwidth / EdgeNode->NumClientSending);

				if(RemainingDataSize<1.0e-8 || (RemainingDataSize / (Bandwidth / EdgeNode->NumClientSending)) < 1.0e-8) break;

				DeleteTopEvent->ClientNode->PreviousClientTime = EventTime;
				DeleteTopEvent->ClientNode->RemainingClientDataSize = RemainingDataSize;
				PreviousTopEvent->Next = DeleteTopEvent->Next;
				AddEvent(FinishTime, EDGECLIENTFINISHEVENT, DeleteTopEvent->ClientNode,  DeleteTopEvent->Data1, DeleteTopEvent->Data2);
				delete DeleteTopEvent;
				break;
			}
			PreviousTopEvent = CurrentTopEvent;
			CurrentTopEvent = CurrentTopEvent->Next;
		}
	}
	else {//待ちがいない
		EdgeNode->State |= EDGECLIENTSENDSTATE;//edgeがpieceをsendする
		EdgeNode->EdgeClientSendClient = ClientNode;
		EdgeNode->EdgeClientSendVideoID = ClientNode->VideoID;
		EdgeNode->EdgeClientSendPieceID = RequestPieceID;

		ClientNode->State |= EDGECLIENTRECEIVESTATE;//clientがpieceをreceiveする

		r = (double)rand() / RAND_MAX;//帯域幅の揺れ
		Bandwidth = (EdgeNode->EdgeClientBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		FinishTime = EventTime + ClientNode->RemainingClientDataSize / (Bandwidth / EdgeNode->NumClientSending);//エッジからvideoをもらうのにかかる時間
		AddEvent(FinishTime, EDGECLIENTFINISHEVENT, ClientNode, RequestPieceID, Cached);//Cached falseだと一番近いエッジにpieceが保存されていない
	}
	EdgeNode->NumPreviousClientSending = EdgeNode->NumClientSending;
}