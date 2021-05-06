void ExecuteClientOnEvent(double EventTime,struct clientnode* ClientNode) {
	double OverheadTime;
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	struct clientnodelist* NewClientNodeList = new struct clientnodelist;
	ClientNode->State |= ONSTATE;
	ClientNode->OnTime = EventTime;

	NewClientNodeList->ClientNode = ClientNode;
	NewClientNodeList->PieceID = -1;

	if (ConnectedEdgeNode->OnClientList == NULL) {
		ConnectedEdgeNode->OnClientList = NewClientNodeList;
		NewClientNodeList->Next = NULL;
	}
	else {
		NewClientNodeList->Next = ConnectedEdgeNode->OnClientList;
		ConnectedEdgeNode->OnClientList = NewClientNodeList;
	}
	NumReceivingClients++;

	OverheadTime = 64.0 * 8.0 / ConnectedEdgeNode->EdgeClientBandwidth;
	AddEvent(EventTime + OverheadTime, EDGECLIENTFETCHEVENT, ClientNode, 0, 0);
}

void ExecuteEdgeClientFetchEvent(double EventTime, struct clientnode* ClientNode) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int ReceiveEdgeNodeID, VideoID = ClientNode->VideoID, ReceivePieceID=ClientNode->EdgeClientReceivedPieceID+1,EdgeNodeID = ConnectedEdgeNode->ID,SearchPieceID;
	int HotCachePosition;
	double OverheadTime=0.0;
	bool Hit;
	int decidedNode;//-1ならcloud,1ならedgeから最初のpieceを取得
	bool Stored = false;
	numOfExsistPieceID = 0;
	int EdgeAccessNum;
	int CloudAccessNum;
	int i;

	//仕様変更のため使用していない
	/*for (ReceiveEdgeNodeID = 0; ReceiveEdgeNodeID < NumEdges; ReceiveEdgeNodeID++) {
		if (VideoID < EdgeNodes[ReceiveEdgeNodeID].StartVideoID)
			break;
	}
	ReceiveEdgeNodeID--;//videoがあるedgeを決定*/
	//ClientNode->VideoEdgeNode = &(EdgeNodes[ConnectedEdgeNode->ID]);//videoをとってくるedge

	for(i=0;i<NumEdges;i++){
		EdgeAccessNum = EdgeNodes[i].NumReceiving + EdgeNodes[i].NumPreviousSending + EdgeNodes[i].NumPreviousClientSending;
		if(EdgeAccessNum>16) EdgeAccessNum=16;
		CloudServer.EdgePowerConsumption[i] += EdgePowerConsumption[i][EdgeAccessNum]*( EventTime - CloudServer.EdgePreviousTime[i] );
		CloudServer.EdgePreviousTime[i] = EventTime;
	}
	CloudAccessNum = CloudNode.NumPreviousSending;
	if(CloudAccessNum>16) CloudAccessNum=16;
	CloudServer.CloudPowerConsumption += CloudPowerConsumption[CloudAccessNum]*( EventTime - CloudServer.CloudPreviousTime );
	CloudServer.CloudPreviousTime = EventTime;

	if(ClientNode->ID==1){
		int stopGG=354;
	}

	Hit = VoteHotCache(ClientNode,true);//Reserveの可能性がある

	if (Hit == true) {//一番近いedgeにpieceがある
		if(numOfExsistPieceID == NumPieces){//全てのpieceを一番近いedgeが持っている
			//(ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted)--;
			CloudServerRequest(EventTime, ClientNode, VideoID, ReceivePieceID);
			ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
			CloudServer.EdgeDiskIORead[ConnectedEdgeNode->ID] += PieceSize;
			CloudServer.EdgeNetworkIORead[ConnectedEdgeNode->ID] += PieceSize;
			//ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[0]]);
			ClientNode->PreviousClientTime = EventTime;
			ClientNode->RemainingClientDataSize = PieceSize * 8;
			ClientNode->ConnectedEdgeNode->NumClientSending += 1; 
			EdgeClientRequest(EventTime, ClientNode, false);
			return;
		}
		else {//途中からは他のエッジから取得
			//(ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted)--;
			CloudServerRequest(EventTime, ClientNode, VideoID, ReceivePieceID);
			ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
			CloudServer.EdgeDiskIORead[ConnectedEdgeNode->ID] += PieceSize;
			CloudServer.EdgeNetworkIORead[ConnectedEdgeNode->ID] += PieceSize;
			//ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[0]]);
			ClientNode->PreviousClientTime = EventTime;
			ClientNode->RemainingClientDataSize = PieceSize * 8;
			ClientNode->ConnectedEdgeNode->NumClientSending += 1;
			EdgeClientRequest(EventTime, ClientNode, false);
			return;
		}
	}

	decidedNode = CloudServerRequest(EventTime, ClientNode, VideoID, ReceivePieceID);//cloudServerにpiece受信のためのリクエストを送信

	//Edgeプリキャッシュ
	if (decidedNode != -1){//一番近いedgeは見ているvideoを保存するedgeと違う
		
		HotCachePosition = ClientNode->EdgeEdgeSearchedHotCachePosition;
		if (HotCachePosition == -1){//最初がない
			if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {//マージできない
				if(IsStoreHotCache(ClientNode, true)) {
					CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] += PieceSize;
					ConnectedEdgeNode->NumReceiving += 1;
					Stored = true;
				}
				ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[0]]);
				OverheadTime = 64.0 * 8.0 / ConnectedEdgeNode->EdgeEdgeBandwidth;

				ClientNode->VideoEdgeNode->EdgeEdgeReadBytes += PieceSize;
				CloudServer.EdgeDiskIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIOWrite[ConnectedEdgeNode->ID] += PieceSize;
				
				ClientNode->PreviousTime = EventTime + OverheadTime;
				ClientNode->RemainingDataSize = PieceSize * 8;
				ClientNode->VideoEdgeNode->NumSending += 1; 
				EdgeEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivePieceID, Stored);
				return;
			}
			
		}
		else if(ConnectedEdgeNode->HotCache[HotCachePosition].PieceID<NumPieces-1){//途中からない　実行されない！
			printf("Error edge request");
		}
		return;
	}
	else if (decidedNode == -1) {//Cloudプリキャッシュ

		HotCachePosition= ClientNode->CloudEdgeSearchedHotCachePosition;
		if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {//マージできない そのpieceを要求しているリクエストがまだない
			if(IsStoreHotCache(ClientNode, true)) {
				CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] += PieceSize;
				ConnectedEdgeNode->NumReceiving += 1;
				Stored = true;
			}
			ClientNode->VideoEdgeNode = NULL;
			OverheadTime += 64.0 * 8.0 / CloudNode.CloudEdgeBandwidth;

			CloudNode.CloudEdgeReadBytes += PieceSize;//クラウドから要求する
			CloudServer.CloudDiskIORead += PieceSize;
			CloudServer.CloudNetworkIORead += PieceSize;
			CloudServer.EdgeNetworkIOWrite[ConnectedEdgeNode->ID] += PieceSize;
			
			ClientNode->PreviousTime = EventTime + OverheadTime;
			ClientNode->RemainingDataSize = PieceSize * 8;
			CloudNode.NumSending += 1; 
			CloudEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivePieceID, Stored);
		}
	}
	else if (ConnectedEdgeNode->HotCache[HotCachePosition].PieceID < NumPieces - 1) {//途中からない 実行されない！
		printf("Error:miss cloud request");
	}
