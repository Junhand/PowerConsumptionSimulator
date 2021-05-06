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
#define CPUCORE 16

#define RETRYCYCLE(a) (8.0*PieceSize/Nodes[a].AverageInBand)

#define CLIENTONEVENT 0//初期
#define RETRYEVENT 1//使っていない
#define CLOUDEDGEFETCHEVENT 2//使っていない
#define CLOUDEDGEFINISHEVENT 3//クラウドエッジ通信
#define EDGEEDGEFETCHEVENT 4//使っていない
#define EDGEEDGEFINISHEVENT 5//エッジエッジ通信
#define EDGECLIENTFETCHEVENT 6//初期segment取得
#define EDGECLIENTFINISHEVENT 7//エッジクライアント通信
#define CLOUDEDGERETRYEVENT 8//使っていない
#define EDGEEDGERETRYEVENT 9//使っていない
#define EDGECLIENTRETRYEVENT 10//使っていない

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
	struct clientnode* EdgeEdgeSendClient;
	int EdgeEdgeSendPieceID;
	int EdgeEdgeSendVideoID;
	int EdgeEdgeSendEdgeID;
	struct clientnode* EdgeClientSendClient;
	int EdgeClientSendPieceID;
	int EdgeClientSendVideoID;
	int NumReceiving;//受信かつ保存する数
	int NumPreviousSending;
	int NumSending;
	int NumPreviousClientSending;
	int NumClientSending;
	struct edgenode* EdgeEdgeReceiveEdgeNode;
};

struct cloudnode {
	struct clientnodelist* CloudEdgeWaitingList;
	double CloudEdgeReadBytes;
	short State;
	double CloudEdgeBandwidth;
	struct clientnode* CloudEdgeSendClient;
	int CloudEdgeSendPieceID;
	int CloudEdgeSendVideoID;
	int CloudEdgeSendEdgeID;
	int NumPreviousSending;
	int NumSending;
};

