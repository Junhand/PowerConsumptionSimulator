#pragma warning(disable:4100)
#include <stdio.h>
#include "stdlib.h"
#include "math.h"
#include <string.h>
#define PI 3.14159265358979323846264338327950288419716939937510 

#define LOG
#define NSIM 1
#define MAXNUMEDGENODES 10
#define MAXNUMCLIENTNODES 1000000 //2147483647/96 22369621くらいまでいける
#define MAXNUMVIDEOS 100
#define MAXHOTCACHE 500000
#define MAXNUMPIECES 200

#define RETRYCYCLE(a) (8.0*PieceSize/Nodes[a].AverageInBand)

#define CLIENTONEVENT 0
#define RETRYEVENT 1
#define CLOUDEDGEFETCHEVENT 2
#define CLOUDEDGEFINISHEVENT 3
#define EDGEEDGEFETCHEVENT 4
#define EDGEEDGEFINISHEVENT 5
#define EDGECLIENTFETCHEVENT 6
#define EDGECLIENTFINISHEVENT 7
#define CLOUDEDGERETRYEVENT 8
#define EDGEEDGERETRYEVENT 9
#define EDGECLIENTRETRYEVENT 10

#define OFFSTATE 0x0000
#define ONSTATE 0x0001
#define CLOUDEDGESENDSTATE 0x0002
#define CLOUDEDGERECEIVESTATE 0x0004
#define EDGEEDGESENDSTATE 0x0008
#define EDGEEDGERECEIVESTATE 0x0010
#define EDGECLIENTSENDSTATE 0x0020
#define EDGECLIENTRECEIVESTATE 0x0040
#define EDGECLIENTWAITSTATE 0x0080
#define RECEIVEDSTATE 0x0100
//CLOUDとEDGEのWAITは複数ある場合があるため使わない
//#define CLOUDEDGEWAITSTATE 0x0008
//#define EDGEEDGEWAITSTATE 0x0040

struct interrupt {
	double	StartTime;
	double	EndTime;
	unsigned int PieceID;
	struct interrupt* Next;
};
struct hotcache {
	int VideoID;
	int PieceID;
	int Voted;
};
struct clientnodelist {
	struct clientnodelist* Next;
	struct clientnode* ClientNode;
	int PieceID;
	bool Stored;
};

//コアネットワークとクライアントネットワークは別
struct edgenode {
	unsigned int ID;
	struct hotcache HotCache[MAXHOTCACHE];
	int HotCacheStart;
	int HotCacheEnd;
	short State;
	int StartVideoID;
	struct clientnodelist* EdgeEdgeWaitingList;
	struct clientnodelist* EdgeClientWaitingList;
	struct clientnodelist* OnClientList;
	double EdgeClientReadBytes;
	double EdgeEdgeWriteBytes;
	double EdgeEdgeReadBytes;
	double CloudEdgeWriteBytes;
	double EdgeEdgeBandwidth;
	double EdgeClientBandwidth;
	int CloudEdgeReceivePieceID;
	int EdgeEdgeReceivePieceID;
	int CloudEdgeReceiveVideoID;
	int EdgeEdgeReceiveVideoID;
	int EdgeEdgeSendPieceID;
	int EdgeEdgeSendVideoID;
	int EdgeEdgeSendEdgeID;
	int NumReceiving;
	struct edgenode* EdgeEdgeReceiveEdgeNode;
};

struct cloudnode {
	struct clientnodelist* CloudEdgeWaitingList;
	double CloudEdgeReadBytes;
	short State;
	double CloudEdgeBandwidth;
	int CloudEdgeSendPieceID;
	int CloudEdgeSendVideoID;
	int CloudEdgeSendEdgeID;
};

struct clientnode {
	unsigned int ID;
	short State;
	double OnTime;
	int VideoID;
	unsigned int NumInterrupt;
	struct interrupt* Interrupts;
	double SumInterruptDuration;
	int EdgeClientReceivedPieceID;
	int VideoRequestsID[MAXNUMPIECES];
	struct edgenode* VideoEdgeNode;
	struct edgenode* ConnectedEdgeNode;
	int VotedHotCachePosition;
	int CloudEdgeSearchedHotCachePosition;
	int EdgeEdgeSearchedHotCachePosition;
	int EdgeClientSearchedHotCachePosition;
	int ConnectedEdgeID;
};

struct cloudserver {
	int ExsistPiece[MAXNUMEDGENODES][MAXNUMVIDEOS][MAXNUMPIECES];
	double EdgeDiskIORead[MAXNUMEDGENODES];
	double EdgeDiskIOWrite[MAXNUMEDGENODES];
	double EdgeNetworkIORead[MAXNUMEDGENODES];
	double EdgeNetworkIOWrite[MAXNUMEDGENODES];
	double CloudDiskIORead;
	double CloudNetworkIORead;
	double EdgePowerConsumption[MAXNUMEDGENODES];
	double CloudPowerConsumption;
	double EdgeResponceTime[MAXNUMEDGENODES];
	double CloudResponceTime;
};

struct event {
	double	Time;
	unsigned int EventNum;
	char EventID;
	struct clientnode* ClientNode;
	int Data1;
	int Data2;
	struct event* Next;
};


double AverageArrivalInterval;
double VarianceArrivalInterval;
unsigned int PieceSize;
double BitRate;
double Duration;
double SegmentTime;
double SimulationTime;
double BandwidthWaver;
int NumEdges;
int NumClients;
int NumVideos;
int NumPieces;
char RandType;
char DistributionMethod;
unsigned int NextEventNum;
int HotCacheNumPieces;
int NumPrePieces;
unsigned char PieceCheck[MAXNUMPIECES / 8+1];
struct cloudnode CloudNode;
struct clientnode ClientNodes[MAXNUMCLIENTNODES];
struct edgenode EdgeNodes[MAXNUMEDGENODES];
struct event* TopEvent;
#ifdef LOG
FILE* LogFile;
#endif
double ZipfVideo[MAXNUMVIDEOS];
int NumReceivingClients;
int NumReceivedClients;
unsigned int AverageNumInterrupt;
double AverageInterruptDuration;
double MaximumInterruptDuration;
double MinimumInterruptDuration;
double LastSumInterruptDuration;
double TotalEdgeClientReadBytes;
double TotalEdgeEdgeWriteBytes;
double TotalEdgeEdgeReadBytes;
double TotalCloudEdgeWriteBytes;
double TotalCloudEdgeReadBytes;
int Seed;

int numOfExsistPieceID=0;
int cloudServerRequestFlag=0;
struct cloudserver CloudServer;

double NormalRand() {
	//ボックスミュラー法
	double r, r1, r2;
	do {
		r1 = (double)rand() / RAND_MAX;
	} while (r1 == 0.0);

	do {
		r2 = (double)rand() / RAND_MAX;
	} while (r2 == 0.0);

	r = sqrt(-2.0 * log(r1)) * cos(2.0 * r2 * PI);
	return r;
}

double ClientOnRand() {
	double r=0.0;
	long double Poisson;
	int i;

	switch (RandType) {
	case 0: //一定
		r = AverageArrivalInterval;
		break;
	case 1: //指数分布
		r = -log(((double)rand() + 1.0) / ((double)RAND_MAX + 2.0)) * AverageArrivalInterval;
		break;
	case 2://正規分布
		r = NormalRand() * VarianceArrivalInterval + AverageArrivalInterval;
		break;
	case 3: //ポアソン乱数
		Poisson = exp(700.0) * rand() / RAND_MAX;//exp(700.0)まで
		for (i = 0; Poisson > 1.0; i++) {
			Poisson *= (double)rand() / RAND_MAX;
		}
		r = (double)i / (700.0 / AverageArrivalInterval);
		break;
	}
	if (r < 1.0e-8) {
		r = 1.0e-8;
	}
	return r;
}


FILE* myfopen(const char* fname, const char* fmode) {
	FILE* fp;
	fp = fopen(fname, fmode);
	//fopen_s(&fp, fname, fmode);
	return fp;
}


void Finalize() {
	struct event* CurrentEvent;
	struct interrupt* CurrentInterrupt;
	struct clientnodelist* CurrentList;
	int i;

	printf("Finalizing...");

	TotalEdgeClientReadBytes=0.0;
	TotalEdgeEdgeWriteBytes = 0.0;
	TotalEdgeEdgeReadBytes = 0.0;
	TotalCloudEdgeWriteBytes = 0.0;
	TotalCloudEdgeReadBytes = CloudNode.CloudEdgeReadBytes;
	for (i = 0; i < NumEdges;i++) {
		TotalEdgeClientReadBytes += EdgeNodes[i].EdgeClientReadBytes;//edgeのRead クライアントから要求された量
		TotalEdgeEdgeWriteBytes += EdgeNodes[i].EdgeEdgeWriteBytes;//edgeのWrite　他のedgeから書き込んだ量
		TotalEdgeEdgeReadBytes += EdgeNodes[i].EdgeEdgeReadBytes;//edgeのread 他のedgeから要求された量
		TotalCloudEdgeWriteBytes += EdgeNodes[i].CloudEdgeWriteBytes;//edgeのwrite　クラウドから書き込まれた量
	}

	while (TopEvent) {//全てのイベントを消去
		CurrentEvent = TopEvent;
		TopEvent = TopEvent->Next;
		delete CurrentEvent;
	}

	for (i = 0; i < NumClients; i++) {//全てのinterruptsを消去
		while (ClientNodes[i].Interrupts) {
			CurrentInterrupt = ClientNodes[i].Interrupts;
			ClientNodes[i].Interrupts = CurrentInterrupt->Next;
			delete CurrentInterrupt;
		}
	}
	for (i = 0; i < NumEdges; i++) {
		while (EdgeNodes[i].EdgeEdgeWaitingList) {//edge間waitingListの消去
			CurrentList = EdgeNodes[i].EdgeEdgeWaitingList;
			EdgeNodes[i].EdgeEdgeWaitingList = CurrentList->Next;
			delete CurrentList;
		}
		while (EdgeNodes[i].EdgeClientWaitingList) {//edge　client間waitingListの消去
			CurrentList = EdgeNodes[i].EdgeClientWaitingList;
			EdgeNodes[i].EdgeClientWaitingList = CurrentList->Next;
			delete CurrentList;
		}
		while (EdgeNodes[i].OnClientList) {//clientListの消去
			CurrentList = EdgeNodes[i].OnClientList;
			EdgeNodes[i].OnClientList = CurrentList->Next;
			delete CurrentList;
		}
	}
	while (CloudNode.CloudEdgeWaitingList) {//cloud edge間waitingListの消去
		CurrentList = CloudNode.CloudEdgeWaitingList;
		CloudNode.CloudEdgeWaitingList = CurrentList->Next;
		delete CurrentList;
	}


#ifdef LOG
	fclose(LogFile);
#endif
	printf("Done\n");

}
struct event* AddEvent(double Time, char EventID, struct clientnode* ClientNode, int Data1, int Data2) {
	struct event* PreviousEvent;
	struct event* CurrentEvent;
	struct event* NewEvent;

