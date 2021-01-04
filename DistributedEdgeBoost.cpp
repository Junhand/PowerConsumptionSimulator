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

double CloudPowerConsumption[CPUCORE+1] = {56.05582809,92.95009613,101.8073807,112.2240067,120.1051788,123.9835587,125.6780472,127.053833,
										   128.6921692,130.7683411,133.304657,136.1736145,139.1330872,141.9121094,144.2817841,146.1325378,147.4867859};

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

double MaxPowerConsumption = 36.89426804;//増加量の最大
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

int CloudServerRequest(double EventTime, struct clientnode* ClientNode, int VideoID, int PieceID) {
	int whichNode[NumPieces][NumEdges+1];
	int existCount = 0;
	int EdgeOrCloudFlag = 1;
	double CloudEdgeNumSending,EdgeEdgeNumSending,EdgeClientNumSending;
	double PredictEdgePowerConsumption[NumEdges][CPUCORE+1];
	double PredictCloudPowerConsumption[CPUCORE+1];
	double PredictEdgeBandwidth[NumEdges];
	double PredictCloudBandwidth;
	double tempAlpha, tempBeta, cost;
	int AccessNum;
	double maxCost=-1;
	double index=-1;
	int edgeCount=0;
	int i,j,k;
	
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

	/*for(j=0; j<NumPieces; j++){//各pieceの取得場所決定(ランダム)
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
	}*/

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
						cost = tempAlpha * PredictEdgeBandwidth[k] / EdgeNodes[k].EdgeEdgeBandwidth + tempBeta * NormalizeEdgePowerConsumption[k][AccessNum];
						if(maxCost<cost){
							maxCost=cost;
							index=k;
						}
					}else{
						AccessNum = CloudNode.NumPreviousSending + 1;
						if(AccessNum>16) AccessNum=16;
						PredictCloudBandwidth = CloudNode.CloudEdgeBandwidth/(CloudNode.NumSending+1);
						cost = tempAlpha * PredictCloudBandwidth / CloudNode.CloudEdgeBandwidth + tempBeta * NormalizeCloudPowerConsumption[AccessNum];
						if(maxCost<cost){
							if(j==0) EdgeOrCloudFlag = -1;//最初クラウドから取得
							maxCost=cost;
							index=k;
						}
						ClientNode->VideoRequestsID[j] = index;
						maxCost=-1;
					}
				}
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


	if(CloudNode.NumPreviousSending==0) CloudEdgeNumSending=1.0;
	else CloudEdgeNumSending = (double)CloudNode.NumPreviousSending;
	AccessNum = CloudNode.NumPreviousSending + 1;
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

	return EdgeOrCloudFlag;//-1はcloud 1はedgeから最初のsegmentを取得
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

		RemainingDataSize = ClientNode->RemainingClientDataSize - (EventTime - ClientNode->PreviousClientTime) * Bandwidth / EdgeNode->NumPreviousClientSending;
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
			//CloudServerRequest(EventTime, ClientNode, VideoID, ReceivePieceID);
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
			if(j==0) {
				NormalizeEdgePowerConsumption[i][j]=0;
			}else{
				NormalizeEdgePowerConsumption[i][j] = 1 - ( ( EdgePowerConsumption[i][j] - EdgePowerConsumption[i][j-1] - MinPowerConsumption ) / MaxMinPowerConsumption );
			}
		}
	}
	for(int j=0; j<CPUCORE+1; j++){
		if(j==0){
			NormalizeCloudPowerConsumption[j]=0;
		}else{
			NormalizeCloudPowerConsumption[j] = 1 - ( ( CloudPowerConsumption[j] - CloudPowerConsumption[j-1] - MinPowerConsumption ) / MaxMinPowerConsumption );
		}
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

void EvaluateLambda() {
	int k, l, n;
	double i,j;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt, EdgeVolume;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth =  1000000000.0;//1Gbps
	EdgeEdgeBandwidth =   1000000000.0;//1Gbps
	EdgeClientBandwidth = 1000000000.0;//1Gbps 非同期通信のために帯域幅が上2つと同じ場合は少し早くすると良い。しなければバッファがないためクラウドエッジ・エッジクラウドで同期通信のようになってしまう

	AverageArrivalInterval = 99999.0;//下で変えてる
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024    5M
	Duration = 30 * 60.0;//視聴時間 30*60
	SegmentTime = 10.0;
	PieceSize = (int)(SegmentTime*BitRate / 8);//5秒
	NumPrePieces = 0;//下で変えてる  360piece
	SimulationTime = 6.0 * 60 * 60;//5*60*60
	BandwidthWaver = 0.0;
	HotCacheNumPieces = 15000000000 / PieceSize;//100MB 1GB　おそらく合計8GB? 320pieces = 320*5*bitRate bit = 1GByte
	//HotCacheNumPieces = 0;
	NumEdges = 8;//8
	NumVideos = 100;//900Gb 112.5GB
	NumPrePieces = 0;
	alpha=0;//ResponseTime
	beta=1;//PowerConsumption

	sprintf(FileName, "EvaluateLambda.dat");
	ResultFile = myfopen(FileName, "w");

	sprintf(ResultFileName, "ResultLambda.dat");
	ServerResultFile = myfopen(ResultFileName, "w");
	for (i = 0; i <= 1; i+=0.5) {
		alpha = i;
		beta = 1-i;
		fprintf(ResultFile, "SimulationTime:%.0lf\talpha%.2f\tbeta%.2f\n", SimulationTime,alpha,beta);
		n = 2;//行数
		for (j = 11.25*1; j <= 11.25*1; j+=11.25) {//15
			HotCacheNumPieces = (double)j*1000000000/PieceSize; //NumPrePieces = (l + 1) * 10;
			
			MinAveInterrupt = 1.0e32;
			//fprintf(ResultFile, "%lf\t\n", AverageArrivalInterval);
			for (l = 1; l <= 10; l++) {//3
				if (l == 0) AverageArrivalInterval = 12;//12
				else AverageArrivalInterval = l ;//j
				
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
				fclose(LogFile);

			}
			fprintf(ResultFile, "\n");
			fprintf(ServerResultFile, "\n");
			n++;
		}
		while (n < 51) {
			fprintf(ResultFile, "\n");
			fprintf(ServerResultFile, "\n");
			n++;
		}
	}
	fclose(ResultFile);
	fclose(ServerResultFile);
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

