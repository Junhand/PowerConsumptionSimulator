//#include <iostream>

#pragma warning(disable:4100)
//#include "stdafx.h"
#include <stdio.h>
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include <string.h>
#define PI 3.14159265358979323846264338327950288419716939937510 

#define LOG
#define NSIM 1
//#define DYNAMIC

#ifdef DYNAMIC
#define NNODE 260000
#else
#define NNODE 520000
#endif

#define NCHANNEL 20
#define NPIECEG 2000
#define CONNECTIONOVERHEAD 20
#define CONVERGECHECK 1 //1で無効化
#define RETRYCYCLE(a) (8.0*PieceSize/Nodes[a].AverageInBand)

#define REQUESTONEVENT 0
#define AFTERONEVENT 1
#define RETRYEVENT 2
#define FETCHREQUESTEVENT 3
#define FINISHREQUESTEVENT 4
#define OFFEVENT 5
#define BROFINISHEVENT 6
#define BROSTARTEVENT 7

struct event {
	double	Time;
	int		EventNum;
	int		EventID;
	int		NodeID;
	int		Data1;
	int		Data2;
	struct event* Next;
};
struct interrupt {
	double	StartTime;
	double	EndTime;
	int		Piece;
	struct interrupt* Next;
};
struct nodelist {
	struct nodelist* Next;
	struct nodelist* Previous;
	int		NodeID;
	int		Piece;
};

#define OFFSTATE 0x0000
#define ONSTATE 0x0001
#define REQUESTSTATE 0x0002
#define SENDSTATE 0x0004
#define RECEIVESTATE 0x0008
#define WAITSTATE 0x0010
#define RECEIVEDSTATE 0x0040
#define RETRYSTATE 0x0080
//ON SEND               REQUEST→→ RECEIVE→↓→RECEIVED
//   SendingNodeID     ↑  　↑　→WAIT→↑　↓
//                ↓←RETRY　↑←←←←←←←←
//                →→↑

struct node {
	char	PieceGs[NPIECEG];//ビット表記
	int		NextContinuousPieceG;
	int		State;
	struct nodelist* WaitingNodes;
	double	RequestOnTime;
	int NumInterrupt;
	struct interrupt* Interrupts;
	double SumInterruptDuration;
	double InBand[1];
	double AverageInBand;
	int SendingNodeID;
	int ReceivingNodeID;
	int NextNodeID;
	int PreviousNodeID;
	int ReceivingPiece;
	double ReadyTime;
};

struct channel {
	double Bandwidth;
	double Interval;
	double BroTime;
	double StartTime;
	int StartPiece;
	int FinishPiece;
	int NextPiece;
	int CreateNodeID;
};


double AverageArrivalInterval;
double VarianceArrivalInterval;
double SubmitRequestLimitTime;
int SubmitRequestLimitPiece;
int PieceSize;
double BitRate;
double Duration;
double IntentDelay;
double SimulationTime;
double PlayOffTime;
double ServerBandwidth;
double BandwidthWaver;
double Prefetch;
int IntentDelayPiece;
int SequentialMode;
int NumServers;
int RandType;
int NumChannels;
int ChannelMethod;
int BroMethod;
int Max_Conn;
double Min_Exptre;
double ChannelP2PBandwidth;

unsigned int NextEventNum;
struct nodelist* OnNodeList;
struct nodelist* FinalOnNodeList;
int NumPieceGs;
#ifdef DYNAMIC
struct node* Nodes;
#else
struct node Nodes[NNODE];
#endif
struct node* Nodesp;
struct event* TopEvent;
struct nodelist* QueueNodeList;
#ifdef LOG
FILE* LogFile;
#endif
struct channel Channels[NCHANNEL];
int	NumOnNodes;
int NumSimulateNodes;
int AverageInterruptNodes;
int AverageNumInterrupt;
int NumReceivedNodes;
int NumOffNodes;
double AverageInterruptDuration;
double MaximumInterruptDuration;
double MinimumInterruptDuration;
double ConvergeTime;
int PreviousNumOnNodes[CONVERGECHECK];
int NextPreviousNumOnNodes;
int CountPiece[NPIECEG * 8 + 1];
int Seed;
int NumP2PPieces;
int NumBroPieces;

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