	NewEvent = new struct event;
	NewEvent->EventID = EventID;//eventの状態
	NewEvent->EventNum = NextEventNum++;//？
	NewEvent->ClientNode= ClientNode;//clientの読みこみ
	NewEvent->Time = Time;//時間
	NewEvent->Data1 = Data1;//pieceID
	NewEvent->Data2 = Data2;//cached true false

	if (TopEvent == NULL) {//初期の場合
		TopEvent = NewEvent;
		TopEvent->Next = NULL;
		return NewEvent;
	}

	PreviousEvent = NULL;
	CurrentEvent = TopEvent;
	while (CurrentEvent) {
		if ((CurrentEvent->Time > NewEvent->Time) ||//追加するEventの方が前の時間
			((CurrentEvent->Time == NewEvent->Time) && (CurrentEvent->EventID > NewEvent->EventID))) {//同時間で新しいEventの方が段階が低い
			NewEvent->Next = CurrentEvent;
			if (PreviousEvent == NULL) {
				printf("AddEvent Error\n");//永遠に追加されるEventが先頭になる
			}
			else {
				PreviousEvent->Next = NewEvent;
			}
			return NewEvent;
		}
		PreviousEvent = CurrentEvent;//次を見る
		CurrentEvent = CurrentEvent->Next;//次を見る
	}
	PreviousEvent->Next = NewEvent;
	NewEvent->Next = CurrentEvent;//CurrentEvent=NULL
	return NewEvent;
}

void ClientFinishReception(double EventTime, struct clientnode* ClientNode) {//clientの通信終了
	struct edgenode* EdgeNode = ClientNode->ConnectedEdgeNode;
	struct clientnodelist* CurrentClientList;
	struct clientnodelist* DeleteClientList;

	NumReceivingClients--;//取得中
	NumReceivedClients++;//取得終了
	ClientNode->State |= RECEIVEDSTATE;

	if (EdgeNode->OnClientList->ClientNode == ClientNode) {//引数でもらったclientが処理中であれば処理待ちを消す(Top)
		DeleteClientList = EdgeNode->OnClientList;
		EdgeNode->OnClientList = EdgeNode->OnClientList->Next;
		delete DeleteClientList;
	}
	else {
		for (CurrentClientList = EdgeNode->OnClientList; CurrentClientList != NULL; CurrentClientList = CurrentClientList->Next) {
			DeleteClientList = CurrentClientList->Next;
			if (DeleteClientList->ClientNode == ClientNode) {
				CurrentClientList->Next = DeleteClientList->Next;
				delete DeleteClientList;
				break;
			}
		}
	}

	LastSumInterruptDuration = ClientNode->SumInterruptDuration;//そのクライアントの遅延時間
	AverageInterruptDuration += ClientNode->SumInterruptDuration;//全てのクライアントの遅延時間
	AverageNumInterrupt += ClientNode->NumInterrupt;//全てのクライアントの遅延回数
	if (MaximumInterruptDuration < ClientNode->SumInterruptDuration)
		MaximumInterruptDuration = ClientNode->SumInterruptDuration;//最大遅延時間
	if (ClientNode->SumInterruptDuration < MinimumInterruptDuration)
		MinimumInterruptDuration = ClientNode->SumInterruptDuration;//最小遅延時間
}

int CloudServerRequest(double EventTime, struct clientnode* ClientNode, int VideoID, int PieceID) {
	int whichNode[NumPieces][NumEdges+1];
	int existCount = 0;
	int EdgeOrCloudFlag = 1;
	
	for(int i=0;i<NumPieces; i++){
		for(int j=0;j<=NumEdges;j++){
			whichNode[i][j]=0;
		}
	}

	for(int i=0; i<NumPieces; i++){//自エッジにpieceがあるかどうか
		if(CloudServer.ExsistPiece[ClientNode->ConnectedEdgeID][ClientNode->VideoID][i] >= 0){
			ClientNode->VideoRequestsID[i] = ClientNode->ConnectedEdgeID;
			whichNode[i][ClientNode->ConnectedEdgeID] = 1;
		}
		else{//他エッジにpieceがあるかどうか
			for (int ReceiveEdgeNodeID = 0; ReceiveEdgeNodeID < NumEdges; ReceiveEdgeNodeID++) {
				if(CloudServer.ExsistPiece[ReceiveEdgeNodeID][ClientNode->VideoID][i] >= 0){
					whichNode[i][ReceiveEdgeNodeID]= 1;
				}
			}
		}
		whichNode[i][NumEdges] = 1;//cloud has all pieces
	}

	for(int j=0; j<NumPieces; j++){//各pieceの取得場所決定
		if(whichNode[j][ClientNode->ConnectedEdgeID] != 1){
			while(1){
				if(whichNode[j][existCount]==1){
					if(j==0 && existCount==NumEdges) EdgeOrCloudFlag = -1;//最初クラウドから取得
					if(existCount!=NumEdges){//他のエッジから取得
						ClientNode->VideoRequestsID[j] = existCount;
						existCount++;
					}else{//クラウドから取得
						ClientNode->VideoRequestsID[j] = existCount;
						existCount=0;
					}
					break;
				}
				if(existCount==NumEdges) existCount=0;
				else existCount++;
			}
		}
	}
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

	return EdgeOrCloudFlag;//-1はcloud 1はedge
}

void EdgeClientWaiting(double EventTime, struct edgenode* EdgeNode) {
	struct clientnodelist* WaitingList = EdgeNode->EdgeClientWaitingList;//edgeのclient waitingList
	struct clientnode* ClientNode;
	double Bandwidth, FinishTime, r;
	int PieceID;
	bool Cached;

	if (WaitingList !=NULL) {//待ちの次のクライアントを実行
		ClientNode = WaitingList->ClientNode;
		if (0 <= WaitingList->PieceID) {//cacheあり
			Cached = true;
			PieceID = WaitingList->PieceID;
		}
		else if (WaitingList->PieceID == -NumPieces) {//cacheなし
			Cached = false;
			PieceID = 0;
		}
		else {//cacheなし
			Cached = false;
			PieceID = -(WaitingList->PieceID);
		}
		EdgeNode->EdgeClientWaitingList = WaitingList->Next;
		delete WaitingList;

		EdgeNode->State |= EDGECLIENTSENDSTATE;
		ClientNode->State |= EDGECLIENTRECEIVESTATE;
		ClientNode->State &= ~EDGECLIENTWAITSTATE;
		r = (double)rand() / RAND_MAX;
		Bandwidth = (EdgeNode->EdgeClientBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));
		FinishTime = EventTime + 8.0 * PieceSize / Bandwidth;
		AddEvent(FinishTime, EDGECLIENTFINISHEVENT, ClientNode, PieceID, Cached);
	}
}

void EdgeEdgeWaiting(double EventTime, struct edgenode* FromEdgeNode) {
	struct clientnodelist* WaitingList = FromEdgeNode->EdgeEdgeWaitingList;
	struct edgenode* ToEdgeNode;
	struct clientnode* ClientNode;
	double Bandwidth, FinishTime, r;
	int PieceID;
	bool Stored;

	if (WaitingList != NULL) {
		ClientNode = WaitingList->ClientNode;
		PieceID = WaitingList->PieceID;
		Stored = WaitingList->Stored;
		ToEdgeNode = ClientNode->ConnectedEdgeNode;
		FromEdgeNode->EdgeEdgeWaitingList = WaitingList->Next;
		delete WaitingList;

		FromEdgeNode->State |= EDGEEDGESENDSTATE;
		FromEdgeNode->EdgeEdgeSendPieceID = PieceID;
		FromEdgeNode->EdgeEdgeSendVideoID = ClientNode->VideoID;
		FromEdgeNode->EdgeEdgeSendEdgeID = ToEdgeNode->ID;
		ToEdgeNode->State |= EDGEEDGERECEIVESTATE;
		ToEdgeNode->EdgeEdgeReceivePieceID = PieceID;
		ToEdgeNode->EdgeEdgeReceiveVideoID = ClientNode->VideoID;
		ToEdgeNode->EdgeEdgeReceiveEdgeNode = FromEdgeNode;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (FromEdgeNode->EdgeEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));
		FinishTime = EventTime + 8.0 * PieceSize / Bandwidth;
		AddEvent(FinishTime, EDGEEDGEFINISHEVENT, ClientNode, PieceID, Stored);
	}
}

void CloudEdgeWaiting(double EventTime) {
	struct clientnodelist* WaitingList = CloudNode.CloudEdgeWaitingList;
	struct edgenode* ToEdgeNode;
	struct clientnode* ClientNode;
	double Bandwidth, FinishTime, r;
	int PieceID;
	bool Stored;

	if (WaitingList != NULL) {
		ClientNode = WaitingList->ClientNode;
		PieceID = WaitingList->PieceID;
		Stored = WaitingList->Stored;
		ToEdgeNode = ClientNode->ConnectedEdgeNode;
		CloudNode.CloudEdgeWaitingList = WaitingList->Next;
		delete WaitingList;

		CloudNode.State |= CLOUDEDGESENDSTATE;
		CloudNode.CloudEdgeSendPieceID = PieceID;
		CloudNode.CloudEdgeSendVideoID = ClientNode->VideoID;
		CloudNode.CloudEdgeSendEdgeID = ToEdgeNode->ID;
		ToEdgeNode->State |= CLOUDEDGERECEIVESTATE;
		ToEdgeNode->CloudEdgeReceivePieceID = PieceID;
		ToEdgeNode->CloudEdgeReceiveVideoID = ClientNode->VideoID;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (CloudNode.CloudEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));
		FinishTime = EventTime + 8.0 * PieceSize / Bandwidth;
	
		AddEvent(FinishTime, CLOUDEDGEFINISHEVENT, ClientNode, PieceID, Stored);
	}
}