struct clientnode {
	unsigned int ID;
	short State;
	double OnTime;
	double RemainingDataSize;
	double PreviousTime;//帯域幅を送信人数で分割した場合の1segment送信にかかる時間の計算のために使用(クラウドエッジ・エッジエッジ)
	double RemainingClientDataSize;
	double PreviousClientTime;//帯域幅を送信人数で分割した場合の1segment送信にかかる時間の計算のために使用(エッジクライアント)
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

struct cloudserver {//どこからsegmentを取ってくるか決定
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
	double EdgePreviousTime[MAXNUMEDGENODES];
	double CloudPreviousTime;
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
int addCount=0;
int PredictDeleteHotCache[MAXNUMEDGENODES];

int numOfExsistPieceID=0;
struct cloudserver CloudServer;

double CloudPowerConsumption[CPUCORE+1] = {72.6111373901367,73.3708267211914,80.09765625,89.1057586669921,109.371032714843,114.578422546386,116.197860717773,118.549942016601,
											121.804946899414,125.261123657226,128.482513427734,131.525634765625,134.554611206054,137.900344848632,142.114013671875,147.735748291015,154.568893432617};

double EdgePowerConsumption[8][CPUCORE+1]={{75.5072250366211,78.5007400512695,83.9513168334961,96.5199661254882,111.490814208984,115.153198242187,116.738952636718,118.769119262695,
											121.638977050781,125.254592895507,129.197601318359,133.114532470703,136.868331909179,140.37451171875,143.535095214843,146.291625976562,148.657501220703},

											{73.0350646972656,76.5428695678711,85.539794921875,95.9329071044921,112.16064453125,114.850807189941,115.99543762207,117.923583984375,
											121.137451171875,125.16177368164,128.96336364746,132.30859375,135.355865478515,138.163925170898,140.689666748046,142.857879638671,144.611694335937},

											{72.6111373901367,73.3708267211914,80.09765625,89.1057586669921,109.371032714843,114.578422546386,116.197860717773,118.549942016601,
											121.804946899414,125.261123657226,128.482513427734,131.525634765625,134.554611206054,137.900344848632,142.114013671875,147.735748291015,154.568893432617},

											{75.3260345458984,80.2545471191406,88.7037124633789,98.5816497802734,109.378829956054,112.863677978515,114.788528442382,117.288619995117,
											120.63175201416,124.335800170898,127.788421630859,130.938751220703,133.99607849121,137.117843627929,140.37107849121,143.747406005859,147.171936035156},

											{73.1381759643554,79.5980834960937,86.5708389282226,97.6285018920898,114.8023147583,116.765838623046,118.112167358398,121.090301513671,
											126.128524780273,130.595870971679,133.363891601562,135.721542358398,138.33787536621,141.519271850585,145.441329956054,150.165374755859,155.60610961914},

											{67.2812805175781,81.8797454833984,84.2863006591796,91.3649597167968,107.327644348144,110.240264892578,111.659072875976,113.873725891113,
											117.193794250488,121.396041870117,125.810333251953,129.75048828125,132.954498291015,135.522354125976,137.627990722656,139.389129638671,140.87043762207},

											{72.4927597045898,79.3793029785156,86.9039840698242,96.7012252807617,112.39468383789,115.211547851562,116.670486450195,118.830093383789,
											121.921066284179,125.476585388183,128.882080078125,132.064208984375,135.220672607421,138.561340332031,142.263473510742,146.442733764648,151.079284667968},

											{73.0856246948242,79.4303665161132,85.7974090576171,93.4306182861328,107.284042358398,110.830764770507,112.784423828125,115.728256225585,
											119.898178100585,124.181518554687,127.584671020507,130.224288940429,132.468795776367,134.572662353515,136.689971923828,138.903488159179,141.239547729492}
											};

double MaxPowerConsumption = 20.26527405;//増加量の最大
double MinPowerConsumption = 0.759689331;//増加量の最小
double NormalizeCloudPowerConsumption[CPUCORE+1];
double NormalizeEdgePowerConsumption[8][CPUCORE+1];

double alpha, beta;

char ResultFileName[64];
FILE* ServerResultFile;

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
		TotalEdgeClientReadBytes += EdgeNodes[i].EdgeClientReadBytes;//edgeのRead クライアントから要求されたデータ量
		TotalEdgeEdgeWriteBytes += EdgeNodes[i].EdgeEdgeWriteBytes;//edgeのWrite　他のedgeから書き込んだデータ量
		TotalEdgeEdgeReadBytes += EdgeNodes[i].EdgeEdgeReadBytes;//edgeのread 他のedgeから要求されたデータ量
		TotalCloudEdgeWriteBytes += EdgeNodes[i].CloudEdgeWriteBytes;//edgeのwrite　クラウドから書き込まれたデータ量
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
		while (EdgeNodes[i].EdgeClientWaitingList) {//edge client間waitingListの消去
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

	addCount += 1;

	NewEvent = new struct event;
	NewEvent->EventID = EventID;//eventの状態
	NewEvent->EventNum = NextEventNum++;//イベント番号 0~
	NewEvent->ClientNode= ClientNode;//clientの読みこみ
	NewEvent->Time = Time;//時間
	NewEvent->Data1 = Data1;//pieceID
	NewEvent->Data2 = Data2;//キャッシュするかどうか cached true false

	if (TopEvent == NULL) {//初期の場合
		TopEvent = NewEvent;
		TopEvent->Next = NULL;
		return NewEvent;
	}

	PreviousEvent = NULL;
	CurrentEvent = TopEvent;
	while (CurrentEvent) {
		if ((CurrentEvent->Time > NewEvent->Time) ||//追加するEventの方が前の時間かどうか
			((CurrentEvent->Time == NewEvent->Time) && (CurrentEvent->EventID > NewEvent->EventID))) {//同時間で新しいEventの方が段階が低いかどうか
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

void EdgeClientWaiting(double EventTime, struct edgenode* EdgeNode) {
	struct clientnodelist* WaitingList = EdgeNode->EdgeClientWaitingList;//edgeのclient waitingList
	struct clientnode* ClientNode;
	double Bandwidth, FinishTime, r;
	int PieceID;
	bool Cached;
	double RemainingDataSize;

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
		EdgeNode->EdgeClientSendClient = ClientNode;
		EdgeNode->EdgeClientSendPieceID = PieceID;
		EdgeNode->EdgeClientSendVideoID = ClientNode->VideoID;

		EdgeNode->EdgeClientWaitingList = WaitingList->Next;
		delete WaitingList;

		EdgeNode->State |= EDGECLIENTSENDSTATE;
		ClientNode->State |= EDGECLIENTRECEIVESTATE;
		ClientNode->State &= ~EDGECLIENTWAITSTATE;
		r = (double)rand() / RAND_MAX;
		Bandwidth = (EdgeNode->EdgeClientBandwidth)*(1.0 + BandwidthWaver * (2.0 * r - 1.0));

		RemainingDataSize = ClientNode->RemainingClientDataSize; //- (EventTime - ClientNode->PreviousClientTime) * Bandwidth / EdgeNode->NumPreviousClientSending;
		if(RemainingDataSize < 0){
			RemainingDataSize = 0;
		}
		ClientNode->PreviousClientTime = EventTime;
		ClientNode->RemainingClientDataSize = RemainingDataSize;

		FinishTime = EventTime + RemainingDataSize / (Bandwidth / EdgeNode->NumClientSending);
		AddEvent(FinishTime, EDGECLIENTFINISHEVENT, ClientNode, PieceID, Cached);

		WaitingList = EdgeNode->EdgeClientWaitingList;
		while(WaitingList != NULL){
			WaitingList->ClientNode->RemainingClientDataSize -= (EventTime - WaitingList->ClientNode->PreviousClientTime) * Bandwidth / EdgeNode->NumPreviousClientSending;
			WaitingList->ClientNode->PreviousClientTime = EventTime;
			if(WaitingList->ClientNode->RemainingClientDataSize < 0){
				WaitingList->ClientNode->RemainingClientDataSize = 0;
			}
			WaitingList = WaitingList->Next;
		}
		EdgeNode->NumPreviousClientSending = EdgeNode->NumClientSending;
	}
}

void EdgeEdgeWaiting(double EventTime, struct edgenode* FromEdgeNode) {
	struct clientnodelist* WaitingList = FromEdgeNode->EdgeEdgeWaitingList;
	struct edgenode* ToEdgeNode;
	struct clientnode* ClientNode;
	double Bandwidth, FinishTime, r;
	int PieceID;
	bool Stored;
	double RemainingDataSize;

	if (WaitingList != NULL) {
		ClientNode = WaitingList->ClientNode;
		FromEdgeNode->EdgeEdgeSendClient = ClientNode;

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

		RemainingDataSize = ClientNode->RemainingDataSize - (EventTime - ClientNode->PreviousTime) * Bandwidth / FromEdgeNode->NumPreviousSending;
		if(RemainingDataSize < 0){
			RemainingDataSize = 0;
		}
		ClientNode->PreviousTime = EventTime;
		ClientNode->RemainingDataSize = RemainingDataSize;

		FinishTime = EventTime + RemainingDataSize / (Bandwidth / FromEdgeNode->NumSending);
		AddEvent(FinishTime, EDGEEDGEFINISHEVENT, ClientNode, PieceID, Stored);

		WaitingList = FromEdgeNode->EdgeEdgeWaitingList;
		while(WaitingList != NULL) {
			WaitingList->ClientNode->RemainingDataSize -= (EventTime - WaitingList->ClientNode->PreviousTime) * Bandwidth / FromEdgeNode->NumPreviousSending;
			WaitingList->ClientNode->PreviousTime = EventTime;
			if(WaitingList->ClientNode->RemainingDataSize < 0){
				WaitingList->ClientNode->RemainingClientDataSize = 0;
			}
			WaitingList = WaitingList->Next;
			WaitingList->Next;
		}
		FromEdgeNode->NumPreviousSending = FromEdgeNode->NumSending;
	}
}

void CloudEdgeWaiting(double EventTime) {
	struct clientnodelist* WaitingList = CloudNode.CloudEdgeWaitingList;
	struct edgenode* ToEdgeNode;
	struct clientnode* ClientNode;
	double Bandwidth, FinishTime, r;
	int PieceID;
	bool Stored;
	double RemainingDataSize;

	if (WaitingList != NULL ) {
		ClientNode = WaitingList->ClientNode;
		CloudNode.CloudEdgeSendClient = ClientNode;
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

		RemainingDataSize = ClientNode->RemainingDataSize - (EventTime - ClientNode->PreviousTime) * Bandwidth / CloudNode.NumPreviousSending;
		if(RemainingDataSize < 0){
			RemainingDataSize = 0;
		}
		ClientNode->PreviousTime = EventTime;
		ClientNode->RemainingDataSize = RemainingDataSize;
		
		FinishTime = EventTime + RemainingDataSize / (Bandwidth / CloudNode.NumSending);
		AddEvent(FinishTime, CLOUDEDGEFINISHEVENT, ClientNode, PieceID, Stored);

		WaitingList = CloudNode.CloudEdgeWaitingList;
		while(WaitingList != NULL) {
			WaitingList->ClientNode->RemainingDataSize -= (EventTime - WaitingList->ClientNode->PreviousTime) * Bandwidth / CloudNode.NumPreviousSending;
			WaitingList->ClientNode->PreviousTime = EventTime;
			if(WaitingList->ClientNode->RemainingDataSize < 0){
				WaitingList->ClientNode->RemainingDataSize = 0;
			}
			WaitingList = WaitingList->Next;
			WaitingList->Next;
		}
		CloudNode.NumPreviousSending = CloudNode.NumSending;
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

		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == -NumVideos)
			break;
		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == VideoID) {
			CurrentPieceID = ConnectedEdgeNode->HotCache[HotCachePosition].PieceID;
			if ( CurrentPieceID==SearchPieceID && CloudServer.ExsistPiece[ConnectedEdgeNode->ID][VideoID][CurrentPieceID]==1) {
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

		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == -NumVideos)
			break;
		if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == VideoID) {
			CurrentPieceID = ConnectedEdgeNode->HotCache[HotCachePosition].PieceID;
			
			if(CloudServer.ExsistPiece[ConnectedEdgeNode->ID][VideoID][CurrentPieceID]==1){
				if (ReceivePieceID <= CurrentPieceID) {//欲しいpieceより大きいpieceが一番近いedgeにあった
					numOfExsistPieceID += 1;
					if (ConnectedEdgeNode->HotCache[HotCachePosition].Voted == -1)//初使用
						ConnectedEdgeNode->HotCache[HotCachePosition].Voted = 1;
					else
						ConnectedEdgeNode->HotCache[HotCachePosition].Voted++;//取得する予定のpieceを予約
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

			if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == -NumVideos)
				break;
			if (ConnectedEdgeNode->HotCache[HotCachePosition].VideoID == VideoID) {
				CurrentPieceID = ConnectedEdgeNode->HotCache[HotCachePosition].PieceID;

				if(CloudServer.ExsistPiece[ConnectedEdgeNode->ID][VideoID][CurrentPieceID]==1){
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
		if (ConnectedEdgeNode->HotCache[CurrentHotCachePosition].Voted == 0 
		&& (ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID<0 || ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID==NumVideos)) {
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

bool IsStoreHotCache(struct clientnode* ClientNode, bool Predict) {//storeされるかどうか確認
	struct edgenode* ConnectedEdgeNode = ClientNode->ConnectedEdgeNode;
	int HotCacheStart= ConnectedEdgeNode->HotCacheStart, HotCacheEnd= ConnectedEdgeNode->HotCacheEnd,HotCacheUsed;
	int CurrentHotCachePosition,DeleteHotCachePosition;
	int CurrentNumDelete = 0;
	int CurrentNumReceiving = ConnectedEdgeNode->NumReceiving;
	
	struct clientnodelist* ClientList;
	struct clientnode* CurrentClientNode;
	int VirtualCurrentPosition,VirtualHotCacheEnd,VirtualDeleteHotCachePosition;
	int VideoID;
	int PieceID;

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
				if(ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID>=0 && ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID<NumVideos){
					VideoID = ConnectedEdgeNode->HotCache[CurrentHotCachePosition].VideoID;
					PieceID = ConnectedEdgeNode->HotCache[CurrentHotCachePosition].PieceID;
					DeleteHotCachePosition = CurrentHotCachePosition;
				}
			}
		} while (CurrentHotCachePosition != HotCacheEnd);
		
		if (CurrentNumDelete>CurrentNumReceiving){
			if(Predict){//消去の予約
				CloudServer.ExsistPiece[ConnectedEdgeNode->ID][VideoID][PieceID] = -1;
				PredictDeleteHotCache[ConnectedEdgeNode->ID] += 1;

				if(ConnectedEdgeNode->HotCache[DeleteHotCachePosition].VideoID!=0){
					ConnectedEdgeNode->HotCache[DeleteHotCachePosition].VideoID = -VideoID;
				}else{//videoが0の時
					ConnectedEdgeNode->HotCache[DeleteHotCachePosition].VideoID = NumVideos;
				}
				
				if(ConnectedEdgeNode->HotCache[DeleteHotCachePosition].PieceID!=0){
					ConnectedEdgeNode->HotCache[DeleteHotCachePosition].PieceID = -PieceID;
				}else{//pieceが0の時
					ConnectedEdgeNode->HotCache[DeleteHotCachePosition].PieceID = NumPieces;
				}
			}
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



void CheckHotCache(struct clientnode* ClientNode) {
	int i,j;
	struct edgenode* EdgeNode = ClientNode->ConnectedEdgeNode;
	for (i = 0; i < HotCacheNumPieces; i++) {
		for (j = i + 1; j < HotCacheNumPieces; j++) {
			if ((EdgeNode->HotCache[i].VideoID >=0)
				&&(EdgeNode->HotCache[i].VideoID < NumVideos)
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
			PredictDeleteHotCache[ConnectedEdgeNode->ID] -= 1;
			//CloudServer.ExsistPiece[ConnectedEdgeNode->ID][ConnectedEdgeNode->HotCache[DeleteHotCachePosition].VideoID][ConnectedEdgeNode->HotCache[DeleteHotCachePosition].PieceID] = -1;

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



void EventParser() {
	if (10<LastSumInterruptDuration)
		LastSumInterruptDuration = LastSumInterruptDuration ;
	if (TopEvent->Time > 18050)
		TopEvent->Time = TopEvent->Time;
	if (TopEvent->ClientNode->ID == 1857)
		TopEvent->ClientNode->ID = TopEvent->ClientNode->ID;
	if ((Duration < TopEvent->Time) && (Duration < TopEvent->ClientNode->SumInterruptDuration)) {
		//printf("Warning LongWaiting");
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


void Simulate() {
	struct event* CurrentEvent;

	while (TopEvent->Time < SimulationTime) {
#ifdef LOG
		fprintf(LogFile, "time%lf\tevent%d\teventID%d\tclientID%d\tData1,%d\tData2,%d\tn_edge%d\tt_node%d\tvideoID%d\n", TopEvent->Time, TopEvent->EventNum, TopEvent->EventID, TopEvent->ClientNode->ID, TopEvent->Data1, TopEvent->Data2,TopEvent->ClientNode->ConnectedEdgeID,TopEvent->ClientNode->VideoRequestsID[TopEvent->Data1],TopEvent->ClientNode->VideoID);
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

		
		//printf("add%d\n",addCount);
		CurrentEvent = TopEvent;
		TopEvent = TopEvent->Next;
		addCount -=1;
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