/*リトライ
	RetryTime = PredictEdgeGetPieceTime(EventTime, ClientNode->VideoID, ReceivePieceID, EdgeNode, Cloud, ClientNode->VideoEdgeNode);
	AddEvent(RetryTime, EDGECLIENTRETRYEVENT, ClientNode, ReceivePieceID, 0);
*/
}

void ExecuteEdgeClientFinishEvent(double EventTime, struct clientnode* ClientNode, int ReceivedPieceID, bool Cached) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	double PlayPosition, TimePlayPosition;
	struct interrupt* CurrentInterrupt;
	bool Hit;
	double OverheadTime = 0.0;
	int ReceivePieceID;
	int AccessNum;
	bool Store = false;

	if(ClientNode->ID==2805 && ReceivedPieceID==151) {
		int edgeclientGet=0;
	}

	if(ClientNode->ID==884&&ReceivedPieceID==47){
		int cloudGet=0;
	}

	if(ClientNode->ID==860&&ReceivedPieceID==75){
		int cloudGet=0;
	}

	ConnectedEdgeNode->State &= (~EDGECLIENTSENDSTATE);
	ClientNode->State &= (~EDGECLIENTRECEIVESTATE);

	AccessNum = ConnectedEdgeNode->NumReceiving+ConnectedEdgeNode->NumPreviousSending+ConnectedEdgeNode->NumPreviousClientSending;
	if(AccessNum>16) AccessNum=16;
	CloudServer.EdgePowerConsumption[ConnectedEdgeNode->ID] += EdgePowerConsumption[ConnectedEdgeNode->ID][AccessNum]*(EventTime-CloudServer.EdgePreviousTime[ConnectedEdgeNode->ID]);
	CloudServer.EdgePreviousTime[ConnectedEdgeNode->ID] = EventTime;

	if(ClientNode->EdgeClientSearchedHotCachePosition<0){//次のHotCacheで範囲外を参照しないように -1の時

	}else if(Cached == false
		&&ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].VideoID==ClientNode->VideoID 
		&&ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].PieceID==ReceivedPieceID
		&&ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted != -1){//見終わった
			(ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted)--;
	}

	CloudServer.EdgeDiskIORead[ConnectedEdgeNode->ID] -= PieceSize;
	CloudServer.EdgeNetworkIORead[ConnectedEdgeNode->ID] -= PieceSize;
	if(Cached == true){//edgeまたはcloudから保存した時に最初のpieceを見終わった
		if(ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].VideoID==ClientNode->VideoID 
		&&ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].PieceID==ReceivedPieceID
		&&ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted == -1){
			ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted=0;
		}
	}

	PlayPosition = 8.0 * ReceivedPieceID * PieceSize / BitRate;
	TimePlayPosition = ClientNode->OnTime + PlayPosition + ClientNode->SumInterruptDuration;

	if (TimePlayPosition < EventTime) {
		ClientNode->NumInterrupt++;
		ClientNode->SumInterruptDuration += EventTime - TimePlayPosition;
		CurrentInterrupt = new struct interrupt;
		CurrentInterrupt->StartTime = TimePlayPosition;
		CurrentInterrupt->EndTime = EventTime;
		CurrentInterrupt->PieceID = ReceivedPieceID;
		CurrentInterrupt->Next = ClientNode->Interrupts;
		ClientNode->Interrupts = CurrentInterrupt;
	}

	ConnectedEdgeNode->NumClientSending -= 1;
	EdgeClientWaiting(EventTime, ConnectedEdgeNode);//待ちの次のクライアントを実行

	if (ClientNode->EdgeClientReceivedPieceID + 1 == ReceivedPieceID) {
		ClientNode->EdgeClientReceivedPieceID++;
	}
	else {
		printf("Error EdgeClientReceivedPieceID\n");
	}

	if (ReceivedPieceID == NumPieces - 1) {
		ClientFinishReception(EventTime, ClientNode);
		ConnectedEdgeNode->NumPreviousClientSending = ConnectedEdgeNode->NumClientSending;
	}
	else {
		ReceivePieceID = ClientNode->EdgeClientReceivedPieceID + 1;
		VoteHotCache(ClientNode, false);
		Hit = SearchHotCache(ClientNode, ReceivePieceID, &(ClientNode->EdgeClientSearchedHotCachePosition), false);
		if (Hit == false) {
			
			if (ClientNode->VideoRequestsID[ReceivePieceID] != NumEdges){//他のエッジから次のpieceを取得
				
				if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
					
					if(IsStoreHotCache(ClientNode, true)) {
						CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] += PieceSize;
						ConnectedEdgeNode->NumReceiving += 1;
						Store = true;
					}
					ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivePieceID]]);
					OverheadTime += 64.0 * 8.0 / ClientNode->VideoEdgeNode->EdgeEdgeBandwidth;

					ClientNode->VideoEdgeNode->EdgeEdgeReadBytes += PieceSize;
					CloudServer.EdgeDiskIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
					CloudServer.EdgeNetworkIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
					CloudServer.EdgeNetworkIOWrite[ConnectedEdgeNode->ID] += PieceSize;

					ClientNode->PreviousTime = EventTime;
					ClientNode->RemainingDataSize = PieceSize * 8;
					ClientNode->VideoEdgeNode->NumSending += 1;
					EdgeEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);

				}
			}
			else if(ClientNode->VideoRequestsID[ReceivePieceID] == NumEdges){//クラウドから次のpieceを取得
				
				if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
					
					if(IsStoreHotCache(ClientNode, true)) {
						CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] += PieceSize;
						ConnectedEdgeNode->NumReceiving += 1;
						Store = true;
					}
					ClientNode->VideoEdgeNode = NULL;
					OverheadTime += 64.0 * 8.0 / CloudNode.CloudEdgeBandwidth;

					CloudNode.CloudEdgeReadBytes += PieceSize;
					CloudServer.CloudDiskIORead += PieceSize;
					CloudServer.CloudNetworkIORead += PieceSize;
					CloudServer.EdgeNetworkIOWrite[ConnectedEdgeNode->ID] += PieceSize;
			
					ClientNode->PreviousTime = EventTime;
					ClientNode->RemainingDataSize = PieceSize * 8;
					CloudNode.NumSending += 1;
					CloudEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
				}
			}
		}
		else {//一番近いedgeにpieceがある
			//(ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted)--;
			ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
			CloudServer.EdgeDiskIORead[ConnectedEdgeNode->ID] += PieceSize;
			CloudServer.EdgeNetworkIORead[ConnectedEdgeNode->ID] += PieceSize;
			//ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivePieceID]]);

			ClientNode->PreviousClientTime = EventTime;
			ClientNode->RemainingClientDataSize = PieceSize * 8;
			ConnectedEdgeNode->NumClientSending += 1;
			EdgeClientRequest(EventTime, ClientNode, false);
		}
		ConnectedEdgeNode->NumPreviousClientSending = ConnectedEdgeNode->NumClientSending;
	}
}
void ExecuteEdgeEdgeFinishEvent(double EventTime, struct clientnode* ClientNode,int ReceivedPieceID, bool Stored) {
	struct edgenode* ToEdgeNode = ClientNode->ConnectedEdgeNode;
	struct edgenode* FromEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivedPieceID]]);
	struct clientnode* CurrentClientNode;
	struct clientnodelist* OnClientNodeList;
	int VideoID= ClientNode->VideoID,ReceivePieceID,StoredHotCachePosition;
	double OverheadTime;
	bool Direct=false;
	bool Store = false;
	int AccessNum;

	if(ClientNode->ID==136&&ReceivedPieceID==10){
		int edgeedgeget=1;
	}
	FromEdgeNode->State &= (~EDGEEDGESENDSTATE);
	FromEdgeNode->EdgeEdgeSendPieceID = -1;
	FromEdgeNode->EdgeEdgeSendVideoID = -1;
	FromEdgeNode->EdgeEdgeSendEdgeID = -1;
	ToEdgeNode->State &= (~EDGEEDGERECEIVESTATE);
	ToEdgeNode->EdgeEdgeReceiveVideoID = -1;
	ToEdgeNode->EdgeEdgeReceivePieceID = -1;
	ToEdgeNode->EdgeEdgeReceiveEdgeNode = NULL;
	CloudServer.EdgeDiskIORead[FromEdgeNode->ID] -= PieceSize;
	CloudServer.EdgeNetworkIORead[FromEdgeNode->ID] -= PieceSize;
	CloudServer.EdgeNetworkIOWrite[ToEdgeNode->ID] -= PieceSize;

	AccessNum = ToEdgeNode->NumReceiving+ToEdgeNode->NumPreviousSending+ToEdgeNode->NumPreviousClientSending;
	if(AccessNum>16) AccessNum=16;
	CloudServer.EdgePowerConsumption[ToEdgeNode->ID] += EdgePowerConsumption[ToEdgeNode->ID][AccessNum]*(EventTime-CloudServer.EdgePreviousTime[ToEdgeNode->ID]);
	CloudServer.EdgePreviousTime[ToEdgeNode->ID] = EventTime;

	if(Stored){
		StoredHotCachePosition = StoreHotCache(ClientNode, ReceivedPieceID);
		if(StoredHotCachePosition == -1){
			printf("Edge Store Error");//保存できるはずだったのが新規クライアントが予約するために保存できない
			CloudServer.EdgeDiskIOWrite[ToEdgeNode->ID] -= PieceSize;
			ToEdgeNode->NumReceiving -= 1;
		}
	}else{
		StoredHotCachePosition = -1;
	}
	if (-1<StoredHotCachePosition) {
		Stored = true;
		ToEdgeNode->EdgeEdgeWriteBytes += PieceSize;
		CloudServer.EdgeDiskIOWrite[ToEdgeNode->ID] -= PieceSize;
		ToEdgeNode->NumReceiving -= 1;
	}
	
	OnClientNodeList = ToEdgeNode->OnClientList;
	while (OnClientNodeList != NULL) {
		CurrentClientNode = OnClientNodeList->ClientNode;
		if (((CurrentClientNode->State) & (EDGECLIENTRECEIVESTATE | EDGECLIENTWAITSTATE)) == 0) {//EDGECLIENTSTREAMない
			if ((CurrentClientNode->VideoID == VideoID) && (CurrentClientNode->EdgeClientReceivedPieceID + 1 == ReceivedPieceID)) {
				ToEdgeNode->EdgeClientReadBytes += PieceSize;
				CloudServer.EdgeDiskIORead[ToEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIORead[ToEdgeNode->ID] += PieceSize;

				ClientNode->PreviousClientTime = EventTime;
				ClientNode->RemainingClientDataSize = PieceSize * 8;
				ToEdgeNode->NumClientSending += 1;
				EdgeClientRequest(EventTime, CurrentClientNode, Stored);//Stored->true 
				if (CurrentClientNode == ClientNode) {
					if(-1< StoredHotCachePosition)
						ClientNode->EdgeClientSearchedHotCachePosition=StoredHotCachePosition;
					Direct = true;

				}
			}
		}
		OnClientNodeList = OnClientNodeList->Next;
	}
	
	FromEdgeNode->NumSending -= 1;
	EdgeEdgeWaiting(EventTime, FromEdgeNode);
	//OverheadTime = 64.0 * 8.0 / FromEdgeNode->EdgeEdgeBandwidth;
	//Direct
	if (ReceivedPieceID != NumPieces - 1 && (IsStoreHotCache(ClientNode, false) || Direct)) {//|| Direct
		
		if (SearchHotCache(ClientNode, ReceivedPieceID + 1, &(ClientNode->EdgeEdgeSearchedHotCachePosition), true)) {
			ReceivedPieceID = ToEdgeNode->HotCache[ClientNode->EdgeEdgeSearchedHotCachePosition].PieceID;
		}
		ReceivePieceID = ReceivedPieceID + 1;
		if(ReceivePieceID == NumPieces) {
			FromEdgeNode->NumPreviousSending = FromEdgeNode->NumSending;
			return;
		}
		while(ClientNode->VideoRequestsID[ReceivePieceID] == ToEdgeNode->ID){
			ReceivePieceID++;
			printf("Serached but exsit piece (EdgeEdge)\n");
		}	
		//一番近いエッジ
			//(ToEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted)--;
			/*ToEdgeNode->EdgeClientReadBytes += PieceSize;
			CloudServer.EdgeDiskIORead[ToEdgeNode->ID] += PieceSize;
			CloudServer.EdgeNetworkIORead[ToEdgeNode->ID] += PieceSize;*/
			//ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivePieceID]]);
			//EdgeClientRequest(EventTime, ClientNode, false);
		
		if (ClientNode->VideoRequestsID[ReceivePieceID] != NumEdges){//他のエッジから次のpieceを取得
			
			if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
				
				if(IsStoreHotCache(ClientNode, true)) {//意味がない　次はedgeClient通信の後に実行されるため実行しない？
					CloudServer.EdgeDiskIOWrite[ToEdgeNode->ID] += PieceSize;
					ToEdgeNode->NumReceiving += 1;
					Store = true;
				}
				ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivePieceID]]);
				OverheadTime += 64.0 * 8.0 / ClientNode->VideoEdgeNode->EdgeEdgeBandwidth;

				ClientNode->VideoEdgeNode->EdgeEdgeReadBytes += PieceSize;
				CloudServer.EdgeDiskIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIOWrite[ToEdgeNode->ID] += PieceSize;

				ClientNode->PreviousTime = EventTime;
				ClientNode->RemainingDataSize = PieceSize * 8;
				ClientNode->VideoEdgeNode->NumSending += 1;
				EdgeEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		else if(ClientNode->VideoRequestsID[ReceivePieceID] == NumEdges){//クラウドから次のpieceを取得
			
			if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
				
				if(IsStoreHotCache(ClientNode, true)) {//意味がない　次はedgeClient通信の後に実行されるため実行しない？
					CloudServer.EdgeDiskIOWrite[ToEdgeNode->ID] += PieceSize;
					ToEdgeNode->NumReceiving += 1;
					Store = true;
				}
				ClientNode->VideoEdgeNode = NULL;
				OverheadTime += 64.0 * 8.0 / CloudNode.CloudEdgeBandwidth;

				CloudNode.CloudEdgeReadBytes += PieceSize;
				CloudServer.CloudDiskIORead += PieceSize;
				CloudServer.CloudNetworkIORead += PieceSize;
				CloudServer.EdgeNetworkIOWrite[ToEdgeNode->ID] += PieceSize;
	
				ClientNode->PreviousTime = EventTime;
				ClientNode->RemainingDataSize = PieceSize * 8;
				CloudNode.NumSending += 1;
				CloudEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		/*if ((ReceivePieceID < NumPrePieces )
			&&(SearchReceivingWaiting(ClientNode, ReceivePieceID) == false)) {//マージできない
			FromEdgeNode->EdgeEdgeReadBytes += PieceSize;
			EdgeEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivePieceID );
		}*/
	}
	else if(ReceivedPieceID != NumPieces - 1) {
		FromEdgeNode->EdgeEdgeReadBytes -= PieceSize;
		if (Stored == true) {
			ToEdgeNode->EdgeEdgeWriteBytes -= PieceSize;
		}
		//Post先取で保存できない、誰も受信していないとき
		//FromEdgeNode->EdgeEdgeReadBytes += PieceSize;
		//EdgeEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivedPieceID);
	}
	FromEdgeNode->NumPreviousSending = FromEdgeNode->NumSending;
}
void ExecuteCloudEdgeFinishEvent(double EventTime, struct clientnode* ClientNode, int ReceivedPieceID, bool Stored) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	struct clientnode* CurrentClientNode;
	struct clientnodelist* OnClientNodeList;
	int VideoID = ClientNode->VideoID,ReceivePieceID,StoredHotCachePosition;
	bool Direct=false;
	bool Store = false;
	double OverheadTime;
	int AccessNum;

	CloudNode.State &= (~CLOUDEDGESENDSTATE);//send終了
	CloudNode.CloudEdgeSendPieceID = -1;
	CloudNode.CloudEdgeSendVideoID = -1;
	CloudNode.CloudEdgeSendEdgeID = -1;
	ConnectedEdgeNode->State &= (~CLOUDEDGERECEIVESTATE);//receive終了
	ConnectedEdgeNode->CloudEdgeReceiveVideoID = -1;
	ConnectedEdgeNode->CloudEdgeReceivePieceID = -1;
	CloudServer.CloudDiskIORead -= PieceSize;
	CloudServer.CloudNetworkIORead -= PieceSize;
	CloudServer.EdgeNetworkIOWrite[ConnectedEdgeNode->ID] -= PieceSize;

	AccessNum = CloudNode.NumPreviousSending;
	if(AccessNum>16) AccessNum=16;
	CloudServer.CloudPowerConsumption += CloudPowerConsumption[AccessNum]*(EventTime-CloudServer.CloudPreviousTime);
	CloudServer.CloudPreviousTime= EventTime;

	if(ClientNode->ID==510&&ReceivedPieceID==168){
		int cloudGet=0;
	}

	if(ClientNode->ID==860&&ReceivedPieceID==75){
		int cloudGet=0;
	}
	if(Stored){
		StoredHotCachePosition = StoreHotCache(ClientNode, ReceivedPieceID);
		if(StoredHotCachePosition == -1) {
			printf("Cloud Store Error");//保存できるはずだったのが新規クライアントが予約するために保存できない
			CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] -= PieceSize;
			ConnectedEdgeNode->NumReceiving -= 1;
		}
	}else{
		StoredHotCachePosition = -1;
	}
	if (-1<StoredHotCachePosition) {
		Stored = true;
		ConnectedEdgeNode->CloudEdgeWriteBytes += PieceSize;
		CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] -= PieceSize;
		ConnectedEdgeNode->NumReceiving -= 1;
	}
	
	OnClientNodeList = ConnectedEdgeNode->OnClientList;
	while (OnClientNodeList != NULL) {
		CurrentClientNode = OnClientNodeList->ClientNode;
		if (((CurrentClientNode->State) & (EDGECLIENTRECEIVESTATE | EDGECLIENTWAITSTATE)) == 0) {//EDGECLIENTSTREAMない
			if ((CurrentClientNode->VideoID==VideoID)&&(CurrentClientNode->EdgeClientReceivedPieceID + 1 == ReceivedPieceID)) {
				ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
				CloudServer.EdgeDiskIORead[ConnectedEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIORead[ConnectedEdgeNode->ID] += PieceSize;

				ClientNode->PreviousClientTime = EventTime;
				ClientNode->RemainingClientDataSize = PieceSize * 8;
				ConnectedEdgeNode->NumClientSending += 1;
				EdgeClientRequest(EventTime, CurrentClientNode, Stored);//Stored->true 
				if (CurrentClientNode == ClientNode) {
					if(-1< StoredHotCachePosition)
						ClientNode->EdgeClientSearchedHotCachePosition=StoredHotCachePosition;
					Direct = true;
				}
			}
		}
		OnClientNodeList = OnClientNodeList->Next;
	}

	CloudNode.NumSending -= 1; 
	CloudEdgeWaiting(EventTime);

	//OverheadTime = 64.0 * 8.0 / CloudNode.CloudEdgeBandwidth;
	//Direct
	if (ReceivedPieceID != NumPieces - 1 && (IsStoreHotCache(ClientNode, false)||Direct)) {//|| Direct
		
		if (SearchHotCache(ClientNode, ReceivedPieceID + 1, &(ClientNode->CloudEdgeSearchedHotCachePosition), true)) {//すでに次のpieceがキャッシュされているか確認
			ReceivedPieceID = ConnectedEdgeNode->HotCache[ClientNode->CloudEdgeSearchedHotCachePosition].PieceID;
		}
		ReceivePieceID = ReceivedPieceID + 1;  //ReceivedではなくReceive   次にどこを取ってくるか決定
		if(ReceivePieceID == NumPieces) {
			CloudNode.NumPreviousSending = CloudNode.NumSending;
			return;
		}
		while(ClientNode->VideoRequestsID[ReceivePieceID] == ConnectedEdgeNode->ID){
			ReceivePieceID++;
			printf("Serached but exsit piece(CloudEdge)\n");
		}
		//一番近いエッジ
			//(ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted)--;
			/*ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
			CloudServer.EdgeDiskIORead[ConnectedEdgeNode->ID] += PieceSize;
			CloudServer.EdgeNetworkIORead[ConnectedEdgeNode->ID] += PieceSize;*/
			//ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivePieceID]]);
			//EdgeClientRequest(EventTime, ClientNode, false);

		if (ClientNode->VideoRequestsID[ReceivePieceID] != NumEdges){//他のエッジから次のpieceを取得
			
			if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
				
				if(IsStoreHotCache(ClientNode, true)) {//falseだと意味がない　次はedgeClient通信の後に実行されるため実行しない？
					CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] += PieceSize;
					ConnectedEdgeNode->NumReceiving += 1;
					Store = true;
				}
				ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivePieceID]]);
				OverheadTime += 64.0 * 8.0 / ClientNode->VideoEdgeNode->EdgeEdgeBandwidth;

				ClientNode->VideoEdgeNode->EdgeEdgeReadBytes += PieceSize;
				CloudServer.EdgeDiskIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIORead[ClientNode->VideoEdgeNode->ID] += PieceSize;
				CloudServer.EdgeNetworkIOWrite[ConnectedEdgeNode->ID] += PieceSize;

				ClientNode->PreviousTime = EventTime;
				ClientNode->RemainingDataSize = PieceSize * 8;
				ClientNode->VideoEdgeNode->NumSending += 1;
				EdgeEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		else if(ClientNode->VideoRequestsID[ReceivePieceID] == NumEdges){//クラウドから次のpieceを取得
			
			if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
				
				if(IsStoreHotCache(ClientNode, true)) {//意味がない　次はedgeClient通信の後に実行されるため実行しない？
					CloudServer.EdgeDiskIOWrite[ConnectedEdgeNode->ID] += PieceSize;
					ConnectedEdgeNode->NumReceiving += 1;
					Store = true;
				}
				ClientNode->VideoEdgeNode = NULL;
				OverheadTime += 64.0 * 8.0 / CloudNode.CloudEdgeBandwidth;

				CloudNode.CloudEdgeReadBytes += PieceSize;
				CloudServer.CloudDiskIORead += PieceSize;
				CloudServer.CloudNetworkIORead += PieceSize;
				CloudServer.EdgeNetworkIOWrite[ConnectedEdgeNode->ID] += PieceSize;

				ClientNode->PreviousTime = EventTime;
				ClientNode->RemainingDataSize = PieceSize * 8;
				CloudNode.NumSending += 1;
				CloudEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		/*if ((ReceivePieceID  < NumPieces)
			&&(SearchReceivingWaiting(ClientNode, ReceivePieceID) == false)) {//マージできない
			CloudNode.CloudEdgeReadBytes += PieceSize;
			CloudEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivePieceID);
		}*/
	}
	else if(ReceivedPieceID != NumPieces - 1) {
		CloudNode.CloudEdgeReadBytes -= PieceSize;
		if (Stored == true) {
			ConnectedEdgeNode->CloudEdgeWriteBytes -= PieceSize;
		}
		//Post先取で保存できない、誰も受信していないとき
		//CloudNode.CloudEdgeReadBytes += PieceSize;
		//CloudEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivedPieceID);
	}
	CloudNode.NumPreviousSending = CloudNode.NumSending;
}