bool SearchHotCache(struct clientnode* ClientNode, int SearchPieceID, int* SearchedHostCachePosition, bool Continuous) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int VideoID = ClientNode->VideoID, CurrentPieceID;
	int HotCachePosition, EndPosition, MaximumPieceID;
	bool Hit = false, Twice = false;

	if (ClientNode->ID == 127) ClientNode->ID =127;

	if (ConnectedEdgeNode->HotCacheStart == -1) {//キャッシュされていない
		return false;
	}

	if (*SearchedHostCachePosition==-1) {
		HotCachePosition = ConnectedEdgeNode->HotCacheStart-1;
		EndPosition = ConnectedEdgeNode->HotCacheEnd;
	}
	else {
		HotCachePosition = *SearchedHostCachePosition;//高速化
		EndPosition = HotCachePosition;//HotCachePositionは+1されるので
	}

	if (Continuous) {
			MaximumPieceID = NumPieces - 1;
	}

	do{
		if (HotCachePosition == HotCacheNumPieces - 1)
			HotCachePosition = 0;
		else
			HotCachePosition++;

		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == -1)
			break;
		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == VideoID) {
			CurrentPieceID = ConnectedEdgeNode->HotCache[HotCachePosition].PieceID;
			if ( CurrentPieceID==SearchPieceID) {
				*SearchedHostCachePosition = HotCachePosition;
				Hit = true;
				//return true;
				if (Continuous) {
					Twice = true;
					if (SearchPieceID == MaximumPieceID)
						return true;
					else
						SearchPieceID++;
				}
				else {
					return true;
				}
			}
		}

	} while (HotCachePosition != EndPosition);

	if (Twice) {
		SearchHotCache(ClientNode, SearchPieceID, SearchedHostCachePosition, true);
	}

	return Hit;
}
bool VoteHotCache(struct clientnode* ClientNode, int Fetch) {
	//Fetchがtrue (Fetch時)のみ
	//	UsedHotCachePosition→Hit,
	//	PreContinuousLastHotCachePosition,
	//	PostContinuousLastHotCachePositn,
	//も更新
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int VideoID = ClientNode->VideoID, ReceivePieceID = ClientNode->EdgeClientReceivedPieceID + 1;
	int HotCachePosition, HotCacheEnd = ConnectedEdgeNode->HotCacheEnd, CurrentPieceID;
	int EdgeClientSearchPieceID=0, EdgeEdgeSearchPieceID=0, CloudEdgeSearchPieceID=0;
	bool Twice=false;

	if (ConnectedEdgeNode->HotCacheStart == -1) {//一番近いedgeに何もキャッシュされていない
		return false;
	}

	if (ClientNode->VotedHotCachePosition == -1) {
		HotCachePosition = ConnectedEdgeNode->HotCacheStart-1;
	}
	else {
		HotCachePosition = ClientNode->VotedHotCachePosition;
		if (HotCachePosition == HotCacheEnd)
			return false;
	}

	if (Fetch) {
		EdgeClientSearchPieceID = 0;
		EdgeEdgeSearchPieceID = 0;
		CloudEdgeSearchPieceID = 0;
	}
	
	do {
		if (HotCachePosition == HotCacheNumPieces - 1)
			HotCachePosition = 0;
		else
			HotCachePosition++;

		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == -1)
			break;
		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == VideoID) {
			CurrentPieceID = ConnectedEdgeNode->HotCache[HotCachePosition].PieceID;
			
			if (ReceivePieceID <= CurrentPieceID) {//欲しいpieceより大きいpieceが一番近いedgeにあった
				numOfExsistPieceID += 1;
				if (ConnectedEdgeNode->HotCache[HotCachePosition].Voted == -1)//初使用
					ConnectedEdgeNode->HotCache[HotCachePosition].Voted = 1;
				else
					ConnectedEdgeNode->HotCache[HotCachePosition].Voted++;//取得する予定
			}

			if (Fetch) {
				if (CurrentPieceID == EdgeClientSearchPieceID) {
					ClientNode->EdgeClientSearchedHotCachePosition = HotCachePosition;//clientが取得済みpiece
					EdgeClientSearchPieceID = -1;
				}
				if (CurrentPieceID == EdgeEdgeSearchPieceID) {//他のエッジからどこまで取得済みかを記録
					ClientNode->EdgeEdgeSearchedHotCachePosition = HotCachePosition;
					if (CurrentPieceID == NumPieces - 1) {
						EdgeEdgeSearchPieceID = -1;
						Twice = false;
					}else {//1週では最後までない場合
						Twice = true;
						EdgeEdgeSearchPieceID++;
					}
				}
				if (CurrentPieceID == CloudEdgeSearchPieceID) {//クラウドからどこまで取得済みかを記録
					ClientNode->CloudEdgeSearchedHotCachePosition = HotCachePosition;
					if (CurrentPieceID == NumPieces - 1) {
						CloudEdgeSearchPieceID = -1;
						Twice = false;
					}else {//1週では最後までない場合
						Twice = true;
						CloudEdgeSearchPieceID++;
					}
				}
			}
		}
	} while (HotCachePosition != HotCacheEnd);
	ClientNode->VotedHotCachePosition = HotCachePosition;

	if (Fetch && Twice) {
		HotCachePosition = ConnectedEdgeNode->HotCacheStart - 1;
		do {
			if (HotCachePosition == HotCacheNumPieces - 1)
				HotCachePosition = 0;
			else
				HotCachePosition++;

			if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == -1)
				break;
			if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == VideoID) {
				CurrentPieceID = ConnectedEdgeNode->HotCache[HotCachePosition].PieceID;
				if (CurrentPieceID == EdgeEdgeSearchPieceID) {
					ClientNode->EdgeEdgeSearchedHotCachePosition = HotCachePosition;
					if (CurrentPieceID == NumPieces - 1) {
						EdgeEdgeSearchPieceID = -1;
					}
					else {
						EdgeEdgeSearchPieceID++;
					}
				}
				if (CurrentPieceID == CloudEdgeSearchPieceID) {
					ClientNode->CloudEdgeSearchedHotCachePosition = HotCachePosition;
					if (CurrentPieceID == NumPieces - 1) {
						CloudEdgeSearchPieceID = -1;
					}
					else {
						CloudEdgeSearchPieceID++;
					}
				}
			}
		} while (HotCachePosition != HotCacheEnd);
	}

	return (EdgeClientSearchPieceID == -1);
	
}

int GetDeleteHotCachePosition(struct clientnode* ClientNode, int PieceID) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int VideoID = ClientNode->VideoID;
	int CurrentVoted = 0;
	int HotCacheStart = ConnectedEdgeNode->HotCacheStart, HotCacheEnd = ConnectedEdgeNode->HotCacheEnd;
	int CurrentHotCachePosition = ConnectedEdgeNode->HotCacheStart - 1, DeleteHotCachePosition = -1;
	struct clientnodelist* ClientList = ConnectedEdgeNode->OnClientList;

	if (HotCacheStart == -1) {
		return false;
	}

	//CurrentVoted
	while (ClientList != NULL) {
		if ((ClientList->ClientNode->VideoID == VideoID)
			&& (ClientList->ClientNode->EdgeClientReceivedPieceID < PieceID)) {//そのvideoを見る可能性がある人数
			CurrentVoted++;
		}
		ClientList = ClientList->Next;
	}

	do {
		if (CurrentHotCachePosition == HotCacheNumPieces - 1)//最後なら最初に戻る
			CurrentHotCachePosition = 0;
		else
			CurrentHotCachePosition++;//次を見る
		if (ConnectedEdgeNode->HotCache[CurrentHotCachePosition].Voted == 0) {
			DeleteHotCachePosition = CurrentHotCachePosition;//誰も見ていない
			break;
		}
		/*
		if (ConnectedEdgeNode->HotCache[CurrentHotCachePosition].Voted < CurrentVoted) {//そのvideoを見る人数の方が多い
			CurrentVoted = ConnectedEdgeNode->HotCache[CurrentHotCachePosition].Voted;
			DeleteHotCachePosition = CurrentHotCachePosition;
		}
*/
	} while (CurrentHotCachePosition != HotCacheEnd);
	return DeleteHotCachePosition;//誰も見る予定がないpieceを返す
}

bool IsStoreHotCache(struct clientnode* ClientNode, int StorePieceID) {//storeされるかどうか確認
	int VideoID = ClientNode->VideoID;
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int HotCacheStart= ConnectedEdgeNode->HotCacheStart, HotCacheEnd= ConnectedEdgeNode->HotCacheEnd,HotCacheUsed;
	int CurrentHotCachePosition,DeleteHotCachePosition;
	int CurrentNumDelete = 0;
	int CurrentNumReceiving = ConnectedEdgeNode->NumReceiving;
	
	struct clientnodelist* ClientList;
	struct clientnode* CurrentClientNode;
	int VirtualCurrentPosition,VirtualHotCacheEnd,VirtualDeleteHotCachePosition;

	if (HotCacheNumPieces == 0) {
		return false;
	}

	if (HotCacheStart == -1) {//初
		HotCacheStart = 0;
		HotCacheUsed = 0;
	}
	else {
		if (HotCacheStart <= HotCacheEnd)
			HotCacheUsed = HotCacheEnd - HotCacheStart + 1;
		else
			HotCacheUsed = HotCacheNumPieces - HotCacheStart + HotCacheEnd + 1;//使用されている数
	}

	if(HotCacheUsed< HotCacheNumPieces){//空きあるor初
		if((HotCacheNumPieces-HotCacheUsed)>CurrentNumReceiving){
			return true;
		}else{
			return false;
		}
	}
	else {//空き無し
		CurrentHotCachePosition = ConnectedEdgeNode->HotCacheStart - 1;
		
		do {
			if (CurrentHotCachePosition == HotCacheNumPieces - 1)//最後なら最初に戻る
				CurrentHotCachePosition = 0;
			else
				CurrentHotCachePosition++;//次を見る
			if (ConnectedEdgeNode->HotCache[CurrentHotCachePosition].Voted == 0) {
				//誰も見ていない
				CurrentNumDelete++;
			}
		} while (CurrentHotCachePosition != HotCacheEnd);
		
		if (CurrentNumDelete>CurrentNumReceiving){
			return true;
		}
		else {
			return false;
		}

	}
}