double RequestOnRand() {
	double r;
	long double Poisson;
	int i;

	switch (RandType) {
	case 0: //指数分布
		r = -log(((double)rand() + 1.0) / ((double)RAND_MAX + 2.0)) * AverageArrivalInterval;
		break;
	case 1://正規分布
		r = NormalRand() * VarianceArrivalInterval + AverageArrivalInterval;
		break;
	case 2:	//一定分布
		r = AverageArrivalInterval;
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

void DeleteNodeList(struct nodelist* NodeList) {
	struct nodelist* CurrentNodeList;
	while (NodeList) {
		CurrentNodeList = NodeList;
		NodeList = NodeList->Next;
		delete CurrentNodeList;
	}
}

void Finalize() {
	struct event* CurrentEvent;
	struct interrupt* CurrentInterrupt;
	int i;


	printf("AverageInterruptNodes¥t%d¥n", AverageInterruptNodes);
	printf("NumSimulateNodes¥t%d¥n", NumSimulateNodes);
	printf("AverageInterruptDuration¥t%lf¥n", AverageInterruptDuration);
	printf("NumP2PPieces¥t%d¥n", NumP2PPieces);
	printf("NumBroPieces¥t%d¥n", NumBroPieces);

// #ifdef LOG
// 	fprintf_s(LogFile, "AverageInterruptNodes¥t%d¥n", AverageInterruptNodes);
// 	fprintf_s(LogFile, "NumSimulateNodes¥t%d¥n", NumSimulateNodes);
// 	fprintf_s(LogFile, "AverageInterruptDuration¥t%lf¥n", AverageInterruptDuration);
// 	fprintf_s(LogFile, "NumP2PPieces¥t%d¥n", NumP2PPieces);
// 	fprintf_s(LogFile, "NumBroPieces¥t%d¥n", NumBroPieces);
// #endif

	printf("Finalizing...");

	while (TopEvent) {
		CurrentEvent = TopEvent;
		TopEvent = TopEvent->Next;
		delete CurrentEvent;
	}

	DeleteNodeList(OnNodeList);
	DeleteNodeList(QueueNodeList);

	for (i = 0; i < NumSimulateNodes + 1; i++) {
		DeleteNodeList(Nodes[i].WaitingNodes);
		while (Nodes[i].Interrupts) {
			CurrentInterrupt = Nodes[i].Interrupts;
			Nodes[i].Interrupts = CurrentInterrupt->Next;
			delete CurrentInterrupt;
		}
	}

#ifdef LOG
	fclose(LogFile);
#endif
	printf("Done¥n");

}
struct event* AddEvent(double Time, int EventID, int NodeID, int Data1, int Data2) {
	struct event* PreviousEvent;
	struct event* CurrentEvent;
	struct event* NewEvent;

	NewEvent = new struct event;
	NewEvent->EventID = EventID;
	NewEvent->EventNum = NextEventNum++;
	NewEvent->NodeID = NodeID;
	NewEvent->Time = Time;
	NewEvent->Data1 = Data1;
	NewEvent->Data2 = Data2;

	if (NewEvent->EventNum == 74862)
		NewEvent->EventNum = 74862;


	if (TopEvent == NULL) {
		TopEvent = NewEvent;
		TopEvent->Next = NULL;
		return NewEvent;
	}

	PreviousEvent = NULL;
	CurrentEvent = TopEvent;
	while (CurrentEvent) {
		if ((CurrentEvent->Time > NewEvent->Time) ||
			((CurrentEvent->Time == NewEvent->Time) && (CurrentEvent->EventID > NewEvent->EventID))) {
			NewEvent->Next = CurrentEvent;
			if (PreviousEvent == NULL) {
				printf("AddEvent Error¥n");
			}
			else {
				PreviousEvent->Next = NewEvent;
			}
			return NewEvent;
		}
		PreviousEvent = CurrentEvent;
		CurrentEvent = CurrentEvent->Next;
	}
	PreviousEvent->Next = NewEvent;
	NewEvent->Next = CurrentEvent;//CurrentEvent=NULL
	return NewEvent;
}
double CalculateBroDownLoadTime(int NodeID, double Time, int Piece) {
	double BroDownLoadTime, PieceBroPosition, TimePosition, MinBroDownLoadTime;
	int CurrentChannel, MinChannel;

	BroDownLoadTime = 1.0e32;
	MinBroDownLoadTime = 1.0e32;
	for (CurrentChannel = 0; CurrentChannel < NumChannels; CurrentChannel++) {
		//2011.7.26Channels[CurrentChannel].StartPiece==0を追加
		if (Channels[CurrentChannel].StartPiece == 0)
			continue;
		if (Piece < Channels[CurrentChannel].StartPiece)
			continue;
		//if(SequentialMode==0)
		if (Channels[CurrentChannel].FinishPiece < Piece)
			continue;
		TimePosition = Time - Channels[CurrentChannel].StartTime;
		PieceBroPosition = Channels[CurrentChannel].BroTime * (Piece - Channels[CurrentChannel].StartPiece);
		if (PieceBroPosition + Channels[CurrentChannel].BroTime + (1.0e-8) <= TimePosition) {
			BroDownLoadTime = 1.0e32;
		}
		else if (PieceBroPosition < TimePosition) {
			if (TimePosition - (Time - Nodes[NodeID].RequestOnTime) < PieceBroPosition) {
				BroDownLoadTime = PieceBroPosition - TimePosition;
			}
			else {
				BroDownLoadTime = 1.0e32;
			}
		}
		else {
			BroDownLoadTime = PieceBroPosition - TimePosition;
		}
		BroDownLoadTime += Channels[CurrentChannel].BroTime;

		if (BroDownLoadTime < MinBroDownLoadTime) {
			MinChannel = CurrentChannel;
			MinBroDownLoadTime = BroDownLoadTime;
		}
	}
	if (MinBroDownLoadTime > 0.9e32) {
		return 1.0e32;
	}

	return BroDownLoadTime;
}
double CalculateP2PDownLoadTime(int SourceNodeID, int TargetNodeID) {
	double P2PPieceTime;
	double P2PDownLoadTime;
	struct nodelist* CurrentWaitingNodes;

	//2018.11.27 P2PPieceTime = 8.0*PieceSize / Nodes[SourceNodeID].InBand[TargetNodeID];
	if (Nodes[SourceNodeID].InBand[TargetNodeID] < 1.0e-8)
		P2PPieceTime = 1.0e8;
	else
		P2PPieceTime = 8.0 * PieceSize / Nodes[SourceNodeID].InBand[TargetNodeID];
	P2PDownLoadTime = P2PPieceTime;

	//送信中を待つ時間
	if ((Nodes[TargetNodeID].State) & SENDSTATE) {
		P2PPieceTime = 8.0 * PieceSize / Nodes[Nodes[TargetNodeID].SendingNodeID].InBand[TargetNodeID];
		P2PDownLoadTime += P2PPieceTime / 2.0;
	}


	//WaitingNodesを待つ時間
	CurrentWaitingNodes = Nodes[TargetNodeID].WaitingNodes;
	while (CurrentWaitingNodes) {
		P2PPieceTime = 8.0 * PieceSize / Nodes[CurrentWaitingNodes->NodeID].InBand[TargetNodeID];
		P2PDownLoadTime += P2PPieceTime;
		CurrentWaitingNodes = CurrentWaitingNodes->Next;
	}

	return P2PDownLoadTime;
}

double CalculateOverheadTime(int SourceNodeID, int TargetNodeID, int InOverhead, int OutOverhead) {
	//TargetNodeIDからSourceNodeIDへ
	double OverheadTime = 0.0;

	OverheadTime += 8.0 * InOverhead / Nodes[SourceNodeID].AverageInBand;
	OverheadTime += 8.0 * OutOverhead / Nodes[TargetNodeID].AverageInBand;
	return OverheadTime;
}
void ConvergeChecker() {
	int i;
	double Average, Diff;

	PreviousNumOnNodes[NextPreviousNumOnNodes] = NumOnNodes;
	Average = 0.0;
	for (i = 0; i < CONVERGECHECK; i++) {
		Average += PreviousNumOnNodes[i];
	}
	Average /= CONVERGECHECK;
	Diff = Average * 0.01;
	if ((NumOnNodes <= Average + Diff) &&
		(NumOnNodes >= Average - Diff)) {
		ConvergeTime = TopEvent->Time;
	}
	NextPreviousNumOnNodes++;
	if (NextPreviousNumOnNodes == CONVERGECHECK) {
		NextPreviousNumOnNodes = 0;
	}
}
void FinishReceiveConstantPlayOff(double Time, int NodeID) {
	double FinishOverheadTime;
	double FinishTime;

	FinishTime = Nodes[NodeID].RequestOnTime + Nodes[NodeID].SumInterruptDuration + Duration + PlayOffTime;
	if (Time > FinishTime) {
		printf("FinishREceivePlayOff Error¥n");
	}

	FinishOverheadTime = CalculateOverheadTime(0, NodeID, CONNECTIONOVERHEAD, CONNECTIONOVERHEAD);
	AddEvent(FinishTime + FinishOverheadTime, OFFEVENT, NodeID, 0, 0);
	if (NodeID == 0) {
		NodeID = 0;
	}
}
int FindRequestPiece(double Time, int SourceNodeID, int TargetNodeID, double* TmpDownLoadTime) {
	int i, j, Piece, TmpPiece;
	char c;
	double P2PDownLoadTime, PiecePlayTime, BroDownLoadTime;

	Piece = 0;
	PiecePlayTime = 8.0 * PieceSize / BitRate;
	P2PDownLoadTime = CalculateP2PDownLoadTime(SourceNodeID, TargetNodeID);
	*TmpDownLoadTime = P2PDownLoadTime;

	for (i = Nodes[SourceNodeID].NextContinuousPieceG; i < NumPieceGs; i++) {
		c = Nodes[SourceNodeID].PieceGs[i];
		c = (c) & (Nodes[TargetNodeID].PieceGs[i]);
		if (c) {
			for (j = 1; j <= 8; j++) {
				for (; j <= 8; j++) {
					if ((c & 0x01) == 1)
						break;
					c >>= 1;
				}
				if (j == 9)
					break;
				TmpPiece = 8 * i + j;
				BroDownLoadTime = CalculateBroDownLoadTime(SourceNodeID, Time, TmpPiece);
				//				BroDownLoadTime=1.0e32;
				if (P2PDownLoadTime < 1.0e32) {
					if ((BroDownLoadTime + Time > Nodes[SourceNodeID].RequestOnTime + PiecePlayTime * (TmpPiece - 1) + Nodes[SourceNodeID].SumInterruptDuration)
						&& (P2PDownLoadTime < BroDownLoadTime)
						//追加
						//これあるとダメ	&&(P2PDownLoadTime+Time>Nodes[SourceNodeID].RequestOnTime+PiecePlayTime*(TmpPiece-1)+Nodes[SourceNodeID].SumInterruptDuration)
						//追加ここまで
						) {
						Piece = TmpPiece;
						break;
					}
				}
				else {
					Piece = TmpPiece;
					//2018.11.27
					break;
				}
				c >>= 1;
			}
			if (j != 9)
				break;
		}
	}

	return Piece;
}

void ProceedRequest(double Time, int SourceNodeID, int TargetNodeID, int Piece) {
	//SourceNodeIDからTargetNodeIDへ
	double FinishTime;
	double r, Bandwidth;
	Nodes[SourceNodeID].State |= SENDSTATE;
	Nodes[SourceNodeID].SendingNodeID = TargetNodeID;
	Nodes[TargetNodeID].State |= RECEIVESTATE;

	//Bandwidth=Nodes[TargetNodeID].InBand[SourceNodeID];
	Bandwidth = Nodes[TargetNodeID].InBand[0];
	r = (double)rand() / RAND_MAX;
	Bandwidth *= (1.0 + BandwidthWaver * (2.0 * r - 1.0));

	FinishTime = Time + 8.0 * PieceSize / Bandwidth;
	AddEvent(FinishTime, FINISHREQUESTEVENT, SourceNodeID, TargetNodeID, Piece);
}

void FinishReceivePiece(double Time, int NodeID) {
	int NextNodeID, i;


	NumReceivedNodes++;
	Nodes[NodeID].State |= RECEIVEDSTATE;
	NextNodeID = Nodes[NodeID].NextNodeID;
	if (NextNodeID != -1) {
		Nodes[NextNodeID].PreviousNodeID = Nodes[NodeID].PreviousNodeID;
	}
	Nodes[NodeID].NextNodeID = -1;
	i = Nodes[NodeID].PreviousNodeID;
	if (i != -1) {
		Nodes[i].NextNodeID = NextNodeID;
	}
	Nodes[NodeID].PreviousNodeID = -1;

	if (ConvergeTime > 1.0) {
		AverageInterruptNodes++;
		AverageInterruptDuration += Nodes[NodeID].SumInterruptDuration;
		AverageNumInterrupt += Nodes[NodeID].NumInterrupt;
		if (MaximumInterruptDuration < Nodes[NodeID].SumInterruptDuration)
			MaximumInterruptDuration = Nodes[NodeID].SumInterruptDuration;
		if (Nodes[NodeID].SumInterruptDuration < MinimumInterruptDuration)
			MinimumInterruptDuration = Nodes[NodeID].SumInterruptDuration;
	}
}
void FinishReceive(double Time, int NodeID) {
	FinishReceiveConstantPlayOff(Time, NodeID);
}

void ProceedWaiting(double Time, int NodeID) {
	struct nodelist* CurrentNodeList;
	if (Nodes[NodeID].WaitingNodes) {
		Nodes[Nodes[NodeID].WaitingNodes->NodeID].State &= (WAITSTATE);
		ProceedRequest(Time, NodeID, Nodes[NodeID].WaitingNodes->NodeID, Nodes[NodeID].WaitingNodes->Piece);
		CurrentNodeList = Nodes[NodeID].WaitingNodes;
		Nodes[NodeID].WaitingNodes = CurrentNodeList->Next;
		delete CurrentNodeList;
	}
}
void ExecuteFetchRequestEvent() {
	struct nodelist* WaitNodeList;
	struct nodelist* CurrentNodeList;

	Nodes[TopEvent->Data1].State &= (REQUESTSTATE);

	if ((Nodes[TopEvent->NodeID].State & ONSTATE) == 0) {
		Nodes[TopEvent->Data1].State |= RETRYSTATE;
		Nodes[TopEvent->Data1].ReceivingNodeID = -1;
		Nodes[TopEvent->Data1].ReceivingPiece = 0;
		AddEvent(TopEvent->Time + RETRYCYCLE(TopEvent->Data1), RETRYEVENT, TopEvent->Data1, 0, 0);
		return;
	}

	if (SENDSTATE & (Nodes[TopEvent->NodeID].State)) {
		Nodes[TopEvent->Data1].State |= WAITSTATE;
		WaitNodeList = new struct nodelist;
		WaitNodeList->NodeID = TopEvent->Data1;
		WaitNodeList->Piece = TopEvent->Data2;
		WaitNodeList->Next = NULL;
		CurrentNodeList = Nodes[TopEvent->NodeID].WaitingNodes;
		if (TopEvent->Time > 399.5)
			TopEvent->Time = TopEvent->Time;
		if (CurrentNodeList) {
			while (CurrentNodeList->Next) {
				CurrentNodeList = CurrentNodeList->Next;
			}
			CurrentNodeList->Next = WaitNodeList;
		}
		else {
			Nodes[TopEvent->NodeID].WaitingNodes = WaitNodeList;
		}
	}
	else {
		ProceedRequest(TopEvent->Time, TopEvent->NodeID, TopEvent->Data1, TopEvent->Data2);
	}
}
void ExecuteAfterOnEvent() {
}
void InitializeBandwidth() {
	//NumSimulateNodesいる．
	int i, j;

	printf("InitializingBandwidth...");

	srand(Seed);

	if (NumSimulateNodes == 0) {
		NumSimulateNodes = NNODE - NumServers - 1;
	}
	if (NumServers + NumSimulateNodes > NNODE) {
		printf("Exceeds NNODE¥n");
	}
	//帯域初期化
	for (i = 0; i < NumServers; i++) {
		Nodes[i].AverageInBand = ServerBandwidth;
		for (j = NumServers; j < NumServers + NumSimulateNodes; j++) {
			Nodes[j].AverageInBand = ServerBandwidth;
			//			Nodes[j].InBand[i]=ServerBandwidth;
			Nodes[j].InBand[0] = ServerBandwidth;
		}
	}
	/*
			Nodes[i].AverageInBand=0.0;
			for(j=0;j<i;j++){
				do{
	//				d=AverageBandwidth+VarianceBandwidth*NormalRand();
					r=(double)rand()/RAND_MAX;
					if(r<0.8){
						if(r<0.022522523)d=300;
						else if(r<0.029279279)d=302.6953872;
						else if(r<0.047297297)d=305.4543622;
						else if(r<0.063063063)d=308.2784248;
						else if(r<0.078828829)d=311.1691108;
						else if(r<0.096846847)d=314.1279918;
						else if(r<0.119369369)d=317.1566766;
						else if(r<0.128378378)d=320.256812;
						else if(r<0.148648649)d=323.4300836;
						else if(r<0.166666667)d=326.6782168;
						else if(r<0.182432432)d=330.0029776;
						else if(r<0.189189189)d=333.4061739;
						else if(r<0.191441441)d=336.8896559;
						else if(r<0.198198198)d=340.4553178;
						else if(r<0.209459459)d=344.1050982;
						else if(r<0.218468468)d=347.8409817;
						else if(r<0.220720721)d=351.6649994;
						else if(r<0.22972973)d=355.5792307;
						else if(r<0.247747748)d=359.5858038;
						else if(r<0.254504505)d=363.6868971;
						else if(r<0.263513514)d=367.8847405;
						else if(r<0.277027027)d=372.1816164;
						else if(r<0.290540541)d=376.5798612;
						else if(r<0.296499685)d=381.0818663;
						else if(r<0.303984306)d=385.6900795;
						else if(r<0.313063063)d=390.4070063;
						else if(r<0.333333333)d=395.2352116;
						else if(r<0.346846847)d=400.1773205;
						else if(r<0.355855856)d=405.2360201;
						else if(r<0.364864865)d=410.4140609;
						else if(r<0.378378378)d=415.7142585;
						else if(r<0.38963964)d=421.1394945;
						else if(r<0.394144144)d=426.6927188;
						else if(r<0.407657658)d=432.3769509;
						else if(r<0.43018018)d=438.1952814;
						else if(r<0.439189189)d=444.1508738;
						else if(r<0.441441441)d=450.2469663;
						else if(r<0.454954955)d=456.4868735;
						else if(r<0.461711712)d=462.8739882;
						else if(r<0.466216216)d=469.4117831;
						else if(r<0.470720721)d=476.1038131;
						else if(r<0.472972973)d=482.9537167;
						else if(r<0.484234234)d=489.9652184;
						else if(r<0.488738739)d=497.1421304;
						else if(r<0.504504505)d=504.4883551;
						else if(r<0.504504505)d=512.0078867;
						else if(r<0.513513514)d=519.7048138;
						else if(r<0.518018018)d=527.5833213;
						else if(r<0.527027027)d=535.647693;
						else if(r<0.545045045)d=543.9023136;
						else if(r<0.554054054)d=552.3516714;
						else if(r<0.565315315)d=561.0003604;
						else if(r<0.576576577)d=569.8530832;
						else if(r<0.59009009)d=578.9146532;
						else if(r<0.605855856)d=588.1899973;
						else if(r<0.614864865)d=597.6841587;
						else if(r<0.617117117)d=607.4022997;
						else if(r<0.626126126)d=617.3497041;
						else if(r<0.635135135)d=627.5317807;
						else if(r<0.648648649)d=637.9540656;
						else if(r<0.657657658)d=648.6222257;
						else if(r<0.671171171)d=659.5420614;
						else if(r<0.675675676)d=670.7195102;
						else if(r<0.684684685)d=682.1606494;
						else if(r<0.68691068)d=693.8716998;
						else if(r<0.689189189)d=705.8590291;
						else if(r<0.694754177)d=718.129155;
						else if(r<0.70045045)d=730.6887489;
						else if(r<0.704954955)d=743.5446399;
						else if(r<0.713893672)d=756.703818;
						else if(r<0.719207595)d=770.1734381;
						else if(r<0.720720721)d=783.9608239;
						else if(r<0.725225225)d=798.0734719;
						else if(r<0.731981982)d=812.5190555;
						else if(r<0.734234234)d=827.3054291;
						else if(r<0.740193379)d=842.4406324;
						else if(r<0.743793254)d=857.9328946;
						else if(r<0.744919227)d=873.7906393;
						else if(r<0.746234622)d=890.0224887;
						else if(r<0.747747748)d=906.6372684;
						else if(r<0.752252252)d=923.6440122;
						else if(r<0.756756757)d=941.051967;
						else if(r<0.759009009)d=958.870598;
						else if(r<0.761261261)d=977.1095935;
						else if(r<0.768018018)d=995.7788704;
						else if(r<0.77027027)d=1014.88858;
						else if(r<0.779279279)d=1034.449112;
						else if(r<0.786036036)d=1054.471102;
						else if(r<0.788288288)d=1074.965437;
						else if(r<0.789778074)d=1095.943259;
						else if(r<0.791279667)d=1117.415976;
						else if(r<0.792792793)d=1139.395262;
						else d=1161.893067;
					}else{
						if(r<0.806306306)d=1184.921625;
						else if(r<0.809285879)d=1208.493456;
						else if(r<0.811549937)d=1232.621377;
						else if(r<0.813063063)d=1257.318507;
						else if(r<0.822072072)d=1282.598274;
						else if(r<0.826576577)d=1308.474423;
						else if(r<0.831081081)d=1334.961024;
						else if(r<0.837837838)d=1362.072479;
						else if(r<0.84009009)d=1389.823527;
						else if(r<0.844594595)d=1418.229259;
						else if(r<0.846084381)d=1447.305118;
						else if(r<0.847585973)d=1477.066915;
						else if(r<0.849099099)d=1507.530831;
						else if(r<0.852078671)d=1538.713429;
						else if(r<0.85434273)d=1570.631666;
						else if(r<0.855855856)d=1603.302895;
						else if(r<0.862612613)d=1636.744881;
						else if(r<0.867117117)d=1670.975806;
						else if(r<0.869369369)d=1706.014283;
						else if(r<0.870859156)d=1741.879363;
						else if(r<0.873839)d=1778.590546;
						else if(r<0.878378378)d=1816.167794;
						else if(r<0.885135135)d=1854.631537;
						else if(r<0.887387387)d=1894.00269;
						else if(r<0.88963964)d=1934.302659;
						else if(r<0.894144144)d=1975.553356;
						else if(r<0.898648649)d=2017.777211;
						else if(r<0.903153153)d=2060.997181;
						else if(r<0.906132726)d=2105.236766;
						else if(r<0.908207669)d=2150.520019;
						else if(r<0.909333642)d=2196.871564;
						else if(r<0.910649036)d=2244.3166;
						else if(r<0.912162162)d=2292.880927;
						else if(r<0.913651948)d=2342.590949;
						else if(r<0.916631793)d=2393.473694;
						else if(r<0.921171171)d=2445.556829;
						else if(r<0.924150744)d=2498.868673;
						else if(r<0.926225687)d=2553.438212;
						else if(r<0.92735166)d=2609.295116;
						else if(r<0.928667054)d=2666.469757;
						else if(r<0.93018018)d=2724.993222;
						else if(r<0.932432432)d=2784.897331;
						else if(r<0.934684685)d=2846.214655;
						else if(r<0.936936937)d=2908.978534;
						else if(r<0.939189189)d=2973.223094;
						else if(r<0.941441441)d=3038.983266;
						else if(r<0.943693694)d=3106.294806;
						else if(r<0.944123386)d=3175.194311;
						else if(r<0.944563216)d=3245.719245;
						else if(r<0.945013421)d=3317.907953;
						else if(r<0.945474248)d=3391.799686;
						else if(r<0.945945946)d=3467.434621;
						else if(r<0.946375638)d=3544.853881;
						else if(r<0.946815468)d=3624.099562;
						else if(r<0.947265674)d=3705.21475;
						else if(r<0.9477265)d=3788.243551;
						else if(r<0.948198198)d=3873.231108;
						else if(r<0.949687984)d=3960.223631;
						else if(r<0.951189577)d=4049.26842;
						else if(r<0.952702703)d=4140.413891;
						else if(r<0.953327017)d=4233.709601;
						else if(r<0.953966061)d=4329.206278;
						else if(r<0.95462018)d=4426.955844;
						else if(r<0.955115478)d=4527.011449;
						else if(r<0.955444097)d=4629.427495;
						else if(r<0.95578047)d=4734.259668;
						else if(r<0.956124777)d=4841.564967;
						else if(r<0.956477208)d=4951.401736;
						else if(r<0.956837952)d=5063.829697;
						else if(r<0.957207207)d=5178.909979;
						else if(r<0.957750732)d=5296.705153;
						else if(r<0.95830708)d=5417.279267;
						else if(r<0.958876552)d=5540.69788;
						else if(r<0.959459459)d=5667.028098;
						else if(r<0.960949246)d=5796.338609;
						else if(r<0.962261723)d=5928.699721;
						else if(r<0.963387696)d=6064.183404;
						else if(r<0.964401919)d=6202.863321;
						else if(r<0.964905707)d=6344.814877;
						else if(r<0.966216216)d=6490.115253;
						else if(r<0.968442211)d=6638.843452;
						else if(r<0.970720721)d=6791.080342;
						else if(r<0.972946716)d=6946.908697;
						else if(r<0.975225225)d=7106.413244;
						else if(r<0.97745122)d=7269.680709;
						else if(r<0.97972973)d=7436.799864;
						else if(r<0.981219516)d=7607.861576;
						else if(r<0.982721108)d=7782.958855;
						else if(r<0.984234234)d=7962.186905;
						else if(r<0.986434179)d=8145.643176;
						else if(r<0.988686023)d=8333.427417;
						else if(r<0.990990991)d=8525.641731;
						else if(r<0.991470943)d=8722.390629;
						else if(r<0.991615306)d=8923.781087;
						else if(r<0.991875108)d=9129.922606;
						else if(r<0.993191191)d=9340.92727;
						else if(r<0.993681198)d=9556.909807;
						else if(r<0.994577771)d=9777.98765;
						else if(r<0.995495495)d=10004.28101;
						else d=10235.91291;
					}


				}while(d<0.0);
				d*=1000.000;
	//			Band[(int)(d/10000)]++;
				Nodes[i].InBand[j]=d;
				Nodes[j].InBand[i]=d;
				Nodes[i].AverageInBand+=d;
				Nodes[j].AverageInBand+=d;
			}
		}
	*/

	printf("Done¥n");


}
char FillBits(int Bit) {
	char r;
	r = 0;
	switch (Bit) {
	case 0:r = (char)0x00; break;
	case 1:r = (char)0x01; break;
	case 2:r = (char)0x03; break;
	case 3:r = (char)0x07; break;
	case 4:r = (char)0x0F; break;
	case 5:r = (char)0x1F; break;
	case 6:r = (char)0x3F; break;
	case 7:r = (char)0x7F; break;
	case 8:r = -1; break;//(char)0xFF to avoid warning;
	}
	return r;
}
void FillPieces(int Start, int Stop) {
	int i, j, StartG, StopG;
	char StartB, StopB;

	if (Start > Stop)
		return;

	StartG = (Start - 1) / 8;
	StartB = (char)(Start - 8 * StartG - 1);
	StopG = (Stop - 1) / 8;
	StopB = (char)(Stop - 8 * StopG - 1);
	if (StartG == StopG) {
		StartB = FillBits(StartB);
		StopB = FillBits(StopB + 1);
		StartB = StartB ^ StopB;
		StopB = StartB;
	}
	else {
		StartB = FillBits(StartB);
		StopB = FillBits(StopB + 1);
	}
	for (i = NumServers; i < NumServers + NumSimulateNodes; i++) {
		Nodes[i].PieceGs[StartG] = StartB;
		for (j = StartG + 1; j < StopG; j++) {
			Nodes[i].PieceGs[j] = -1;//(char)0xFF to avoid warning
		}
		Nodes[i].PieceGs[StopG] = StopB;
	}
}
void InitializePrefetch() {
	int prefetchedp, CurrentChannel, Start, Stop, NumPrefetch;
	double rPrefetch;

	if (BroMethod == 1) {
		prefetchedp = (int)(Duration * Prefetch * BitRate / 8.0 / PieceSize);
		FillPieces(1, prefetchedp);
		return;
	}

	Start = 1;
	NumPrefetch = 0;
	for (CurrentChannel = 0; CurrentChannel < NumChannels; CurrentChannel++) {
		Stop = Channels[CurrentChannel].StartPiece - 1;
		if (Stop == -1) {//チャネル放送なし
			break;
		}
		if (Stop < Start) {
			Start = Channels[CurrentChannel].FinishPiece + 1;
			continue;
		}
		NumPrefetch += Stop - Start + 1;

		FillPieces(Start, Stop);
		Start = Channels[CurrentChannel].FinishPiece + 1;
	}
	if (Start <= NumPieceGs * 8) {
		Stop = NumPieceGs * 8;
		NumPrefetch += Stop - Start + 1;

		FillPieces(Start, Stop);
	}
	rPrefetch = (double)NumPrefetch / NumPieceGs / 8;
	if ((rPrefetch < Prefetch - 1.0e-8) || (Prefetch + 1.0e-8 < rPrefetch)) {
		rPrefetch = Prefetch;
	}
}
void Initialize() {
	int i, j;
	double d;
	struct event* NewEvent = NULL;

	printf("Initializing...");

	srand(Seed + 1);

	//到着イベント
	d = 0.0;
	TopEvent = NULL;
	NextEventNum = 0;
	for (NumSimulateNodes = NumServers;; NumSimulateNodes++) {
		if (TopEvent == NULL) {
			TopEvent = new struct event;
			NewEvent = TopEvent;
		}
		else {
			NewEvent->Next = new struct event;
			NewEvent = NewEvent->Next;
		}
		NewEvent->EventID = REQUESTONEVENT;
		NewEvent->EventNum = NextEventNum++;
		NewEvent->NodeID = NumSimulateNodes;
		NewEvent->Time = d;
		NewEvent->Data1 = 0;
		NewEvent->Data2 = 0;
		if (d > SimulationTime) {
			break;
		}
		d += RequestOnRand();
	}
	NewEvent->Next = NULL;

	NumPieceGs = (int)(BitRate * Duration / PieceSize / 8.0 / 8.0);
	if (NumPieceGs > NPIECEG) {
		printf("Exceeds PIECES");
	}
	else if (NumPieceGs < 1) {
		printf("Zero PIECES");
	}


	//トラッカ
	OnNodeList = NULL;
	for (i = 0; i < NumServers; i++) {
		if (OnNodeList == NULL) {
			OnNodeList = new struct nodelist;
			FinalOnNodeList = OnNodeList;
		}
		else {
			FinalOnNodeList->Next = new struct nodelist;
			FinalOnNodeList = FinalOnNodeList->Next;
		}
		FinalOnNodeList->Next = NULL;
		FinalOnNodeList->Previous = NULL;
		FinalOnNodeList->NodeID = i;

		for (j = 0; j < NumPieceGs; j++) {
			Nodes[i].PieceGs[j] = -1;//(char)0xFF to avoid warning
		}
		Nodes[i].State = ONSTATE | RECEIVEDSTATE;
		Nodes[i].WaitingNodes = NULL;
		Nodes[i].NumInterrupt = 0;
		Nodes[i].SumInterruptDuration = 0.0;
		Nodes[i].Interrupts = NULL;
		Nodes[i].NextContinuousPieceG = NumPieceGs;
		Nodes[i].SendingNodeID = -1;
		Nodes[i].ReceivingNodeID = -1;
		Nodes[i].ReceivingPiece = 0;
		Nodes[i].NextNodeID = -1;
		Nodes[i].PreviousNodeID = -1;
		Nodes[i].ReadyTime = 0.0;
	}

	//RequestOnTimeはExecuteRequestOnで処理される
	for (; i < NumSimulateNodes; i++) {
		for (j = 0; j < NumPieceGs; j++) {
			Nodes[i].PieceGs[j] = 0;
		}
		Nodes[i].State = OFFSTATE;
		Nodes[i].WaitingNodes = NULL;
		Nodes[i].NumInterrupt = 0;
		Nodes[i].SumInterruptDuration = 0.0;
		Nodes[i].Interrupts = NULL;
		Nodes[i].NextContinuousPieceG = 0;
		Nodes[i].SendingNodeID = -1;
		Nodes[i].ReceivingNodeID = -1;
		Nodes[i].ReceivingPiece = 0;
		Nodes[i].NextNodeID = -1;
		Nodes[i].PreviousNodeID = -1;
		Nodes[i].ReadyTime = 0.0;
	}

	ConvergeTime = 0.0;
	for (i = 0; i < CONVERGECHECK; i++) {
		PreviousNumOnNodes[i] = 0;
	}
	NextPreviousNumOnNodes = 0;

	NumP2PPieces = 0;
	NumBroPieces = 0;
	NumOnNodes = 0;//Onのノード数
	NumReceivedNodes = 0;//全部受信してOnのノード数
	NumOffNodes = 0;//全部受信してOffのノード数
	AverageInterruptNodes = 0;//収束してから全部受信したノード数
	AverageNumInterrupt = 0;
	AverageInterruptDuration = 0.0;//収束してから全部受信したノードの待ち時間合計
	MaximumInterruptDuration = 0.0;
	MinimumInterruptDuration = 1.0e32;
	for (i = 1; i <= NPIECEG * 8; i++) {
		CountPiece[i] = 0;
	}
	QueueNodeList = NULL;

#ifdef LOG
	char FileName[64];
	printf(FileName, 64, "log%d.txt", ChannelMethod);
	LogFile = myfopen(FileName, "w");
#endif

	srand(Seed + 2);

	printf("Done¥n");
}
void InitializeChannel() {
	double cp, p[NCHANNEL];//再生時間
	double ca, a[NCHANNEL];//データサイズ
	double cd, d[NCHANNEL];//放送時間
	long double ea, sp;
	int i, Order, PieceControl, CurrentChannel, ep;

	Order = 1000;
	PieceControl = 0;
	SubmitRequestLimitPiece = 0;
	Channels[0].StartPiece = 1;
	Channels[0].FinishPiece = 0;
	Channels[0].BroTime = 8.0 * PieceSize / Channels[0].Bandwidth;
	Channels[0].NextPiece = 1;
	for (i = 0; i < 10; i++) {
		PieceControl += Order;
		ca = (double)PieceSize * PieceControl;
		cp = 8.0 * ca / BitRate;
		cd = 8.0 * ca / ServerBandwidth;
		sp = cp;
		//d[0]を決めておく
		if (ServerBandwidth < 1.0e-8) {//放送のみ
			d[0] = 8.0 * ca / Channels[0].Bandwidth;
			sp = 0.0;
		}
		else if (ServerBandwidth < BitRate) {
			SubmitRequestLimitPiece = PieceControl;
			d[0] = cd + 8.0 * PieceSize / BitRate;
		}
		else {
			SubmitRequestLimitPiece = PieceControl;
			d[0] = 8.0 * PieceSize / ServerBandwidth + cp;
		}
		for (CurrentChannel = 0; CurrentChannel < NumChannels; CurrentChannel++) {
			ea = Channels[CurrentChannel].Bandwidth * d[CurrentChannel] / 8.0;
			ep = (int)(ea / PieceSize);
			a[CurrentChannel] = (double)PieceSize * ep;
			p[CurrentChannel] = 8.0 * a[CurrentChannel] / BitRate;
			sp += p[CurrentChannel];
			if (ep == 0) {
				Channels[CurrentChannel].StartPiece = 0;
				Channels[CurrentChannel].FinishPiece = 0;
			}
			else {
				if (CurrentChannel == 0)
					Channels[CurrentChannel].StartPiece = SubmitRequestLimitPiece + 1;
				else
					Channels[CurrentChannel].StartPiece = Channels[CurrentChannel - 1].FinishPiece + 1;
				Channels[CurrentChannel].FinishPiece = Channels[CurrentChannel].StartPiece + ep - 1;
			}
			Channels[CurrentChannel].BroTime = 8.0 * PieceSize / Channels[CurrentChannel].Bandwidth;
			Channels[CurrentChannel].Interval = ep * Channels[CurrentChannel].BroTime;
			Channels[CurrentChannel].NextPiece = Channels[CurrentChannel].StartPiece;
			Channels[CurrentChannel].CreateNodeID = 0;
			//次の準備，d[CurrentChannel+1]を決めておく
			d[CurrentChannel + 1] = d[CurrentChannel] + p[CurrentChannel];
		}
		if (sp > Duration - 1.0e-8) {
			if (Order == 1)	break;
			else {
				PieceControl -= Order;
				i = -1;
				Order /= 10;
			}
		}
	}
	//最終調整
	for (CurrentChannel = 0; CurrentChannel < NumChannels; CurrentChannel++) {
		if (Channels[CurrentChannel].StartPiece != 0) {
			Channels[CurrentChannel].StartTime = 0.0;
			AddEvent(Channels[CurrentChannel].BroTime, BROFINISHEVENT, -1, CurrentChannel, Channels[CurrentChannel].NextPiece);
			if (Channels[CurrentChannel].FinishPiece > NumPieceGs * 8) {
				if (Channels[CurrentChannel].StartPiece > NumPieceGs * 8)	return;
				Channels[CurrentChannel].FinishPiece = NumPieceGs * 8;
				Channels[CurrentChannel].Interval = (Channels[CurrentChannel].FinishPiece - Channels[CurrentChannel].StartPiece + 1) * Channels[CurrentChannel].BroTime;
				for (CurrentChannel++; CurrentChannel < NumChannels; CurrentChannel++) {
					Channels[CurrentChannel].StartPiece = 0;
					Channels[CurrentChannel].FinishPiece = 0;
					Channels[CurrentChannel].NextPiece = 0;
					Channels[CurrentChannel].Interval = 0.0;
				}
			}
		}
	}
	SequentialMode = 0;
}


int FindCountinuouslyBlankFinishPiece(int NodeID, int StartPiece) {
	int i, j, Piece;
	char c;

	Piece = StartPiece - 1;

	for (i = (StartPiece - 1) / 8; i < NumPieceGs; i++) {
		c = Nodes[NodeID].PieceGs[i];
		if (c) {
			for (j = 1; j <= 8; j++) {
				Piece = 8 * i + j;
				if (((c & 0x01) == 1) && (Piece > StartPiece))
					break;
				c >>= 1;
			}
			if (j != 9)
				break;
		}
	}
	if (i == NumPieceGs)
		Piece = NumPieceGs * 8 + 1;

	return Piece - 1;
}
int FindNextContinuouslyBlankPiece(int NodeID, int StartPiece) {
	int i, j, Piece;
	char c;

	Piece = NumPieceGs * 8 + 1;

	for (i = (StartPiece - 1) / 8; i < NumPieceGs; i++) {
		c = Nodes[NodeID].PieceGs[i];
		if (c != -1) {//(char)0xFF to avoid warning
			for (j = 1; j <= 8; j++) {
				Piece = 8 * i + j;
				if (((c & 0x01) == 0) && (Piece >= StartPiece))
					break;
				c >>= 1;
			}
			if (j != 9)
				break;
		}
	}
	if (i == NumPieceGs)
		return 0;

	return Piece;
}

int CheckChannelCaru(double SubmitRequestOverheadTime, int NodeID, int Piece) {
	int i, From, Scheduled, CurrentChannel;

	Scheduled = 0;
	if (Piece == 0) {//放送終了
		CurrentChannel = TopEvent->Data1;
		if (Channels[CurrentChannel].NextPiece == NumPieceGs * 8)
			From = 1;
		else
			From = Channels[CurrentChannel].NextPiece + 1;
		Channels[CurrentChannel].NextPiece = From;
		Channels[CurrentChannel].StartPiece = From;
		Channels[CurrentChannel].FinishPiece = From;
		Channels[CurrentChannel].StartTime = TopEvent->Time;
		AddEvent(TopEvent->Time + Channels[CurrentChannel].BroTime, BROFINISHEVENT, -1, CurrentChannel, Channels[CurrentChannel].NextPiece);
	}
	else {//再生端末から要求来た
		for (i = 0; i < NumChannels; i++) {
			if (Channels[i].StartPiece == 0) {
				From = 1;
				Channels[i].StartPiece = From;
				Channels[i].FinishPiece = From;
				Channels[i].NextPiece = From;
				Channels[i].Interval = Channels[i].BroTime;
				Channels[i].CreateNodeID = NodeID;
				Channels[i].StartTime = TopEvent->Time + SubmitRequestOverheadTime;
				AddEvent(TopEvent->Time + Channels[i].BroTime + SubmitRequestOverheadTime, BROFINISHEVENT, -1, i, Channels[i].NextPiece);
				/*これ入れると性能わるくなる？
								if(Piece!=0){//通信受信続けるため
									SubmitRequest(TopEvent->Time,NodeID);
								}
								Scheduled=1;
				*/
				break;
			}
		}
	}
	return Scheduled;

}
int CheckChannelNone(double SubmitRequestOverhead, int NodeID, int Piece) {
	return 0;
}

int CheckChannelBCD(double SubmitRequestOverhead, int NodeID, int Piece) {
	double p[NCHANNEL];//再生時間
	double a[NCHANNEL];//データサイズ
	double d[NCHANNEL];//放送時間
	double e[NCHANNEL];//余分時間
	long double ea, sp;
	int i, Order, CurrentChannel, ep, Scheduled;

	Scheduled = 0;
	if (Piece == 0) {//放送終了
		CurrentChannel = TopEvent->Data1;
		Channels[CurrentChannel].NextPiece = Channels[CurrentChannel].StartPiece;
		Channels[CurrentChannel].StartTime = TopEvent->Time;
		AddEvent(TopEvent->Time + Channels[CurrentChannel].BroTime, BROFINISHEVENT, -1, CurrentChannel, Channels[CurrentChannel].NextPiece);
	}
	else if (Channels[0].StartPiece == 0) {
		Order = 1000;
		//	Order=10;ChannelP2PBandwidth=105000000.0;
		Channels[0].StartPiece = 1;
		Channels[0].FinishPiece = 0;
		Channels[0].BroTime = 8.0 * PieceSize / Channels[0].Bandwidth;
		Channels[0].NextPiece = 1;
		for (i = 0; i < 10; i++) {
			Channels[0].FinishPiece += Order;
			a[0] = (double)PieceSize * Channels[0].FinishPiece;
			p[0] = 8.0 * a[0] / BitRate;
			d[0] = Channels[0].BroTime * Channels[0].FinishPiece;
			Channels[0].Interval = d[0];
			sp = p[0];

			//d[1]を決めておく
			if (Channels[0].Bandwidth < BitRate) {
				d[1] = d[0] + 8.0 * PieceSize / BitRate;
			}
			else {
				d[1] = 8.0 * PieceSize / Channels[0].Bandwidth + p[0];
			}
			e[1] = ChannelP2PBandwidth * d[1] / BitRate;
			if (Duration - sp < e[1]) {
				e[1] = (double)(Duration - sp);
			}if (e[1] < 0.0) {//ここに来るとエラーチェックすること
				e[1] = 0.0;
			}
			d[1] += e[1];
			for (CurrentChannel = 1; CurrentChannel < NumChannels; CurrentChannel++) {
				ea = Channels[CurrentChannel].Bandwidth * d[CurrentChannel] / 8.0;
				ep = (int)(ea / PieceSize);
				a[CurrentChannel] = (double)PieceSize * ep;
				p[CurrentChannel] = 8.0 * a[CurrentChannel] / BitRate;
				sp += p[CurrentChannel];
				Channels[CurrentChannel].StartPiece = Channels[CurrentChannel - 1].FinishPiece + 1;
				Channels[CurrentChannel].FinishPiece = Channels[CurrentChannel].StartPiece + ep - 1;
				Channels[CurrentChannel].BroTime = 8.0 * PieceSize / Channels[CurrentChannel].Bandwidth;
				Channels[CurrentChannel].Interval = ep * Channels[CurrentChannel].BroTime;
				Channels[CurrentChannel].NextPiece = Channels[CurrentChannel].StartPiece;

				//次の準備，d[CurrentChannel+1]を決めておく
				e[CurrentChannel + 1] = ChannelP2PBandwidth * p[CurrentChannel] / BitRate;
				if (Duration - sp < e[CurrentChannel + 1]) {
					e[CurrentChannel + 1] = (double)(Duration - sp);//ここに来るとエラーチェックすること
				}
				if (e[CurrentChannel + 1] < 0.0) {
					e[CurrentChannel + 1] = 0.0;
				}
				d[CurrentChannel + 1] = d[CurrentChannel] + p[CurrentChannel] + e[CurrentChannel + 1];
			}
			if (sp > Duration - 1.0e-8) {
				if (Order == 1) {
					break;
				}
				else {
					Channels[0].FinishPiece -= Order;
					i = -1;
					Order /= 10;
				}
			}
		}
		//最終調整
		for (CurrentChannel = 0; CurrentChannel < NumChannels; CurrentChannel++) {
			Channels[CurrentChannel].StartTime = TopEvent->Time + SubmitRequestOverhead;
			AddEvent(TopEvent->Time + Channels[CurrentChannel].BroTime, BROFINISHEVENT, -1, CurrentChannel, Channels[CurrentChannel].NextPiece);
			if (Channels[CurrentChannel].FinishPiece > NumPieceGs * 8) {
				if (Channels[CurrentChannel].StartPiece > NumPieceGs * 8)
					return false;
				Channels[CurrentChannel].FinishPiece = NumPieceGs * 8;
				Channels[CurrentChannel].Interval = (Channels[CurrentChannel].FinishPiece - Channels[CurrentChannel].StartPiece + 1) * Channels[CurrentChannel].BroTime;
				for (CurrentChannel++; CurrentChannel < NumChannels; CurrentChannel++) {
					Channels[CurrentChannel].StartPiece = 0;
					Channels[CurrentChannel].FinishPiece = 0;
					Channels[CurrentChannel].NextPiece = 0;
					Channels[CurrentChannel].Interval = 0.0;
				}
			}
		}
		//Scheduled=1;
	}
	return Scheduled;

}
int CheckChannelShareHybrid(double SubmitRequestOverhead, int NodeID, int Piece) {
	int CurrentChannel, Scheduled;

	Scheduled = 0;
	if (Piece == 0) {//放送終了
		CurrentChannel = TopEvent->Data1;
		Channels[CurrentChannel].NextPiece = Channels[CurrentChannel].StartPiece;
		Channels[CurrentChannel].StartTime = TopEvent->Time;
		AddEvent(TopEvent->Time + Channels[CurrentChannel].BroTime, BROFINISHEVENT, -1, CurrentChannel, Channels[CurrentChannel].NextPiece);
	}
	return Scheduled;

}
int CheckChannel(double SubmitRequestOverhead, int NodeID, int Piece) {
	int r = 0;
	switch (ChannelMethod) {
	case 0:
		r = CheckChannelShareHybrid(SubmitRequestOverhead, NodeID, Piece);
		break;
	case 1:
		r = CheckChannelNone(SubmitRequestOverhead, NodeID, Piece);
		break;
	}
	return r;
}
void SubmitRequest(double Time, int NodeID) {

	int TargetNodeID, TmpTargetNodeID, TmpPiece, Piece;
	double TmpDownLoadTime, DownLoadTime, SubmitRequestOverheadTime;

	if (Nodes[NodeID].RequestOnTime + SubmitRequestLimitTime < Time) {
		return;
	}
	SubmitRequestOverheadTime = 0.0;
	Piece = NumPieceGs * 8 + 1;
	TmpPiece = 0;
	TargetNodeID = -1;
	DownLoadTime = 1.0e32;
	for (TmpTargetNodeID = 0; TmpTargetNodeID < NumServers; TmpTargetNodeID++) {
		TmpPiece = FindRequestPiece(Time, NodeID, TmpTargetNodeID, &TmpDownLoadTime);
		if (TmpPiece) {
			if (SubmitRequestLimitPiece < TmpPiece) {
				continue;
			}
			if (TmpDownLoadTime < DownLoadTime) {
				Piece = TmpPiece;
				TargetNodeID = TmpTargetNodeID;
				DownLoadTime = TmpDownLoadTime;
			}
			else if (TmpDownLoadTime == DownLoadTime) {
				if (TmpPiece < Piece) {
					Piece = TmpPiece;
					TargetNodeID = TmpTargetNodeID;
					DownLoadTime = TmpDownLoadTime;
				}
			}
			SubmitRequestOverheadTime += CalculateOverheadTime(NodeID, TmpTargetNodeID, 1, 1);
		}
	}

	if (TargetNodeID == -1) {
		//		Nodes[NodeID].State|=RETRYSTATE;
		//		AddEvent(Time+SubmitRequestOverheadTime+RETRYCYCLE(NodeID),RETRYEVENT,NodeID,0,0);
		return;
	}

	if (CheckChannel(SubmitRequestOverheadTime, NodeID, Piece) == 0) {
		AddEvent(Time + SubmitRequestOverheadTime, FETCHREQUESTEVENT, TargetNodeID, NodeID, Piece);
		Nodes[NodeID].State |= REQUESTSTATE;
		Nodes[NodeID].ReceivingNodeID = TargetNodeID;
		Nodes[NodeID].ReceivingPiece = Piece;
	}
}

void ExecuteFinishRequestEvent() {
	int i, PieceG, PieceB;
	double PlayPosition, TimePlayPosition, CurrentTime;
	struct interrupt* CurrentInterrupt;

	Nodes[TopEvent->NodeID].State &= (SENDSTATE);
	Nodes[TopEvent->NodeID].SendingNodeID = -1;
	Nodes[TopEvent->Data1].State &= (RECEIVESTATE);
	Nodes[TopEvent->Data1].ReceivingNodeID = -1;
	Nodes[TopEvent->Data1].ReceivingPiece = 0;

	PieceG = (TopEvent->Data2 - 1) / 8;
	PieceB = TopEvent->Data2 - 8 * PieceG - 1;
	PieceB = 0x01 << PieceB;

	CurrentTime = TopEvent->Time;
	if (((Nodes[TopEvent->Data1].PieceGs[PieceG]) & PieceB) == 0) {
		(Nodes[TopEvent->Data1].PieceGs[PieceG]) |= PieceB;


		NumP2PPieces++;
		(CountPiece[TopEvent->Data2])++;

		PlayPosition = 8.0 * (TopEvent->Data2 - 1) * PieceSize / BitRate;
		TimePlayPosition = Nodes[TopEvent->Data1].RequestOnTime + PlayPosition + Nodes[TopEvent->Data1].SumInterruptDuration;

		if (IntentDelayPiece <= TopEvent->Data2)//意図的に待つ分
			TimePlayPosition += IntentDelay;
		if ((TopEvent->Data2 == IntentDelayPiece) && (CurrentTime <= TimePlayPosition)) {
			//意図的にまつ．したのinterruption処理は実行されない。
			Nodes[TopEvent->Data1].NumInterrupt++;
			Nodes[TopEvent->Data1].SumInterruptDuration += IntentDelay;
			CurrentInterrupt = new struct interrupt;
			CurrentInterrupt->StartTime = TimePlayPosition - IntentDelay;
			CurrentInterrupt->EndTime = TimePlayPosition;
			CurrentInterrupt->Piece = TopEvent->Data2;
			CurrentInterrupt->Next = Nodes[TopEvent->Data1].Interrupts;
			Nodes[TopEvent->Data1].Interrupts = CurrentInterrupt;
		}

		if (TimePlayPosition < CurrentTime) {
			Nodes[TopEvent->Data1].NumInterrupt++;
			Nodes[TopEvent->Data1].SumInterruptDuration += CurrentTime - TimePlayPosition;
			CurrentInterrupt = new struct interrupt;
			CurrentInterrupt->StartTime = TimePlayPosition;
			CurrentInterrupt->EndTime = CurrentTime;
			CurrentInterrupt->Piece = TopEvent->Data2;
			CurrentInterrupt->Next = Nodes[TopEvent->Data1].Interrupts;
			Nodes[TopEvent->Data1].Interrupts = CurrentInterrupt;
		}



		ProceedWaiting(TopEvent->Time, TopEvent->NodeID);

		for (i = Nodes[TopEvent->Data1].NextContinuousPieceG; i < NumPieceGs; i++) {
			if (Nodes[TopEvent->Data1].PieceGs[i] != -1) {//(char)0xFF to avoid warning
				SubmitRequest(TopEvent->Time, TopEvent->Data1);
				break;
			}
			else {
				(Nodes[TopEvent->Data1].NextContinuousPieceG)++;
			}
		}
		if (i == NumPieceGs) {
			//受信完了
			FinishReceivePiece(TopEvent->Time, TopEvent->Data1);
			FinishReceive(TopEvent->Time, TopEvent->Data1);
		}
	}
	else {
		printf("ExecuteFinishRequestEvent Error¥n");
	}
}

void ExecuteBroFinishEvent() {
	struct nodelist* CurrentNodeList;
	struct nodelist* WaitingNodeList;
	struct nodelist* PreWaitingNodeList;
	double CurrentTime;
	int i, CurrentChannel, Piece, PieceG, RequestNodeID;
	char PieceB;
	double PlayPosition, TimePlayPosition;
	struct interrupt* CurrentInterrupt;
	struct event* CurrentEvent;
	struct event* PreCurrentEvent;

	CurrentTime = TopEvent->Time;
	CurrentChannel = TopEvent->Data1;
	Piece = TopEvent->Data2;
	PieceG = (Piece - 1) / 8;
	PieceB = (char)(Piece - PieceG * 8);
	PieceB = 0x01 << (PieceB - 1);



	CurrentNodeList = OnNodeList;
	while (CurrentNodeList) {
		if ((Nodes[CurrentNodeList->NodeID].RequestOnTime <= CurrentTime - Channels[CurrentChannel].BroTime + 0.00000001)
			&& (((Nodes[CurrentNodeList->NodeID].PieceGs[PieceG]) & PieceB) == 0)) {

			NumBroPieces++;
			(CountPiece[Piece])++;
			Nodes[CurrentNodeList->NodeID].PieceGs[PieceG] |= PieceB;

			for (i = 0; i < NumPieceGs; i++) {
				if (Nodes[CurrentNodeList->NodeID].PieceGs[i] != -1) {//(char)0xFF to avoid warning
					Nodes[CurrentNodeList->NodeID].NextContinuousPieceG = i;
					break;
				}
			}
			if (i == NumPieceGs) {
				Nodes[CurrentNodeList->NodeID].NextContinuousPieceG = NumPieceGs;
			}

			PlayPosition = 8.0 * (Piece - 1) * PieceSize / BitRate;
			TimePlayPosition = Nodes[CurrentNodeList->NodeID].RequestOnTime + PlayPosition + Nodes[CurrentNodeList->NodeID].SumInterruptDuration;

			if (IntentDelayPiece <= Piece)//意図的に待つ分
				TimePlayPosition += IntentDelay;
			if ((Piece == IntentDelayPiece) && (CurrentTime <= TimePlayPosition)) {
				//意図的にまつ．したのinterruption処理は実行されない。
				Nodes[CurrentNodeList->NodeID].NumInterrupt++;
				Nodes[CurrentNodeList->NodeID].SumInterruptDuration += IntentDelay;
				CurrentInterrupt = new struct interrupt;
				CurrentInterrupt->StartTime = TimePlayPosition - IntentDelay;
				CurrentInterrupt->EndTime = TimePlayPosition;
				CurrentInterrupt->Piece = Piece;
				CurrentInterrupt->Next = Nodes[CurrentNodeList->NodeID].Interrupts;
				Nodes[CurrentNodeList->NodeID].Interrupts = CurrentInterrupt;
			}

			if (TimePlayPosition < CurrentTime) {
				Nodes[CurrentNodeList->NodeID].NumInterrupt++;
				Nodes[CurrentNodeList->NodeID].SumInterruptDuration += CurrentTime - TimePlayPosition;
				CurrentInterrupt = new struct interrupt;
				CurrentInterrupt->StartTime = TimePlayPosition;
				CurrentInterrupt->EndTime = CurrentTime;
				CurrentInterrupt->Piece = Piece;
				CurrentInterrupt->Next = Nodes[CurrentNodeList->NodeID].Interrupts;
				Nodes[CurrentNodeList->NodeID].Interrupts = CurrentInterrupt;
			}


			//Pieceを受信中だった
			if (Nodes[CurrentNodeList->NodeID].ReceivingPiece == Piece) {
				RequestNodeID = Nodes[CurrentNodeList->NodeID].ReceivingNodeID;
				//受信中
				if (Nodes[CurrentNodeList->NodeID].State & RECEIVESTATE) {
					PreCurrentEvent = TopEvent;
					CurrentEvent = TopEvent->Next;
					while (CurrentEvent) {
						if ((CurrentEvent->EventID == FINISHREQUESTEVENT) &&
							(CurrentEvent->NodeID == RequestNodeID)) {
							PreCurrentEvent->Next = CurrentEvent->Next;
							delete CurrentEvent;
							break;
						}
						PreCurrentEvent = CurrentEvent;
						CurrentEvent = CurrentEvent->Next;
					}
					Nodes[CurrentNodeList->NodeID].State &= (RECEIVESTATE);
					Nodes[CurrentNodeList->NodeID].ReceivingNodeID = -1;
					Nodes[CurrentNodeList->NodeID].ReceivingPiece = 0;
					Nodes[RequestNodeID].State &= (SENDSTATE);
					Nodes[RequestNodeID].SendingNodeID = -1;
					ProceedWaiting(CurrentTime, RequestNodeID);
				}
				//待ち中
				if (Nodes[CurrentNodeList->NodeID].State & WAITSTATE) {
					WaitingNodeList = Nodes[RequestNodeID].WaitingNodes;
					PreWaitingNodeList = NULL;
					while (WaitingNodeList) {
						if (WaitingNodeList->NodeID == CurrentNodeList->NodeID) {
							if (PreWaitingNodeList) {
								PreWaitingNodeList->Next = WaitingNodeList->Next;
							}
							else {
								Nodes[RequestNodeID].WaitingNodes = WaitingNodeList->Next;
							}
							delete WaitingNodeList;
							break;
						}
						PreWaitingNodeList = WaitingNodeList;
						WaitingNodeList = WaitingNodeList->Next;
					}
					Nodes[CurrentNodeList->NodeID].State &= (WAITSTATE);
					Nodes[CurrentNodeList->NodeID].ReceivingNodeID = -1;
					Nodes[CurrentNodeList->NodeID].ReceivingPiece = 0;
				}
				//要求中
				if (Nodes[CurrentNodeList->NodeID].State & REQUESTSTATE) {
					PreCurrentEvent = TopEvent;
					CurrentEvent = TopEvent->Next;
					while (CurrentEvent) {
						if ((CurrentEvent->Data1 == CurrentNodeList->NodeID) &&
							(CurrentEvent->EventID == FETCHREQUESTEVENT)) {
							PreCurrentEvent->Next = CurrentEvent->Next;
							delete CurrentEvent;
							break;
						}
						PreCurrentEvent = CurrentEvent;
						CurrentEvent = CurrentEvent->Next;
					}
					Nodes[CurrentNodeList->NodeID].State &= (REQUESTSTATE);
					Nodes[CurrentNodeList->NodeID].ReceivingNodeID = -1;
					Nodes[CurrentNodeList->NodeID].ReceivingPiece = 0;
				}
				//リトライ中
				if (Nodes[CurrentNodeList->NodeID].State & RETRYSTATE) {
					//ありえないはず
					Nodes[CurrentNodeList->NodeID].State &= (RETRYSTATE);
					PreCurrentEvent = TopEvent;
					CurrentEvent = TopEvent->Next;
					while (CurrentEvent) {
						if ((CurrentEvent->NodeID == CurrentNodeList->NodeID) &&
							(CurrentEvent->EventID == RETRYEVENT)) {
							PreCurrentEvent->Next = CurrentEvent->Next;
							delete CurrentEvent;
							break;
						}
						PreCurrentEvent = CurrentEvent;
						CurrentEvent = CurrentEvent->Next;
					}
				}
				if (Nodes[CurrentNodeList->NodeID].NextContinuousPieceG != NumPieceGs) {
					SubmitRequest(CurrentTime, CurrentNodeList->NodeID);
				}
			}
			if (Nodes[CurrentNodeList->NodeID].NextContinuousPieceG == NumPieceGs) {
				//受信完了
				if (CurrentNodeList->NodeID == 0)
					CurrentNodeList->NodeID = 0;
				FinishReceivePiece(CurrentTime, CurrentNodeList->NodeID);
				FinishReceive(CurrentTime, CurrentNodeList->NodeID);
			}

		}
		CurrentNodeList = CurrentNodeList->Next;
	}

	//Add next RequestOnEvent
	if (Channels[CurrentChannel].NextPiece == Channels[CurrentChannel].FinishPiece) {
		//		if(ChannelMethod==0)
		//			AddEvent(CurrentTime+16/Nodes[CurrentChannel].AverageInBand,BROSTARTEVENT,-1,CurrentChannel,Channels[CurrentChannel].CreateNodeID);
		//		else
		CheckChannel(0.0, Channels[CurrentChannel].CreateNodeID, 0);
	}
	else {
		Channels[CurrentChannel].NextPiece++;
		AddEvent(CurrentTime + Channels[CurrentChannel].BroTime, BROFINISHEVENT, -1, CurrentChannel, Channels[CurrentChannel].NextPiece);
	}
}
void ExecuteOffEvent() {
	struct nodelist* CurrentNodeList;
	struct nodelist* PreCurrentNodeList;
	struct event* CurrentEvent;
	struct event* PreCurrentEvent;

	Nodes[TopEvent->NodeID].State &= ONSTATE;
	Nodes[TopEvent->NodeID].SendingNodeID = -1;

	//Remove from OnNodeList. The top is server.
	CurrentNodeList = OnNodeList->Next;
	PreCurrentNodeList = OnNodeList;
	while (CurrentNodeList->NodeID != TopEvent->NodeID) {
		PreCurrentNodeList = CurrentNodeList;
		CurrentNodeList = CurrentNodeList->Next;
	}
	if (CurrentNodeList->Next == NULL) {
		FinalOnNodeList = PreCurrentNodeList;
	}
	PreCurrentNodeList->Next = CurrentNodeList->Next;
	delete CurrentNodeList;
	NumOnNodes--;
	NumReceivedNodes--;
	NumOffNodes++;

	//WaitingNodes
	CurrentNodeList = Nodes[TopEvent->NodeID].WaitingNodes;
	while (CurrentNodeList) {
		Nodes[CurrentNodeList->NodeID].State &= (WAITSTATE);
		Nodes[CurrentNodeList->NodeID].ReceivingNodeID = -1;
		Nodes[CurrentNodeList->NodeID].ReceivingPiece = 0;
		SubmitRequest(TopEvent->Time, CurrentNodeList->NodeID);
		CurrentNodeList = CurrentNodeList->Next;
	}
	//SendingNodes
	if (Nodes[TopEvent->NodeID].State & SENDSTATE) {
		Nodes[TopEvent->NodeID].State &= (SENDSTATE);
		Nodes[TopEvent->NodeID].SendingNodeID = -1;
		PreCurrentEvent = TopEvent;
		CurrentEvent = TopEvent->Next;
		while (CurrentEvent) {
			if ((CurrentEvent->EventID == FINISHREQUESTEVENT) &&
				(CurrentEvent->NodeID == TopEvent->NodeID)) {
				PreCurrentEvent->Next = CurrentEvent->Next;
				Nodes[CurrentEvent->Data1].State &= (RECEIVESTATE);
				Nodes[CurrentEvent->Data1].ReceivingNodeID = -1;
				Nodes[CurrentEvent->Data1].ReceivingPiece = 0;
				SubmitRequest(TopEvent->Time, CurrentEvent->Data1);
				delete CurrentEvent;
				break;
			}
			PreCurrentEvent = CurrentEvent;
			CurrentEvent = CurrentEvent->Next;
		}
	}
}

void ExecuteBroStartEvent() {
	CheckChannel(0.0, TopEvent->Data2, 0);
}

void ExecuteRetryEvent() {
	Nodes[TopEvent->NodeID].State &= (RETRYSTATE);
	SubmitRequest(TopEvent->Time, TopEvent->NodeID);
}
void ExecuteRequestOnEvent() {
	struct nodelist* CurrentNodeList;
	Nodes[TopEvent->NodeID].State |= ONSTATE;
	Nodes[TopEvent->NodeID].RequestOnTime = TopEvent->Time;

	//Add to OnNodeList
	CurrentNodeList = new struct nodelist;
	CurrentNodeList->NodeID = TopEvent->NodeID;
	CurrentNodeList->Next = NULL;
	if (OnNodeList == NULL) {
		OnNodeList = CurrentNodeList;
	}
	else {
		FinalOnNodeList->Next = CurrentNodeList;
	}
	FinalOnNodeList = CurrentNodeList;
	NumOnNodes++;

	if (ConvergeTime < 1.0) {
		ConvergeChecker();
	}

	SubmitRequest(TopEvent->Time, TopEvent->NodeID);
}
void EventParser() {
	switch (TopEvent->EventID) {
	case REQUESTONEVENT:

		//			fclose(LogFile);
		//			LogFile=myfopen("log.txt","a");
		printf("On:%lf¥t%d¥t%d¥t%d¥t%d¥t%d¥t%lf¥n", TopEvent->Time, TopEvent->NodeID, NumOnNodes, NumReceivedNodes, NumOffNodes, AverageInterruptNodes, AverageInterruptDuration / AverageInterruptNodes);
		//				printf("On:%lf¥t%d¥t%d¥t%d¥t%d¥t%lf¥t%lf¥n",TopEvent->Time,TopEvent->NodeID,NumOnNodes,NumReceivedNodes,NumOffNodes,TopEvent->Time/NumOffNodes,AverageInterruptDuration/AverageInterruptNodes);
		ExecuteRequestOnEvent();
		//ぜんぜん定常しなければ終了
		if ((ConvergeTime < 1.0) && (TopEvent->Time > 10.0 * Duration)) {
			return;
		}
		break;
	case AFTERONEVENT:
		ExecuteAfterOnEvent();
		break;
	case FETCHREQUESTEVENT:
		ExecuteFetchRequestEvent();
		break;
	case RETRYEVENT:
		ExecuteRetryEvent();
		break;
	case FINISHREQUESTEVENT:
		ExecuteFinishRequestEvent();
		break;
	case OFFEVENT:
		ExecuteOffEvent();
		break;
	case BROFINISHEVENT:
		ExecuteBroFinishEvent();
		break;
	case BROSTARTEVENT:
		ExecuteBroStartEvent();
		break;

	}
}


void Simulate() {
	struct event* CurrentEvent;


	while (TopEvent->Time < SimulationTime) {


#ifdef LOG
		printf(LogFile, "%lf¥t%d¥t%d¥t%d¥t%d¥t%d¥t", TopEvent->Time, TopEvent->EventNum, TopEvent->EventID, TopEvent->NodeID, TopEvent->Data1, TopEvent->Data2);
		printf(LogFile, "%d¥t%d¥t%d¥t%d¥n", Nodes[TopEvent->NodeID].PreviousNodeID, Nodes[TopEvent->NodeID].NextNodeID, Nodes[TopEvent->Data1].PreviousNodeID, Nodes[TopEvent->Data1].NextNodeID);
		fflush(LogFile);
#endif

		EventParser();

		CurrentEvent = TopEvent;
		TopEvent = TopEvent->Next;
		delete CurrentEvent;
	}
	if (AverageInterruptNodes == 0) {
		AverageInterruptDuration = 0;
		AverageNumInterrupt = 0;
	}
	else {
		AverageInterruptDuration = AverageInterruptDuration / AverageInterruptNodes;
		AverageNumInterrupt = AverageNumInterrupt / AverageInterruptNodes;
	}
}

void EvaluateLambda() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double TotalBandwidth, AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;
	AverageArrivalInterval = 30.0;
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 3.0 * 60.0;
	PieceSize = (int)(BitRate / 16);
	SimulationTime = 4 * 60 * 60;
	ServerBandwidth = 0.0;
	BandwidthWaver = 0.0;
	PlayOffTime = 0.0;
	NumServers = 1;
	Prefetch = 0.00;
	SubmitRequestLimitTime = 1.0e32;
	IntentDelay = 0.0;
	IntentDelayPiece = 4000;
	ChannelMethod = 0;

	NumChannels = 1;
	Channels[0].Bandwidth = 1000000.0;

	sprintf(FileName, 64, "EvaluateLambda.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 3; i <= 3; i++) {
		TotalBandwidth = 10000000.0 * i;
		fprintf(ResultFile, "%lf [Mbps]¥tSplitCast (Proposed)¥t¥t¥tFull Communication¥t¥t¥tFull Broadcasting¥t¥t¥tHalf Hybrid¥n", TotalBandwidth / 1000000.0);
		n = 2;//行数
		for (j = 1; j <= 15; j++) {
			if (j == 1)AverageArrivalInterval = j;
			else AverageArrivalInterval = (j - 1) * 5.0;
			/*
			switch (j) {
			case 1:AverageArrivalInterval = 0.1; break;
			case 2:AverageArrivalInterval = 0.2; break;
			case 3:AverageArrivalInterval = 0.5; break;
			case 4:AverageArrivalInterval = 1.0; break;
			case 5:AverageArrivalInterval = 2.0; break;
			case 6:AverageArrivalInterval = 5.0; break;
			case 7:AverageArrivalInterval = 10.0; break;
			case 8:AverageArrivalInterval = 20.0; break;
			case 9:AverageArrivalInterval = 50.0; break;
			case 10:AverageArrivalInterval = 100.0; break;
			}
			*/
			NumSimulateNodes = (int)(SimulationTime / AverageArrivalInterval) + NumServers;
			MinAveInterrupt = 1.0e32;
			fprintf(ResultFile, "%lf¥t", AverageArrivalInterval);
			for (l = 1; l <= 4; l++) {
				switch (l) {
				case 1: Channels[0].Bandwidth = (TotalBandwidth - BitRate) / 2.0; break;
				case 2: Channels[0].Bandwidth = 0.0; break;
				case 3: Channels[0].Bandwidth = TotalBandwidth; break;
				case 4: Channels[0].Bandwidth = TotalBandwidth / 2.0; break;
				}
				ServerBandwidth = TotalBandwidth - Channels[0].Bandwidth;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k + 1;
					NumSimulateNodes += 20;
					InitializeBandwidth();
					Initialize();
					InitializeChannel();
					//					InitializePrefetch();
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
				AverageNumInterrupt /= NSIM;
				printf(ResultFile, "%lf¥t%lf¥t%d¥t", AveInterruptDuration, AveNumInterrupt, AverageInterruptNodes);
				fflush(ResultFile);
			}
			printf(ResultFile, "¥n");
			n++;
		}
		while (n < 51) {
			printf(ResultFile, "¥n");
			n++;
		}
	}
	fclose(ResultFile);
}

void EvaluateTotalBandwidth() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double TotalBandwidth, AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;
	AverageArrivalInterval = 15.0;
	VarianceArrivalInterval = 5.0 / 1.65;
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 3.0 * 60.0;
	PieceSize = (int)(BitRate / 16);
	SimulationTime = 4 * 60 * 60;
	ServerBandwidth = 0.0;
	BandwidthWaver = 0.0;
	PlayOffTime = 0.0;
	NumServers = 1;
	Prefetch = 0.00;
	SubmitRequestLimitTime = 1.0e32;
	IntentDelay = 0.0;
	IntentDelayPiece = 4000;
	ChannelMethod = 0;

	NumChannels = 1;
	Channels[0].Bandwidth = 1000000.0;

	printf(FileName, 64, "EvaluateTotalBandwidth.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 3; i <= 3; i++) {
		/*
		switch (i) {
		case 1:AverageArrivalInterval = 0.1; break;
		case 2:AverageArrivalInterval = 0.2; break;
		case 3:AverageArrivalInterval = 0.5; break;
		case 4:AverageArrivalInterval = 1.0; break;
		case 5:AverageArrivalInterval = 2.0; break;
		case 6:AverageArrivalInterval = 5.0; break;
		case 7:AverageArrivalInterval = 10.0; break;
		case 8:AverageArrivalInterval = 20.0; break;
		case 9:AverageArrivalInterval = 50.0; break;
		}
		*/
		AverageArrivalInterval = 10.0 * i;

		NumSimulateNodes = (int)(SimulationTime / AverageArrivalInterval) + NumServers;
		printf(ResultFile, "Lambda=%lf¥tSplitCast (Proposed)¥t¥t¥tFull Communication¥t¥t¥tFull Broadcasting¥t¥t¥tHalf Hybrid¥n", AverageArrivalInterval);
		n = 2;//行数
		for (j = 0; j <= 24; j++) {
			TotalBandwidth = 2500000.0 * j;
			//				AverageArrivalInterval=j*0.1;
			MinAveInterrupt = 1.0e32;
			printf(ResultFile, "%lf¥t", TotalBandwidth / 1000000.0);
			for (l = 1; l <= 4; l++) {
				switch (l) {
				case 1: Channels[0].Bandwidth = (TotalBandwidth - BitRate) / 2.0; break;
				case 2: Channels[0].Bandwidth = 0.0; break;
				case 3: Channels[0].Bandwidth = TotalBandwidth; break;
				case 4: Channels[0].Bandwidth = TotalBandwidth / 2.0; break;
				}
				ServerBandwidth = TotalBandwidth - Channels[0].Bandwidth;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k + 1;
					NumSimulateNodes += 20;
					InitializeBandwidth();
					Initialize();
					InitializeChannel();
					//					InitializePrefetch();
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
				AverageNumInterrupt /= NSIM;
				printf(ResultFile, "%lf¥t%lf¥t%d¥t", AveInterruptDuration, AveNumInterrupt, AverageInterruptNodes);
				fflush(ResultFile);
			}
			printf(ResultFile, "¥n");
			n++;
		}
		while (n < 51) {
			printf(ResultFile, "¥n");
			n++;
		}
	}
	fclose(ResultFile);
}

void EvaluateDuration() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double TotalBandwidth, AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;
	AverageArrivalInterval = 30.0;
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	//Duration = 2.0 * 60.0;
	PieceSize = (int)(BitRate / 16);
	SimulationTime = 4 * 60 * 60;
	ServerBandwidth = 9000000.0;
	BandwidthWaver = 0.0;
	PlayOffTime = 0.0;
	NumServers = 1;
	Prefetch = 0.00;
	SubmitRequestLimitTime = 1.0e32;
	IntentDelay = 0.0;
	IntentDelayPiece = 4000;
	ChannelMethod = 0;

	NumChannels = 1;
	Channels[0].Bandwidth = 1000000.0;
	TotalBandwidth = 30000000.0;

	printf(FileName, 64, "EvaluateDuration.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 3; i <= 3; i++) {
		/*
		switch (i) {
		case 1:AverageArrivalInterval = 0.1; break;
		case 2:AverageArrivalInterval = 0.2; break;
		case 3:AverageArrivalInterval = 0.5; break;
		case 4:AverageArrivalInterval = 1.0; break;
		case 5:AverageArrivalInterval = 2.0; break;
		case 6:AverageArrivalInterval = 5.0; break;
		case 7:AverageArrivalInterval = 10.0; break;
		case 8:AverageArrivalInterval = 20.0; break;
		case 9:AverageArrivalInterval = 50.0; break;
		}
		*/
		AverageArrivalInterval = 10.0 * i;
		NumSimulateNodes = (int)(SimulationTime / AverageArrivalInterval) + NumServers;
		printf(ResultFile, "Lambda=%lf¥tSplitCast (Proposed)¥t¥t¥tFull Communication¥t¥t¥tFull Broadcasting¥t¥t¥tHalf Hybrid¥n", AverageArrivalInterval);
		n = 2;//行数
		for (j = 1; j <= 24; j++) {
			Duration = 15.0 * j;
			//				AverageArrivalInterval=j*0.1;
			MinAveInterrupt = 1.0e32;
			printf(ResultFile, "%lf¥t", Duration / 60.0);
			for (l = 1; l <= 4; l++) {
				switch (l) {
				case 1: Channels[0].Bandwidth = (TotalBandwidth - BitRate) / 2.0; break;
				case 2: Channels[0].Bandwidth = 0.0; break;
				case 3: Channels[0].Bandwidth = TotalBandwidth; break;
				case 4: Channels[0].Bandwidth = TotalBandwidth / 2.0; break;
				}
				ServerBandwidth = TotalBandwidth - Channels[0].Bandwidth;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k + 1;
					NumSimulateNodes += 20;
					InitializeBandwidth();
					Initialize();
					InitializeChannel();
					//					InitializePrefetch();
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
				AverageNumInterrupt /= NSIM;
				printf(ResultFile, "%lf¥t%lf¥t%d¥t", AveInterruptDuration, AveNumInterrupt, AverageInterruptNodes);
				fflush(ResultFile);
			}
			printf(ResultFile, "¥n");
			n++;
		}
		while (n < 51) {
			printf(ResultFile, "¥n");
			n++;
		}
	}
	fclose(ResultFile);
}
void EvaluateBitRate() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double TotalBandwidth, AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;
	AverageArrivalInterval = 30.0;
	VarianceArrivalInterval = 5.0 / 1.65;
	//BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 3.0 * 60.0;
	//PieceSize = (int)(BitRate / 16);
	SimulationTime = 4 * 60 * 60;
	ServerBandwidth = 0.0;
	BandwidthWaver = 0.0;
	PlayOffTime = 0.0;
	NumServers = 1;
	Prefetch = 0.00;
	SubmitRequestLimitTime = 1.0e32;
	IntentDelay = 0.0;
	IntentDelayPiece = 4000;
	ChannelMethod = 0;

	NumChannels = 1;
	Channels[0].Bandwidth = 1000000.0;
	TotalBandwidth = 30000000.0;

	printf(FileName, 64, "EvaluateBitRate.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 3; i <= 3; i++) {
		/*
		switch (i) {
		case 1:AverageArrivalInterval = 0.1; break;
		case 2:AverageArrivalInterval = 0.2; break;
		case 3:AverageArrivalInterval = 0.5; break;
		case 4:AverageArrivalInterval = 1.0; break;
		case 5:AverageArrivalInterval = 2.0; break;
		case 6:AverageArrivalInterval = 5.0; break;
		case 7:AverageArrivalInterval = 10.0; break;
		case 8:AverageArrivalInterval = 20.0; break;
		case 9:AverageArrivalInterval = 50.0; break;
		}
		*/
		AverageArrivalInterval = 10.0 * i;
		NumSimulateNodes = (int)(SimulationTime / AverageArrivalInterval) + NumServers;
		printf(ResultFile, "Lambda=%lf¥tSplitCast (Proposed)¥t¥t¥tFull Communication¥t¥t¥tFull Broadcasting¥t¥t¥tHalf Hybrid¥n", AverageArrivalInterval);
		n = 2;//行数
		for (j = 1; j <= 20; j++) {
			BitRate = 500000.0 * j;
			PieceSize = (int)(BitRate / 16);
			//				AverageArrivalInterval=j*0.1;
			MinAveInterrupt = 1.0e32;
			printf(ResultFile, "%lf¥t", BitRate / 1000000.0);
			for (l = 1; l <= 4; l++) {
				switch (l) {
				case 1: Channels[0].Bandwidth = (TotalBandwidth - BitRate) / 2.0; break;
				case 2: Channels[0].Bandwidth = 0.0; break;
				case 3: Channels[0].Bandwidth = TotalBandwidth; break;
				case 4: Channels[0].Bandwidth = TotalBandwidth / 2.0; break;
				}
				ServerBandwidth = TotalBandwidth - Channels[0].Bandwidth;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k + 1;
					NumSimulateNodes += 20;
					InitializeBandwidth();
					Initialize();
					InitializeChannel();
					//					InitializePrefetch();
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
				AverageNumInterrupt /= NSIM;
				printf(ResultFile, "%lf¥t%lf¥t%d¥t", AveInterruptDuration, AveNumInterrupt, AverageInterruptNodes);
				fflush(ResultFile);
			}
			printf(ResultFile, "¥n");
			n++;
		}
		while (n < 51) {
			printf(ResultFile, "¥n");
			n++;
		}
	}
	fclose(ResultFile);
}
void EvaluateChannelBandwidth() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double TotalBandwidth, AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 0;
	VarianceArrivalInterval = 0.0;
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 3.0 * 60.0;
	PieceSize = (int)(BitRate / 16);
	SimulationTime = 4 * 60 * 60;
	ServerBandwidth = 0.0;
	BandwidthWaver = 0.0;
	PlayOffTime = 0.0;
	NumServers = 1;
	Prefetch = 0.00;
	SubmitRequestLimitTime = 1.0e32;
	IntentDelay = 0.0;
	IntentDelayPiece = 4000;
	ChannelMethod = 0;

	NumChannels = 1;
	Channels[0].Bandwidth = 1000000.0;


	printf(FileName, 64, "EvaluateChannelBandwidth.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 27; i <= 29; i++) {
		/*
		switch (i) {
		case 1:AverageArrivalInterval = 0.1; break;
		case 2:AverageArrivalInterval = 0.2; break;
		case 3:AverageArrivalInterval = 0.5; break;
		case 4:AverageArrivalInterval = 1.0; break;
		case 5:AverageArrivalInterval = 2.0; break;
		case 6:AverageArrivalInterval = 5.0; break;
		case 7:AverageArrivalInterval = 10.0; break;
		case 8:AverageArrivalInterval = 20.0; break;
		case 9:AverageArrivalInterval = 50.0; break;
		}
		*/
		AverageArrivalInterval = 1.0 * i;
		//VarianceArrivalInterval = AverageArrivalInterval / 1.65;
		NumSimulateNodes = (int)(SimulationTime / AverageArrivalInterval) + NumServers;
		//fprintf_s(ResultFile, "Lambda=%lf¥tBB=0 [Mbps]¥t¥t¥tBB=1 [Mbps]¥t¥t¥tBB=2 [Mbps]¥t¥t¥tBB=3 [Mbps]¥t¥t¥tBB=4 [Mbps]¥t¥t¥tBB=5 [Mbps]¥t¥t¥tBB=6 [Mbps]¥t¥t¥tBB=7 [Mbps]¥t¥t¥tBB=8 [Mbps]¥t¥t¥tBB=9 [Mbps]¥t¥t¥tBB=10 [Mbps]¥n", AverageArrivalInterval);
		printf(ResultFile, "Lambda=%lf¥tTotal Bandwidth=1 [Mbps]¥t¥t¥tTotal Bandwidth=10 [Mbps]¥t¥t¥tTotal Bandwidth=20 [Mbps]¥t¥t¥tTotal Bandwidth=30 [Mbps]¥t¥t¥tTotal Bandwidth=40 [Mbps]¥t¥t¥tTotal Bandwidth=50 [Mbps]¥t¥t¥tTotal Bandwidth=60 [Mbps]¥n", AverageArrivalInterval);
		n = 2;//行数
		for (j = 0; j <= 30; j++) {
			Channels[0].Bandwidth = 2000000.0 * j;
			//				AverageArrivalInterval=j*0.1;
			MinAveInterrupt = 1.0e32;
			printf(ResultFile, "%lf¥t", Channels[0].Bandwidth / 1000000.0);
			for (l = 2; l <= 4; l++) {
				if (l == 0)TotalBandwidth = 1000000.0;
				else TotalBandwidth = 10000000.0 * l;
				ServerBandwidth = TotalBandwidth - Channels[0].Bandwidth;
				if (ServerBandwidth < 0.0) {
					printf(ResultFile, "¥t¥t¥t");
					continue;
				}

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k + 1;
					NumSimulateNodes += 20;
					InitializeBandwidth();
					Initialize();
					InitializeChannel();
					//					InitializePrefetch();
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
				AverageNumInterrupt /= NSIM;
				printf(ResultFile, "%lf¥t%lf¥t%d¥t", AveInterruptDuration, AveNumInterrupt, AverageInterruptNodes);
				fflush(ResultFile);
			}
			printf(ResultFile, "¥n");
			n++;
		}
		while (n < 51) {
			printf(ResultFile, "¥n");
			n++;
		}
	}
	fclose(ResultFile);
}