double PredictEdgeGetPieceTime(double EventTime, int VideoID, int PieceID, struct edgenode* EdgeNode, bool Cloud, struct edgenode* FromEdgeNode) {
	double PredictedTime;
	int i;
	struct clientnodelist* CurrentList;


	if (Cloud) {
		PredictedTime = PieceSize * 8.0 / CloudNode.CloudEdgeBandwidth;
		if ((EdgeNode->State & CLOUDEDGERECEIVESTATE) == 0) {
			i = 1;
			CurrentList = CloudNode.CloudEdgeWaitingList;
			while(CurrentList){
				i++;
				if ((CurrentList->ClientNode->VideoID == VideoID)
					|| (CurrentList->PieceID == PieceID)) {
					break;
				}
				i++;
				CurrentList = CurrentList->Next;
			}
			PredictedTime *= i;
		}
	}
	else {
		PredictedTime = PieceSize * 8.0 / FromEdgeNode->EdgeEdgeBandwidth;
		if ((EdgeNode->State & EDGEEDGERECEIVESTATE) == 0) {
			i = 1;
			CurrentList = FromEdgeNode->EdgeEdgeWaitingList;
			while (CurrentList) {
				i++;
				if ((CurrentList->ClientNode->VideoID == VideoID)
					|| (CurrentList->PieceID == PieceID)) {
					break;
				}
				i++;
				CurrentList = CurrentList->Next;
			}
			PredictedTime *= i;
		}
	}
	return EventTime + PredictedTime;
}
double PredictClientGetPieceTime(double EventTime, int VideoID, int PieceID, struct edgenode* EdgeNode, struct clientnode* ClientNode) {
	double PredictedTime;
	int i;
	struct clientnodelist* CurrentList;

	PredictedTime = PieceSize * 8.0 / EdgeNode->EdgeClientBandwidth;
	if ((ClientNode->State & EDGECLIENTRECEIVESTATE) == 0) {
		i = 1;
		CurrentList = EdgeNode->EdgeClientWaitingList;
		while (CurrentList) {
			i++;
			if ((CurrentList->ClientNode->VideoID == VideoID)
				|| (CurrentList->PieceID == PieceID)) {
				break;
			}
			CurrentList = CurrentList->Next;
		}
		PredictedTime *= i;
	}
	return EventTime + PredictedTime;
}
bool SearchReceivingWaiting(struct clientnode* ClientNode, int SearchPieceID) {
	int VideoID = ClientNode->VideoID;
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	struct clientnodelist* WaitingList;

	
	if (ConnectedEdgeNode->State & EDGEEDGERECEIVESTATE) {//1番近いエッジが他のエッジから受信中のpieceが探しているpiece
		if ((ConnectedEdgeNode->EdgeEdgeReceiveVideoID == VideoID)
			&& (ConnectedEdgeNode->EdgeEdgeReceivePieceID == SearchPieceID)) {
			return true;
		}
	}
	for(int i=0;i<NumEdges;i++){
		if(EdgeNodes[i].EdgeEdgeSendEdgeID==ConnectedEdgeNode->ID
			&&EdgeNodes[i].EdgeEdgeSendVideoID == VideoID
			&&EdgeNodes[i].EdgeEdgeSendPieceID == SearchPieceID){
			return true;
		}
	}

	for(int i=0;i<NumEdges;i++){
		if(EdgeNodes[i].EdgeEdgeSendEdgeID==ConnectedEdgeNode->ID
			&&EdgeNodes[i].EdgeEdgeSendVideoID == VideoID
			&&EdgeNodes[i].EdgeEdgeSendPieceID == SearchPieceID){
			return true;
		}

		WaitingList = EdgeNodes[i].EdgeEdgeWaitingList;//ClientNode->VideoEdgeNode->EdgeEdgeWaitingList;
		while (WaitingList != NULL) {//videoがキャッシュされるエッジがクラウドから受信中のpieceが探しているpiece
			if ((WaitingList->ClientNode->ConnectedEdgeNode==ConnectedEdgeNode)
				&& (WaitingList->ClientNode->VideoID == VideoID)
				&& (WaitingList->PieceID == SearchPieceID)) {
				return true;
			}
			WaitingList = WaitingList->Next;
		}
	}
	
	if (ConnectedEdgeNode->State & CLOUDEDGERECEIVESTATE) {//1番近いエッジがクラウドから受信中のpieceが探しているpiece
		if ((ConnectedEdgeNode->CloudEdgeReceiveVideoID == VideoID)
			&& (ConnectedEdgeNode->CloudEdgeReceivePieceID == SearchPieceID)) {
			return true;
		}
	}

	if(CloudNode.CloudEdgeSendEdgeID==ConnectedEdgeNode->ID
		&&CloudNode.CloudEdgeSendVideoID==VideoID
		&&CloudNode.CloudEdgeSendPieceID==SearchPieceID){
		return true;
	}
	
	WaitingList = CloudNode.CloudEdgeWaitingList;
	while(WaitingList != NULL) {//一番近いエッジに探しているpieceを送っているかどうか
		if ((WaitingList->ClientNode->ConnectedEdgeNode==ConnectedEdgeNode)
			&&(WaitingList->ClientNode->VideoID == VideoID)
			&& (WaitingList->PieceID == SearchPieceID)) {
			return true;
		}
		WaitingList = WaitingList->Next;
	}
	
	return false;
}
void CloudEdgeRequest(double EventTime, struct clientnode* ClientNode, int RequestPieceID, bool Stored) {
	double FinishTime;
	double r, Bandwidth;
	struct edgenode* EdgeNode = ClientNode->ConnectedEdgeNode;
	struct clientnodelist* WaitingList;
	struct clientnodelist* CurrentList;

	if (CloudNode.State & CLOUDEDGESENDSTATE) {//すでにどこかに送信中
		WaitingList = new struct clientnodelist;
		WaitingList->ClientNode = ClientNode;
		WaitingList->PieceID = RequestPieceID;
		WaitingList->Stored = Stored;
		WaitingList->Next = NULL;
		CurrentList = CloudNode.CloudEdgeWaitingList;
		if (CurrentList) {//waitinglistにリクエストを追加
			while (CurrentList->Next) {
				CurrentList = CurrentList->Next;
			}
			CurrentList->Next = WaitingList;
		}
		else {
			CloudNode.CloudEdgeWaitingList= WaitingList;
		}
	}
	else {
		CloudNode.State |= CLOUDEDGESENDSTATE;
		CloudNode.CloudEdgeSendPieceID = RequestPieceID;
		CloudNode.CloudEdgeSendVideoID = ClientNode->VideoID;
		CloudNode.CloudEdgeSendEdgeID = EdgeNode->ID;
		EdgeNode->State |= CLOUDEDGERECEIVESTATE;
		EdgeNode->CloudEdgeReceivePieceID = RequestPieceID;
		EdgeNode->CloudEdgeReceiveVideoID = ClientNode->VideoID;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (CloudNode.CloudEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		FinishTime = EventTime + 8.0 * PieceSize / Bandwidth;
		AddEvent(FinishTime, CLOUDEDGEFINISHEVENT, ClientNode, RequestPieceID, Stored);
	}
}
void EdgeEdgeRequest(double EventTime, struct clientnode* ClientNode, int RequestPieceID, bool Stored) {
	struct edgenode* ToEdgeNode = ClientNode->ConnectedEdgeNode;
	struct edgenode* FromEdgeNode = ClientNode->VideoEdgeNode;
	double FinishTime;
	double r, Bandwidth;
	struct clientnodelist* WaitingList;
	struct clientnodelist* CurrentList;

	if (FromEdgeNode->State & EDGEEDGESENDSTATE) {
		WaitingList = new struct clientnodelist;
		WaitingList->PieceID = RequestPieceID;
		WaitingList->Stored = Stored;
		WaitingList->ClientNode = ClientNode;
		WaitingList->Next = NULL;
		CurrentList = FromEdgeNode->EdgeEdgeWaitingList;
		if (CurrentList) {
			while (CurrentList->Next) {
				CurrentList = CurrentList->Next;
			}
			CurrentList->Next = WaitingList;
		}
		else {
			FromEdgeNode->EdgeEdgeWaitingList = WaitingList;
		}
	}
	else {
		FromEdgeNode->State |= EDGEEDGESENDSTATE;
		FromEdgeNode->EdgeEdgeSendPieceID = RequestPieceID;
		FromEdgeNode->EdgeEdgeSendVideoID = ClientNode->VideoID;
		FromEdgeNode->EdgeEdgeSendEdgeID = ToEdgeNode->ID;
		ToEdgeNode->State |= EDGEEDGERECEIVESTATE;
		ToEdgeNode->EdgeEdgeReceivePieceID = RequestPieceID;
		ToEdgeNode->EdgeEdgeReceiveVideoID = ClientNode->VideoID;
		ToEdgeNode->EdgeEdgeReceiveEdgeNode = FromEdgeNode;

		r = (double)rand() / RAND_MAX;
		Bandwidth = (FromEdgeNode->EdgeEdgeBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		FinishTime = EventTime + 8.0 * PieceSize / Bandwidth;
		AddEvent(FinishTime, EDGEEDGEFINISHEVENT, ClientNode, RequestPieceID, Stored);
	}
}

void EdgeClientRequest(double EventTime, struct clientnode* ClientNode,bool Cached) {
	struct edgenode* EdgeNode = ClientNode->ConnectedEdgeNode;
	int RequestPieceID = ClientNode->EdgeClientReceivedPieceID + 1;
	double FinishTime;
	double r, Bandwidth;
	struct clientnodelist* WaitingList;
	struct clientnodelist* CurrentList;

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
		if (CurrentList) {
			while (CurrentList->Next) {
				CurrentList = CurrentList->Next;
			}
			CurrentList->Next = WaitingList;//最後に今回のクライアントを追加
		}
		else {
			EdgeNode->EdgeClientWaitingList = WaitingList;
		}
	}
	else {//待ちがいない
		EdgeNode->State |= EDGECLIENTSENDSTATE;//edgeがpieceをsendする
		ClientNode->State |= EDGECLIENTRECEIVESTATE;//clientがpieceをreceiveする

		r = (double)rand() / RAND_MAX;//帯域幅の揺れ
		Bandwidth = (EdgeNode->EdgeClientBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		FinishTime = EventTime + 8.0 * PieceSize / Bandwidth;//エッジからvideoをもらうのにかかる時間
		AddEvent(FinishTime, EDGECLIENTFINISHEVENT, ClientNode, RequestPieceID, Cached);//Cached falseだと一番近いエッジにpieceが保存されていない
	}
}
void ExecuteEdgeClientFetchEvent(double EventTime, struct clientnode* ClientNode) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int ReceiveEdgeNodeID, VideoID = ClientNode->VideoID, ReceivePieceID=ClientNode->EdgeClientReceivedPieceID+1,EdgeNodeID = ConnectedEdgeNode->ID,SearchPieceID;
	int HotCachePosition;
	double OverheadTime=0.0;
	bool Hit;
	int decidedNode;//-1ならcloud,1ならedgeからpieceを取得
	bool Stored = false;
	numOfExsistPieceID = 0;

	/*for (ReceiveEdgeNodeID = 0; ReceiveEdgeNodeID < NumEdges; ReceiveEdgeNodeID++) {
		if (VideoID < EdgeNodes[ReceiveEdgeNodeID].StartVideoID)
			break;
	}
	ReceiveEdgeNodeID--;//videoがあるedgeを決定*/
	ClientNode->VideoEdgeNode = &(EdgeNodes[ConnectedEdgeNode->ID]);//videoをとってくるedge

	if(ClientNode->ID==316){
		int stopGG=354;
	}

	Hit = VoteHotCache(ClientNode,true);//Reserveの可能性がある

	if (Hit == false) {
		if ((ConnectedEdgeNode == ClientNode->VideoEdgeNode)//一番近いedgeが見ているvideoを保存するedge
			&& (numOfExsistPieceID == NumPieces)) {//前半何個か分だけ保存
			printf("error:not hit but have all pieces");
			ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
			EdgeClientRequest(EventTime, ClientNode, false);
		}
		
	}
	else {//一番近いedgeにpieceがある
		if(numOfExsistPieceID == NumPieces){//全てのpieceを一番近いedgeが持っている
			//(ConnectedEdgeNode->HotCache[ClientNode->EdgeClientSearchedHotCachePosition].Voted)--;
			CloudServerRequest(EventTime, ClientNode, VideoID, ReceivePieceID);
			ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
			CloudServer.EdgeDiskIORead[ConnectedEdgeNode->ID] += PieceSize;
			CloudServer.EdgeNetworkIORead[ConnectedEdgeNode->ID] += PieceSize;
			numOfExsistPieceID=0;
			//ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[0]]);
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
				if(IsStoreHotCache(ClientNode, ReceivePieceID)) {
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
				
				EdgeEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivePieceID, Stored);
				return;
			}
			
		}
		else if(ConnectedEdgeNode->HotCache[HotCachePosition].PieceID<NumPieces-1){//途中からない　実行されない！
			printf("Error edge request");
		}
		return;
	}

	//Cloudプリキャッシュ
	
	HotCachePosition= ClientNode->CloudEdgeSearchedHotCachePosition;
	if (HotCachePosition == -1 && decidedNode == -1) {//最初がない
		
		if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {//マージできない そのpieceを要求しているリクエストがまだない
			if(IsStoreHotCache(ClientNode, ReceivePieceID)) {
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

void CheckHotCache(struct clientnode* ClientNode) {
	int i,j;
	struct edgenode* EdgeNode = ClientNode->ConnectedEdgeNode;
	for (i = 0; i < HotCacheNumPieces; i++) {
		for (j = i + 1; j < HotCacheNumPieces; j++) {
			if ((EdgeNode->HotCache[i].VideoID != -1)
				&&(EdgeNode->HotCache[i].VideoID == EdgeNode->HotCache[j].VideoID)
				&& (EdgeNode->HotCache[i].PieceID == EdgeNode->HotCache[j].PieceID)) {
				printf("Error DuplicateStore\n");//2つ同じpieceが保存されている
			}
		}

	}
}
int StoreHotCache(struct clientnode* ClientNode, int StorePieceID) {
	int VideoID = ClientNode->VideoID;
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int HotCacheStart= ConnectedEdgeNode->HotCacheStart, HotCacheEnd= ConnectedEdgeNode->HotCacheEnd,HotCacheUsed;
	int CurrentHotCachePosition,DeleteHotCachePosition;
	struct clientnodelist* ClientList;
	struct clientnode* CurrentClientNode;
	int VirtualCurrentPosition,VirtualHotCacheEnd,VirtualDeleteHotCachePosition;

	if (HotCacheNumPieces == 0) {
		return -1;
	}

	if (HotCacheStart == -1) {//初
		HotCacheStart = 0;
		HotCacheUsed = 0;
	}
	else {
		if (HotCacheStart <= HotCacheEnd)
			HotCacheUsed = HotCacheEnd - HotCacheStart + 1;
		else
			HotCacheUsed = HotCacheNumPieces - HotCacheStart + HotCacheEnd + 1;//使用されている数
	}

	if(HotCacheUsed< HotCacheNumPieces){//空きあるor初
		if (HotCacheEnd == HotCacheNumPieces - 1)
			HotCacheEnd = 0;
		else
			HotCacheEnd++;//endの増加　つまり保存
	}
	else {//空き無し
		DeleteHotCachePosition = GetDeleteHotCachePosition(ClientNode,StorePieceID);
		
		if (DeleteHotCachePosition!=-1){
			//詰める
			CloudServer.ExsistPiece[ConnectedEdgeNode->ID][ConnectedEdgeNode->HotCache[DeleteHotCachePosition].VideoID][ConnectedEdgeNode->HotCache[DeleteHotCachePosition].PieceID] = -1;

			CurrentHotCachePosition = DeleteHotCachePosition;//誰も見ていないpiece
			//ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID;
			//ConnectedEdgeNode->HotCache[CurrentHotCachePosition].PieceID;
			//ClientNode->ConnectedEdgeID;
			while (CurrentHotCachePosition != HotCacheStart) {//前に戻っていきCurrentHotCachePositionをstartにする
				if (CurrentHotCachePosition == 0) {//全て一つずらす
					ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID = ConnectedEdgeNode->HotCache[HotCacheNumPieces - 1].VideoID;
					ConnectedEdgeNode->HotCache[CurrentHotCachePosition].PieceID = ConnectedEdgeNode->HotCache[HotCacheNumPieces - 1].PieceID;
					ConnectedEdgeNode->HotCache[CurrentHotCachePosition].Voted = ConnectedEdgeNode->HotCache[HotCacheNumPieces - 1].Voted;
				}
				else {
					ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID = ConnectedEdgeNode->HotCache[CurrentHotCachePosition - 1].VideoID;
					ConnectedEdgeNode->HotCache[CurrentHotCachePosition].PieceID = ConnectedEdgeNode->HotCache[CurrentHotCachePosition - 1].PieceID;
					ConnectedEdgeNode->HotCache[CurrentHotCachePosition].Voted = ConnectedEdgeNode->HotCache[CurrentHotCachePosition - 1].Voted;
				}
				if (CurrentHotCachePosition == 0)
					CurrentHotCachePosition = HotCacheNumPieces - 1;
				else
					CurrentHotCachePosition--;
			}
			//補正も
			if (HotCacheEnd < HotCacheStart)
				VirtualHotCacheEnd = HotCacheNumPieces + HotCacheEnd;//事実上の終わり地点
			else
				VirtualHotCacheEnd = HotCacheEnd;
			if (DeleteHotCachePosition<HotCacheStart)
				VirtualDeleteHotCachePosition = HotCacheNumPieces + DeleteHotCachePosition;//事実上の削除地点
			else
				VirtualDeleteHotCachePosition =DeleteHotCachePosition;
			ClientList = ConnectedEdgeNode->OnClientList;
			while (ClientList != NULL) {//キャッシュの中に自分がこれから使うデータがあれば、消さないようにvoteしている 無駄な削除を省いている
				CurrentClientNode = ClientList->ClientNode;
				VirtualCurrentPosition =  CurrentClientNode->VotedHotCachePosition;//voteずみの場所　おそらくend
				if (VirtualCurrentPosition != -1) {
					if (VirtualCurrentPosition < HotCacheStart)
						VirtualCurrentPosition += HotCacheNumPieces ;
					if (VirtualCurrentPosition < VirtualDeleteHotCachePosition) {
						if (HotCacheNumPieces <= VirtualCurrentPosition)
							VirtualCurrentPosition -= HotCacheNumPieces;
						if (VirtualCurrentPosition == HotCacheNumPieces - 1)
							CurrentClientNode->VotedHotCachePosition = 0;
						else
							CurrentClientNode->VotedHotCachePosition++;
					}
					else if (VirtualCurrentPosition == HotCacheStart) {
						CurrentClientNode->VotedHotCachePosition = -1;
					}
				}//edge
				VirtualCurrentPosition = CurrentClientNode->EdgeClientSearchedHotCachePosition;
				if (VirtualCurrentPosition != -1) {//常にある部分
					if (VirtualCurrentPosition < HotCacheStart)
						VirtualCurrentPosition += HotCacheNumPieces;
					if (VirtualCurrentPosition < VirtualDeleteHotCachePosition) {
						if (HotCacheNumPieces <= VirtualCurrentPosition)
							VirtualCurrentPosition -= HotCacheNumPieces;
						if (VirtualCurrentPosition == HotCacheNumPieces - 1)
							CurrentClientNode->EdgeClientSearchedHotCachePosition = 0;
						else
							CurrentClientNode->EdgeClientSearchedHotCachePosition++;
					}
					else if (VirtualCurrentPosition == HotCacheStart) {
						CurrentClientNode->EdgeClientSearchedHotCachePosition = -1;
					}
				}//edge edge
				VirtualCurrentPosition = CurrentClientNode->EdgeEdgeSearchedHotCachePosition;
				if (VirtualCurrentPosition != -1) {//numPrePieces以降で存在する
					if (VirtualCurrentPosition < HotCacheStart)
						VirtualCurrentPosition += HotCacheNumPieces ;
					if (VirtualCurrentPosition < VirtualDeleteHotCachePosition) {
						if (HotCacheNumPieces <= VirtualCurrentPosition)
							VirtualCurrentPosition -= HotCacheNumPieces;
						if (VirtualCurrentPosition == HotCacheNumPieces - 1)
							CurrentClientNode->EdgeEdgeSearchedHotCachePosition = 0;
						else
							CurrentClientNode->EdgeEdgeSearchedHotCachePosition++;
					}
					else if (VirtualCurrentPosition == HotCacheStart) {
						CurrentClientNode->EdgeEdgeSearchedHotCachePosition = -1;
					}
				}//cloud edge
				VirtualCurrentPosition =  CurrentClientNode->CloudEdgeSearchedHotCachePosition;
				if (VirtualCurrentPosition != -1) {//保存されていない
					if (VirtualCurrentPosition < HotCacheStart)
						VirtualCurrentPosition += CurrentClientNode->CloudEdgeSearchedHotCachePosition;
					if (VirtualCurrentPosition < VirtualDeleteHotCachePosition) {
						if (HotCacheNumPieces <= VirtualCurrentPosition)
							VirtualCurrentPosition -= HotCacheNumPieces;
						if (VirtualCurrentPosition == HotCacheNumPieces - 1)
							CurrentClientNode->CloudEdgeSearchedHotCachePosition = 0;
						else
							CurrentClientNode->CloudEdgeSearchedHotCachePosition++;
					}
					else if (VirtualCurrentPosition == HotCacheStart) {
						CurrentClientNode->CloudEdgeSearchedHotCachePosition= -1;
					}
				}
				ClientList = ClientList->Next;
			}

			if (HotCacheStart == HotCacheNumPieces-1)
				HotCacheStart = 0;
			else
				HotCacheStart++;
			if (HotCacheEnd == HotCacheNumPieces - 1)
				HotCacheEnd = 0;
			else
				HotCacheEnd++;
		}
		else {
			return -1;
		}

	}
	
	ConnectedEdgeNode->HotCache[HotCacheEnd].VideoID = VideoID;
	ConnectedEdgeNode->HotCache[HotCacheEnd].PieceID = StorePieceID;
	ConnectedEdgeNode->HotCache[HotCacheEnd].Voted = -1;
	ConnectedEdgeNode->HotCacheStart = HotCacheStart;
	ConnectedEdgeNode->HotCacheEnd = HotCacheEnd;

	CloudServer.ExsistPiece[ConnectedEdgeNode->ID][VideoID][StorePieceID] = 1;

	CheckHotCache(ClientNode);
	return HotCacheEnd;

}

void ExecuteEdgeClientFinishEvent(double EventTime, struct clientnode* ClientNode, int ReceivedPieceID, bool Cached) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	double PlayPosition, TimePlayPosition;
	struct interrupt* CurrentInterrupt;
	bool Hit;
	double OverheadTime = 0.0;
	int ReceivePieceID;
	bool Store = false;

	if(ClientNode->ID==136&&ReceivedPieceID==9){
		int edgeclientGet=0;
	}

	ConnectedEdgeNode->State &= (~EDGECLIENTSENDSTATE);
	ClientNode->State &= (~EDGECLIENTRECEIVESTATE);

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

	EdgeClientWaiting(EventTime, ConnectedEdgeNode);//待ちの次のクライアントを実行

	if (ClientNode->EdgeClientReceivedPieceID + 1 == ReceivedPieceID) {
		ClientNode->EdgeClientReceivedPieceID++;
	}
	else {
		printf("Error EdgeClientReceivedPieceID\n");
	}

	if (ReceivedPieceID == NumPieces - 1) {
		ClientFinishReception(EventTime, ClientNode);
	}
	else {
		ReceivePieceID = ClientNode->EdgeClientReceivedPieceID + 1;
		VoteHotCache(ClientNode, false);
		Hit = SearchHotCache(ClientNode, ReceivePieceID, &(ClientNode->EdgeClientSearchedHotCachePosition), false);
		if (Hit == false) {
			
			/*if (ConnectedEdgeNode == ClientNode->VideoEdgeNode) {//実行されない
			//if (ClientNode->ConnectedEdgeNode->ID == ClientNode->VideoRequestsID[ReceivePieceID]) {
				printf("error miss:piece get but not exsit");
				//ClientNode->VideoEdgeNode = &(EdgeNodes[ClientNode->VideoRequestsID[ReceivePieceID]]);
				if (ReceivePieceID < NumPrePieces) {
					ConnectedEdgeNode->EdgeClientReadBytes += PieceSize;
					EdgeClientRequest(EventTime, ClientNode, false);
				}
				else {
					if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
						OverheadTime += 64.0 * 8.0 / CloudNode.CloudEdgeBandwidth;
						CloudNode.CloudEdgeReadBytes += PieceSize;
						CloudEdgeRequest(EventTime, ClientNode, ReceivePieceID);
					}
				}
			}
			else */
			if (ClientNode->VideoRequestsID[ReceivePieceID] != NumEdges){//他のエッジから次のpieceを取得
				
				if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
					
					if(IsStoreHotCache(ClientNode, ReceivePieceID)) {
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

					EdgeEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
				}
			}
			else if(ClientNode->VideoRequestsID[ReceivePieceID] == NumEdges){//クラウドから次のpieceを取得
				
				if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
					
					if(IsStoreHotCache(ClientNode, ReceivePieceID)) {
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
			EdgeClientRequest(EventTime, ClientNode, false);
		}
	}
}
void ExecuteEdgeEdgeFinishEvent(double EventTime, struct clientnode* ClientNode,int ReceivedPieceID, bool Stored) {
	struct edgenode* ToEdgeNode = ClientNode->ConnectedEdgeNode;
	struct edgenode* FromEdgeNode = ClientNode->VideoEdgeNode;
	struct clientnode* CurrentClientNode;
	struct clientnodelist* OnClientNodeList;
	int VideoID= ClientNode->VideoID,ReceivePieceID,StoredHotCachePosition;
	double OverheadTime;
	bool Direct=false;

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
	if(Stored){
		StoredHotCachePosition = StoreHotCache(ClientNode, ReceivedPieceID);
		if(StoredHotCachePosition == -1){
			 printf("Edge Store Error");
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
	
	EdgeEdgeWaiting(EventTime, FromEdgeNode);
	//OverheadTime = 64.0 * 8.0 / FromEdgeNode->EdgeEdgeBandwidth;
	//Direct
	if ((Stored||Direct||IsStoreHotCache(ClientNode, ReceivedPieceID)) && ReceivedPieceID != NumPieces - 1) {
		bool Store = false;
		if (SearchHotCache(ClientNode, ReceivedPieceID + 1, &(ClientNode->EdgeEdgeSearchedHotCachePosition), true)) {
			ReceivedPieceID = ToEdgeNode->HotCache[ClientNode->EdgeEdgeSearchedHotCachePosition].PieceID;
		}
		ReceivePieceID = ReceivedPieceID + 1;
		if(ReceivePieceID == NumPieces) return;
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
				
				if(IsStoreHotCache(ClientNode, ReceivePieceID)) {//意味がない　次はedgeClient通信の後に実行されるため実行しない？
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

				EdgeEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		else if(ClientNode->VideoRequestsID[ReceivePieceID] == NumEdges){//クラウドから次のpieceを取得
			
			if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
				
				if(IsStoreHotCache(ClientNode, ReceivePieceID)) {//意味がない　次はedgeClient通信の後に実行されるため実行しない？
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
				
				CloudEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		/*if ((ReceivePieceID < NumPrePieces )
			&&(SearchReceivingWaiting(ClientNode, ReceivePieceID) == false)) {//マージできない
			FromEdgeNode->EdgeEdgeReadBytes += PieceSize;
			EdgeEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivePieceID );
		}*/
	}
	else {
		//Post先取で保存できない、誰も受信していないとき
		//FromEdgeNode->EdgeEdgeReadBytes += PieceSize;
		//EdgeEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivedPieceID);
	}
}
void ExecuteCloudEdgeFinishEvent(double EventTime, struct clientnode* ClientNode, int ReceivedPieceID, bool Stored) {
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	struct clientnode* CurrentClientNode;
	struct clientnodelist* OnClientNodeList;
	int VideoID = ClientNode->VideoID,ReceivePieceID,StoredHotCachePosition;
	bool Direct=false;
	double OverheadTime;

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

	if(ClientNode->ID==5&&ReceivedPieceID==75){
		int cloudGet=0;
	}

	if(Stored){
		StoredHotCachePosition = StoreHotCache(ClientNode, ReceivedPieceID);
		if(StoredHotCachePosition == -1) {
			printf("Cloud Store Error");
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


	CloudEdgeWaiting(EventTime);

	//OverheadTime = 64.0 * 8.0 / CloudNode.CloudEdgeBandwidth;
	//Direct
	if ((Stored||Direct||IsStoreHotCache(ClientNode, ReceivedPieceID)) && ReceivedPieceID != NumPieces - 1) {
		bool Store = false;
		if (SearchHotCache(ClientNode, ReceivedPieceID + 1, &(ClientNode->CloudEdgeSearchedHotCachePosition), true)) {//すでに次のpieceがキャッシュされているか確認
			ReceivedPieceID = ConnectedEdgeNode->HotCache[ClientNode->CloudEdgeSearchedHotCachePosition].PieceID;
		}
		ReceivePieceID = ReceivedPieceID + 1;  //ReceivedではなくReceive   次にどこを取ってくるか決定
		if(ReceivePieceID == NumPieces) return;
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
				
				if(IsStoreHotCache(ClientNode, ReceivePieceID)) {//falseだと意味がない　次はedgeClient通信の後に実行されるため実行しない？
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
				
				EdgeEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		else if(ClientNode->VideoRequestsID[ReceivePieceID] == NumEdges){//クラウドから次のpieceを取得
			
			if (SearchReceivingWaiting(ClientNode, ReceivePieceID) == false) {
				
				if(IsStoreHotCache(ClientNode, ReceivePieceID)) {//意味がない　次はedgeClient通信の後に実行されるため実行しない？
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

				CloudEdgeRequest(EventTime, ClientNode, ReceivePieceID, Store);
			}
		}
		/*if ((ReceivePieceID  < NumPieces)
			&&(SearchReceivingWaiting(ClientNode, ReceivePieceID) == false)) {//マージできない
			CloudNode.CloudEdgeReadBytes += PieceSize;
			CloudEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivePieceID);
		}*/
	}
	else {
		//Post先取で保存できない、誰も受信していないとき
		//CloudNode.CloudEdgeReadBytes += PieceSize;
		//CloudEdgeRequest(EventTime + OverheadTime, ClientNode, ReceivedPieceID);
	}

}

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

void EventParser() {
	if (10<LastSumInterruptDuration)
		LastSumInterruptDuration = LastSumInterruptDuration ;
	if (TopEvent->Time > 18050)
		TopEvent->Time = TopEvent->Time;
	if (TopEvent->ClientNode->ID == 1857)
		TopEvent->ClientNode->ID = TopEvent->ClientNode->ID;
	if ((Duration < TopEvent->Time) && (Duration < TopEvent->ClientNode->SumInterruptDuration)) {
		printf("Warning LongWaiting\n");
	}
	switch (TopEvent->EventID) {
	case CLIENTONEVENT:
		printf("OnTime:%lf\tID%d\tReciving%d\tReceived%d\tLast:%lf\tAve:%lf\tMax:%lf\n", TopEvent->Time, ((clientnode*)(TopEvent->ClientNode))->ID, NumReceivingClients, NumReceivedClients, LastSumInterruptDuration,AverageInterruptDuration / NumReceivedClients,MaximumInterruptDuration);//ClientIntterupt　最後　全ての平均　最大
		ExecuteClientOnEvent(TopEvent->Time, (clientnode*)(TopEvent->ClientNode));
		break;
	case EDGECLIENTFETCHEVENT:
		ExecuteEdgeClientFetchEvent(TopEvent->Time, (clientnode*)(TopEvent->ClientNode));
		break;
	case EDGECLIENTFINISHEVENT:
		ExecuteEdgeClientFinishEvent(TopEvent->Time, (clientnode*)(TopEvent->ClientNode),TopEvent->Data1,(TopEvent->Data2)==1);
		break;
	case EDGEEDGEFINISHEVENT:
		ExecuteEdgeEdgeFinishEvent(TopEvent->Time, (clientnode*)(TopEvent->ClientNode), TopEvent->Data1,(TopEvent->Data2)==1);
		break;
	case CLOUDEDGEFINISHEVENT:
		ExecuteCloudEdgeFinishEvent(TopEvent->Time, (clientnode*)(TopEvent->ClientNode), TopEvent->Data1,(TopEvent->Data2)==1);
		break;
		/*
	case RETRYEVENT:
		ExecuteRetryEvent();
		break;
	case FINISHREQUESTEVENT:
		ExecuteFinishRequestEvent();
		break;
*/
	}
}
void CalculateZipfVideo() {
	int n,k;
	double Hks, HNs;
	double s=0.7;

	HNs = 0.0;
	for (n = 1; n <= NumVideos; n++) {
		HNs += 1.0 / pow(n, s);
	}

	Hks = 0.0;
	for (k = 1; k <= NumVideos; k++) {
		Hks += 1.0 / pow(k, s);
		ZipfVideo[k - 1] = Hks / HNs;
	}
}
int GetZipfVideoID() {
	int i;
	double r = (double)rand() / RAND_MAX;

	for (i = 0; i < NumVideos; i++) {
		if (r <= ZipfVideo[i] + 1.0E-8)
			return i;
	}
	return NumVideos - 1;
}
void Initialize() {
	printf("Initializing...");

	srand(Seed);

	NumReceivingClients = 0;//Onのノード数
	NumReceivedClients = 0;//全部受信してOnのノード数
	AverageNumInterrupt = 0;
	AverageInterruptDuration = 0.0;
	MaximumInterruptDuration = 0.0;
	MinimumInterruptDuration = 1.0e32;

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
	CloudNode.CloudEdgeSendPieceID = -1;
	CloudNode.CloudEdgeSendVideoID = -1;
	CloudNode.CloudEdgeSendEdgeID = -1;

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
	}
	CloudServer.CloudDiskIORead=0;
	CloudServer.CloudNetworkIORead=0;
	CloudServer.CloudPowerConsumption=0;
	CloudServer.CloudResponceTime=0;
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
			EdgeNodes[i].HotCache[j].VideoID = -1;
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
		EdgeNodes[i].EdgeEdgeReceiveEdgeNode=NULL;
		EdgeNodes[i].EdgeEdgeSendPieceID = -1;
		EdgeNodes[i].EdgeEdgeSendVideoID = -1;
		EdgeNodes[i].EdgeEdgeSendEdgeID = -1;
		EdgeNodes[i].NumReceiving = 0;
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

void Simulate() {
	struct event* CurrentEvent;

	while (TopEvent->Time < SimulationTime) {
#ifdef LOG
		fprintf(LogFile, "time%lf\tevent%d\teventID%d\tclientID%d\tData1,%d\tData2,%d\tn_edge%d\tvideoID%d\n", TopEvent->Time, TopEvent->EventNum, TopEvent->EventID, TopEvent->ClientNode->ID, TopEvent->Data1, TopEvent->Data2,TopEvent->ClientNode->ConnectedEdgeID,TopEvent->ClientNode->VideoID);
		fflush(LogFile);
#endif
		EventParser();
		/*printf("cloud %f %f\n",CloudServer.CloudDiskIORead,CloudServer.CloudNetworkIORead);
		if(CloudServer.CloudDiskIORead<0||CloudServer.CloudNetworkIORead<0){
			int cloudServerGets=112;
		}
		for(int i=0;i<NumEdges;i++){
			printf("%d %f %f %f %f \n",i,CloudServer.EdgeDiskIORead[i],CloudServer.EdgeNetworkIORead[i],CloudServer.EdgeDiskIOWrite[i],CloudServer.EdgeNetworkIOWrite[i]);
			if(CloudServer.EdgeDiskIORead[i] <0 || CloudServer.EdgeNetworkIORead[i] <0 || CloudServer.EdgeDiskIOWrite[i]<0 || CloudServer.EdgeNetworkIOWrite[i]<0 ||CloudServer.EdgeDiskIOWrite[i]>CloudServer.EdgeNetworkIOWrite[i]){
				int cloudServerGet=111;
			}
		}*/
		CurrentEvent = TopEvent;
		TopEvent = TopEvent->Next;
		delete CurrentEvent;
		if (TopEvent == NULL)
			break;
	}
	if (NumReceivedClients == 0) {
		AverageInterruptDuration = 0;
		AverageNumInterrupt = 0;
	}
	else {
		AverageInterruptDuration = AverageInterruptDuration / NumReceivedClients;
		AverageNumInterrupt = AverageNumInterrupt / NumReceivedClients;
	}
}

void EvaluateLambda() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt, EdgeVolume;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth =  1000000000.0;//1Gbps
	EdgeEdgeBandwidth =   1000000000.0;//1Gbps
	EdgeClientBandwidth = 1000000000.0;//100Mbps

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024    5M
	Duration = 30 * 60.0;//視聴時間 30*60
	SegmentTime = 10.0;
	PieceSize = (int)(SegmentTime*BitRate / 8);//5秒
	NumPrePieces = 0;//下で変えてる  360piecesh
	SimulationTime = 10.0 * 60 * 60;//5*60*60
	BandwidthWaver = 0.0;
	HotCacheNumPieces = 15000000000 / PieceSize;//100MB 1GB　おそらく合計8GB? 320pieces = 320*5*bitRate bit = 1GByte
	//HotCacheNumPieces = 0;
	NumEdges = 8;//8
	NumVideos = 100;//900Gb 112.5GB

	sprintf(FileName, "EvaluateLambda.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 1; i <= 1; i++) {
		fprintf(ResultFile, "%d pieces\tEdgeBoost (10 pieces)\t\t\t\t\t\t\t\tEdgeBoost (20 pieces)\t\t\t\t\t\t\t\tEdgeBoost (30 pieces)\t\t\t\t\t\t\t\tNo EdgeBoost\n", HotCacheNumPieces);
		n = 2;//行数
		for (j = 1; j <= 1; j++) {//15
			if (j == 0)AverageArrivalInterval = 12;//12
			else AverageArrivalInterval = j ;//j
			MinAveInterrupt = 1.0e32;
			//fprintf(ResultFile, "%lf\t\n", AverageArrivalInterval);
			for (l = 100; l <= 100; l++) {//3
				if (l >= 0) HotCacheNumPieces = (double)l*1000000000/PieceSize; //NumPrePieces = (l + 1) * 10;
				else NumPrePieces = 0;
				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k;
					Initialize();
					InitializeCloudNode(CloudEdgeBandwidth);
					InitializeEdgeNodes(EdgeEdgeBandwidth, EdgeClientBandwidth);
					InitializeClientNodes();

					Simulate();
					AveInterruptDuration += AverageInterruptDuration;
					AveNumInterrupt += AverageNumInterrupt;
					if (MaxInterrupt < MaximumInterruptDuration)
						MaxInterrupt = MaximumInterruptDuration;
					if (MinInterrupt > MinimumInterruptDuration)
						MinInterrupt = MinimumInterruptDuration;
					Finalize();
					//DistributionMethod +=1;
				}
				AveInterruptDuration /= NSIM;
				AveNumInterrupt /= NSIM;
				EdgeVolume = (double)HotCacheNumPieces*PieceSize;
				fprintf(ResultFile, "%lf\t%.0lf\t%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t\n",AverageArrivalInterval,EdgeVolume, AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes,TotalEdgeEdgeWriteBytes,TotalEdgeEdgeReadBytes,TotalCloudEdgeWriteBytes,TotalCloudEdgeReadBytes);
				fflush(ResultFile);

			}
			fprintf(ResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
}
void EvaluateCloudEdgeBandwidth() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth = 0.0;
	EdgeEdgeBandwidth = 1000000000000.0;
	EdgeClientBandwidth = 100000000.0;

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 30 * 60.0;
	PieceSize = (int)(5.0 * BitRate / 8);//5秒
	NumPrePieces = 999;
	SimulationTime = 6.0 * 60 * 60;
	BandwidthWaver = 0.0;
	//HotCacheNumPieces = 1000000000 / PieceSize;
	HotCacheNumPieces = 0;
	NumEdges = 8;
	NumVideos = 4000;

	sprintf(FileName, "EvaluateCloudEdgeBandwidth.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 10; i <= 15; i++) {
		if (i == 0)AverageArrivalInterval = 1;
		else AverageArrivalInterval = (i - 1.0) * 2;
		fprintf(ResultFile, "%d pieces\tEdgeBoost (10 pieces)\t\t\t\t\t\t\t\tEdgeBoost (20 pieces)\t\t\t\t\t\t\t\tEdgeBoost (30 pieces)\t\t\t\t\t\t\t\tNo EdgeBoost\n", HotCacheNumPieces);
		n = 2;//行数
		for (j = 0; j <= 20; j++) {
			if (j == 0)CloudEdgeBandwidth = 1000000000.0;//1.6G
			else CloudEdgeBandwidth=100000000.0* (j + 1);
			MinAveInterrupt = 1.0e32;
			fprintf(ResultFile, "%lf\t", CloudEdgeBandwidth/1000000.0);
			for (l = 3; l <= 3; l++) {
				if (l < 3)NumPrePieces = (l + 1) * 10;
				else NumPrePieces = 0;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k;
					Initialize();
					InitializeCloudNode(CloudEdgeBandwidth);
					InitializeEdgeNodes(EdgeEdgeBandwidth, EdgeClientBandwidth);
					InitializeClientNodes();

					Simulate();
					AveInterruptDuration += AverageInterruptDuration;
					AveNumInterrupt += AverageNumInterrupt;
					if (MaxInterrupt < MaximumInterruptDuration)
						MaxInterrupt = MaximumInterruptDuration;
					if (MinInterrupt > MinimumInterruptDuration)
						MinInterrupt = MinimumInterruptDuration;
					Finalize();
				}
				AveInterruptDuration /= NSIM;
				AveNumInterrupt /= NSIM;
				fprintf(ResultFile, "%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t", AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes, TotalEdgeEdgeWriteBytes, TotalEdgeEdgeReadBytes, TotalCloudEdgeWriteBytes, TotalCloudEdgeReadBytes);
				fflush(ResultFile);

			}
			fprintf(ResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
}


void EvaluateNumPrePieces() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth = 1600000000.0;
	EdgeEdgeBandwidth =  1000000.0;
	EdgeClientBandwidth = 100000000.0;

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 30 * 60.0;
	PieceSize = (int)(5.0 * BitRate / 8);//5秒, 3.125Mバイト@5Mbps, 360個で1.125Gバイト, 800個で2.5Gバイト
	NumPrePieces = 999;
	SimulationTime = 24.0 * 60 * 60;
	BandwidthWaver = 0.0;
	//HotCacheNumPieces = 1000000000 / PieceSize;
	HotCacheNumPieces = 0;
	NumEdges = 8;
	NumVideos = 4000;

	sprintf(FileName, "EvaluateNumPrePieces.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 7; i <= 7; i++) {
		if (i == 0)AverageArrivalInterval = 1;
		else AverageArrivalInterval = (i - 1.0) * 2;
		fprintf(ResultFile, "%d pieces\tEdgeBoost (10 pieces)\t\t\t\t\t\t\t\tEdgeBoost (20 pieces)\t\t\t\t\t\t\t\tEdgeBoost (30 pieces)\t\t\t\t\t\t\t\tNo EdgeBoost\n", HotCacheNumPieces);
		n = 2;//行数
		for (j = 0; j <= 36; j++) {
			if (j == 0)NumPrePieces=10;
			else NumPrePieces = j*10;
			MinAveInterrupt = 1.0e32;
			fprintf(ResultFile, "%d\t", NumPrePieces);
			for (l = 0; l <= 0; l++) {
				//if (l < 3)NumPrePieces = (l + 1) * 10;
				//else NumPrePieces = 0;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k;
					Initialize();
					InitializeCloudNode(CloudEdgeBandwidth);
					InitializeEdgeNodes(EdgeEdgeBandwidth, EdgeClientBandwidth);
					InitializeClientNodes();

					Simulate();
					AveInterruptDuration += AverageInterruptDuration;
					AveNumInterrupt += AverageNumInterrupt;
					if (MaxInterrupt < MaximumInterruptDuration)
						MaxInterrupt = MaximumInterruptDuration;
					if (MinInterrupt > MinimumInterruptDuration)
						MinInterrupt = MinimumInterruptDuration;
					Finalize();
				}
				AveInterruptDuration /= NSIM;
				AveNumInterrupt /= NSIM;
				fprintf(ResultFile, "%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t", AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes, TotalEdgeEdgeWriteBytes, TotalEdgeEdgeReadBytes, TotalCloudEdgeWriteBytes, TotalCloudEdgeReadBytes);
				fflush(ResultFile);

			}
			fprintf(ResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
}
void EvaluateHotCacheNumPieces() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth = 600000000.0;
	EdgeEdgeBandwidth = 1000000000000.0;
	EdgeClientBandwidth = 100000000.0;

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 30 * 60.0;
	PieceSize = (int)(5.0 * BitRate / 8);//5秒
	NumPrePieces = 50;
	SimulationTime = 48.0 * 60 * 60;
	BandwidthWaver = 0.0;
	//HotCacheNumPieces = 1000000000 / PieceSize;
	HotCacheNumPieces = 0;//下で変える
	NumEdges = 8;
	NumVideos = 99999;

	sprintf(FileName, "EvaluateHotCacheNumPieces.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 7; i <= 7; i++) {
		if (i == 0)AverageArrivalInterval = 1;
		else AverageArrivalInterval = (i - 1.0) * 2;
		fprintf(ResultFile, "%d pieces\tEdgeBoost (10 pieces)\t\t\t\t\t\t\t\tEdgeBoost (20 pieces)\t\t\t\t\t\t\t\tEdgeBoost (30 pieces)\t\t\t\t\t\t\t\tNo EdgeBoost\n", HotCacheNumPieces);
		n = 2;//行数
		for (j = 0; j <= 30; j++) {
			if (j == 0)HotCacheNumPieces = 0;
			else HotCacheNumPieces = j * 10;
			MinAveInterrupt = 1.0e32;
			fprintf(ResultFile, "%d\t", HotCacheNumPieces);
			for (l = 0; l <= 0; l++) {

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k;
					Initialize();
					InitializeCloudNode(CloudEdgeBandwidth);
					InitializeEdgeNodes(EdgeEdgeBandwidth, EdgeClientBandwidth);
					InitializeClientNodes();

					Simulate();
					AveInterruptDuration += AverageInterruptDuration;
					AveNumInterrupt += AverageNumInterrupt;
					if (MaxInterrupt < MaximumInterruptDuration)
						MaxInterrupt = MaximumInterruptDuration;
					if (MinInterrupt > MinimumInterruptDuration)
						MinInterrupt = MinimumInterruptDuration;
					Finalize();
				}
				AveInterruptDuration /= NSIM;
				AveNumInterrupt /= NSIM;
				fprintf(ResultFile, "%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t", AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes, TotalEdgeEdgeWriteBytes, TotalEdgeEdgeReadBytes, TotalCloudEdgeWriteBytes, TotalCloudEdgeReadBytes);
				fflush(ResultFile);

			}
			fprintf(ResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
}
void EvaluateEdgeEdgeBandwidth() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth = 1600000000.0;
	EdgeEdgeBandwidth = 1.0;
	EdgeClientBandwidth = 100000000.0;

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 30 * 60.0;
	PieceSize = (int)(5.0 * BitRate / 8);//5秒
	NumPrePieces = 0;
	SimulationTime = 6.0 * 60 * 60;
	BandwidthWaver = 0.0;
	//HotCacheNumPieces = 1000000000 / PieceSize;
	HotCacheNumPieces = 0;//下で変える
	NumEdges = 8;
	NumVideos = 100;

	sprintf(FileName, "EvaluateEdgeEdgeBandwidth.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 7; i <= 7; i++) {
		if (i == 0)AverageArrivalInterval = 1;
		else AverageArrivalInterval = (i - 1.0) * 2;
		fprintf(ResultFile, "%d pieces\tEdgeBoost (10 pieces)\t\t\t\t\t\t\t\tEdgeBoost (20 pieces)\t\t\t\t\t\t\t\tEdgeBoost (30 pieces)\t\t\t\t\t\t\t\tNo EdgeBoost\n", HotCacheNumPieces);
		n = 2;//行数
		for (j = 0; j <= 5; j++) {
			if (j == 0)EdgeEdgeBandwidth = 1600000000.0;
			else EdgeEdgeBandwidth = j * 100000000.0;
			MinAveInterrupt = 1.0e32;
			fprintf(ResultFile, "%lf\t", EdgeEdgeBandwidth / 1000000.0);
			for (l = 3; l <= 3; l++) {
				if (l < 3)NumPrePieces = (l + 1) * 10;
				else NumPrePieces = 0;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k;
					Initialize();
					InitializeCloudNode(CloudEdgeBandwidth);
					InitializeEdgeNodes(EdgeEdgeBandwidth, EdgeClientBandwidth);
					InitializeClientNodes();

					Simulate();
					AveInterruptDuration += AverageInterruptDuration;
					AveNumInterrupt += AverageNumInterrupt;
					if (MaxInterrupt < MaximumInterruptDuration)
						MaxInterrupt = MaximumInterruptDuration;
					if (MinInterrupt > MinimumInterruptDuration)
						MinInterrupt = MinimumInterruptDuration;
					Finalize();
				}
				AveInterruptDuration /= NSIM;
				AveNumInterrupt /= NSIM;
				fprintf(ResultFile, "%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t", AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes, TotalEdgeEdgeWriteBytes, TotalEdgeEdgeReadBytes, TotalCloudEdgeWriteBytes, TotalCloudEdgeReadBytes);
				fflush(ResultFile);

			}
			fprintf(ResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
}
void EvaluateNumVideos() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth = 600000000.0;
	EdgeEdgeBandwidth = 1000000000000.0;
	EdgeClientBandwidth = 100000000.0;

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 30 * 60.0;
	PieceSize = (int)(5.0 * BitRate / 8);//5秒
	NumPrePieces = 70;
	SimulationTime = 6.0 * 60 * 60;
	BandwidthWaver = 0.0;
	//HotCacheNumPieces = 1000000000 / PieceSize;
	HotCacheNumPieces = 0;//下で変える
	NumEdges = 8;
	NumVideos = 100;

	sprintf(FileName, "EvaluateNumVideos.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 7; i <= 7; i++) {
		if (i == 0)AverageArrivalInterval = 1;
		else AverageArrivalInterval = (i - 1.0) * 2;
		fprintf(ResultFile, "%d pieces\tEdgeBoost (10 pieces)\t\t\t\t\t\t\t\tEdgeBoost (20 pieces)\t\t\t\t\t\t\t\tEdgeBoost (30 pieces)\t\t\t\t\t\t\t\tNo EdgeBoost\n", HotCacheNumPieces);
		n = 2;//行数
		for (j = 0; j <= 5; j++) {
			if (j == 0)NumVideos=1;
			else NumVideos=(int)pow(10,j);
			MinAveInterrupt = 1.0e32;
			fprintf(ResultFile, "%d\t", NumVideos);
			for (l = 0; l <= 0; l++) {

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k;
					Initialize();
					InitializeCloudNode(CloudEdgeBandwidth);
					InitializeEdgeNodes(EdgeEdgeBandwidth, EdgeClientBandwidth);
					InitializeClientNodes();

					Simulate();
					AveInterruptDuration += AverageInterruptDuration;
					AveNumInterrupt += AverageNumInterrupt;
					if (MaxInterrupt < MaximumInterruptDuration)
						MaxInterrupt = MaximumInterruptDuration;
					if (MinInterrupt > MinimumInterruptDuration)
						MinInterrupt = MinimumInterruptDuration;
					Finalize();
				}
				AveInterruptDuration /= NSIM;
				AveNumInterrupt /= NSIM;
				fprintf(ResultFile, "%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t", AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes, TotalEdgeEdgeWriteBytes, TotalEdgeEdgeReadBytes, TotalCloudEdgeWriteBytes, TotalCloudEdgeReadBytes);
				fflush(ResultFile);

			}
			fprintf(ResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
}
void EvaluateNumEdges() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth = 600000000.0;//600Mb
	EdgeEdgeBandwidth = 1000000000000.0;//1Tb
	EdgeClientBandwidth = 100000000.0;//100Mb

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 30 * 60.0;
	PieceSize = (int)(5.0 * BitRate / 8);//5秒Byte
	NumPrePieces = 70;
	SimulationTime = 6.0 * 60 * 60;
	BandwidthWaver = 0.0;
	//HotCacheNumPieces = 1000000000 / PieceSize;
	HotCacheNumPieces = 0;//下で変える
	NumEdges = 8;
	NumVideos = 100;

	sprintf(FileName, "EvaluateNumEdges.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 7; i <= 7; i++) {
		if (i == 0)AverageArrivalInterval = 1;
		else AverageArrivalInterval = (i - 1.0) * 2;
		fprintf(ResultFile, "%d pieces\tEdgeBoost (10 pieces)\t\t\t\t\t\t\t\tEdgeBoost (20 pieces)\t\t\t\t\t\t\t\tEdgeBoost (30 pieces)\t\t\t\t\t\t\t\tNo EdgeBoost\n", HotCacheNumPieces);
		n = 2;//行数
		for (j = 0; j <= 5; j++) {
			if (j == 0)NumEdges = 1;
			else NumEdges=j*2;
			MinAveInterrupt = 1.0e32;
			fprintf(ResultFile, "%d\t", NumEdges);
			for (l = 0; l <= 0; l++) {

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k;
					Initialize();
					InitializeCloudNode(CloudEdgeBandwidth);
					InitializeEdgeNodes(EdgeEdgeBandwidth, EdgeClientBandwidth);
					InitializeClientNodes();

					Simulate();
					AveInterruptDuration += AverageInterruptDuration;
					AveNumInterrupt += AverageNumInterrupt;
					if (MaxInterrupt < MaximumInterruptDuration)
						MaxInterrupt = MaximumInterruptDuration;
					if (MinInterrupt > MinimumInterruptDuration)
						MinInterrupt = MinimumInterruptDuration;
					Finalize();
				}
				AveInterruptDuration /= NSIM;
				AveNumInterrupt /= NSIM;
				fprintf(ResultFile, "%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t", AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes, TotalEdgeEdgeWriteBytes, TotalEdgeEdgeReadBytes, TotalCloudEdgeWriteBytes, TotalCloudEdgeReadBytes);
				fflush(ResultFile);

			}
			fprintf(ResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
}
int main(int argc, char* argv[])
{


#ifdef DYNAMIC
	Nodes = (struct node*)malloc(sizeof(struct node) * NNODE);
#endif
	//EvaluateNumEdges();
	//EvaluateNumVideos();
	//EvaluateHotCacheNumPieces();
	//EvaluateEdgeEdgeBandwidth();
	//EvaluateNumPrePieces();
	//EvaluateCloudEdgeBandwidth();
	EvaluateLambda();

#ifdef DYNAMIC
	free(Nodes);
#endif


	return 0;
}