void EvaluateVariance() {
	int i, j, k, l, n;
	char FileName[64];
	FILE* ResultFile;
	double TotalBandwidth, AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt;

	RandType = 1;
	AverageArrivalInterval = 15.0;
	//VarianceArrivalInterval = 0.0 / 1.65;
	BitRate = 5000000.0;//128,256,384,512,640,768,896,1024
	Duration = 3.0 * 60.0;
	PieceSize = (int)(BitRate / 16);
	SimulationTime = 4 * 60 * 60;
	ServerBandwidth = 0.0;
	BandwidthWaver = 0.0;
	PlayOffTime = 0.0;
	NumServers = 1;
	Prefetch = 0.00;
	SubmitRequestLimitTime = 1.0e32;
	IntentDelay = 0.0;
	IntentDelayPiece = 4000;
	ChannelMethod = 0;

	NumChannels = 1;
	Channels[0].Bandwidth = 1000000.0;

	printf(FileName, 64, "EvaluateVariance.dat");
	ResultFile = myfopen(FileName, "w");
	for (i = 3; i <= 3; i++) {
		TotalBandwidth = 10000000.0 * i;
		printf(ResultFile, "%lf [Mbps]¥tSplitCast (Proposed)¥t¥t¥tFull Communication¥t¥t¥tFull Broadcasting¥t¥t¥tHalf Hybrid¥n", TotalBandwidth / 1000000.0);
		n = 2;//行数
		for (j = 0; j <= 30; j++) {
			VarianceArrivalInterval = j / 1.65;
			NumSimulateNodes = (int)(SimulationTime / AverageArrivalInterval) + NumServers;
			MinAveInterrupt = 1.0e32;
			printf(ResultFile, "%lf¥t", VarianceArrivalInterval);
			for (l = 1; l <= 4; l++) {
				switch (l) {
				case 1: Channels[0].Bandwidth = (TotalBandwidth - BitRate) / 2.0; break;
				case 2: Channels[0].Bandwidth = 0.0; break;
				case 3: Channels[0].Bandwidth = TotalBandwidth; break;
				case 4: Channels[0].Bandwidth = TotalBandwidth / 2.0; break;
				}
				ServerBandwidth = TotalBandwidth - Channels[0].Bandwidth;

				AveInterruptDuration = 0.0;
				AveNumInterrupt = 0.0;
				MaxInterrupt = 0.0;
				MinInterrupt = 1.0e32;
				for (k = 0; k < NSIM; k++) {
					Seed = k + 1;
					NumSimulateNodes += 20;
					InitializeBandwidth();
					Initialize();
					InitializeChannel();
					//					InitializePrefetch();
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
				AverageNumInterrupt /= NSIM;
				printf(ResultFile, "%lf¥t%lf¥t%d¥t", AveInterruptDuration, AveNumInterrupt, AverageInterruptNodes);
				fflush(ResultFile);
			}
			printf(ResultFile, "¥n");
			n++;
		}
		while (n < 51) {
			printf(ResultFile, "¥n");
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

	//EvaluateTotalBandwidth();
	//EvaluateLambda();
	//EvaluateDuration();
	//EvaluateBitRate();
	EvaluateChannelBandwidth();
	//EvaluateVariance();

#ifdef DYNAMIC
	free(Nodes);
#endif


	return 0;
}

