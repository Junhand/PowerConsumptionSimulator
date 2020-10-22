
#pragma warning(disable:4100)
//#include "stdafx.h"
#include <stdio.h>
#include "stdlib.h"
#include "math.h"
#include <string.h>
#include <random>
#include <iostream>
#include <iomanip>
#include <limits>

#define PI 3.14159265358979323846264338327950288419716939937510 
#define BITRATENUM 6 //Number of bitRate which you want to simulate 
#define MEDIANUM 3 //Number of media which you want to simulate

#define REQUESTMOVIE 0 //State of client
#define STARTMOVIE 1 //State of client
#define WATCHINGMOVIE 2 //State of client
#define FINISHMOVIE 3 //State of client
#define COLD -1 //Communication with the edge and cloud
#define ONLYCOLD -2 //Communication with cloud only
#define INTROCOLD -3 //Communication with the edge and cloud and store only intro
#define HOT 1 //Communication with the edge only
#define OTHERHOT 2 //Communication with other edge and the edge
#define ONLYHOT 3 //Communication with other edge only
#define INTROHOT 4 //Communication with the edge and other edge and store only intro


#define NUMBEROFCUSTOMER 10000 //Number of customers
#define EDGENUM 1 //Number of edges
#define EDGEVOLUME 400// volume of edges (e.g.1TB:800Mbit)

#define MOVIENUM 5 //Number of movies
#define MOVIELENGTH 60 //Video length
#define INTROLENGTH 10 //The length of the video stored on the edge
#define MOVIEPIECESIZE 5 //segment size
#define INTROPIECE INTROLENGTH/MOVIEPIECESIZE
#define LASTPIECE MOVIELENGTH/MOVIEPIECESIZE
#define LEASTNUM 1+(INTROLENGTH+MOVIELENGTH-INTROLENGTH-1)/(MOVIELENGTH-INTROLENGTH)

double bandWidthECL=1000.0;//Bandwidth between Edge and Client (e.g.1Gbps)
double bandWidthCE=200.0;//Bandwidth between Cloud and Edge (e.g.200Mbps)
double bandWidthEE=10000.0;//bandwidth between an Edge and the other Edge (e.g.10Gbps)
double bitRate[BITRATENUM] = {0.5, 1, 1.5, 2, 2.5, 5};//bitrate Mbps
double VarianceArrivalInterval;
double AverageArrivalInterval;
double lambda;//average number of views　divides time(s)
int range;//Determine the range of uniform distribution(e.g. 10)
int RandType;//Determine the distribution to be used
double simtime = 0.0;//Time in Simulation
double waittime = 0.0;
int usedcnt[EDGENUM][MOVIENUM];//lfu use, count the number of waching times
double storeTime[EDGENUM][MOVIENUM][LASTPIECE];//lfu use, time when the edge stores the piece 
int waitingFlag[EDGENUM][MOVIENUM][LASTPIECE];


//////////////////////////////////////////////////////////////////               CLASS               ////////////////////////////////////////////////////////////////////////
class Storage{
	int movieID[MOVIENUM];// e.g. hikakin 
	//int introLength[MOVIENUM][];//The intro time of the video in storage
	int moviePiece[MOVIENUM][LASTPIECE];//The time of the video in storage
	int watchingcnt[MOVIENUM][LASTPIECE];//
	int tmpWatchingcnt[MOVIENUM][LASTPIECE];//pieceがstoreされた際の要求人数を保存(そのpieceがstoreされるのを待っていた人数)
	double edgeBandWidth;
	double requestBandWidth;
	static double tmpEdgeBandWidth;//bandWidth
	double tmpRequestBandWidth;//bandWidth
	int numOfRequest;//number of customer
	static int numOfEdgeRequest;//number of Request
	double volume;//edgeの容量
	double sumReadData;//読み込んだデータ量
	double sumWriteData;//書き込まれたデータ量
	int sumClient;//アクセスしたクライアントの数
	double readTime;//total reading time
	double writeTime;//total writing time
	double firstWaitTime;//The time it takes for users to start watching the video.

  public:
	Storage(){
		for(int i=0;i<MOVIENUM;i++){
			//movieID[i]=-1;//no movie
			//introLength[i]=0;//no intro mobie
			for(int j=0;j<LASTPIECE;j++) {
				moviePiece[i][j] = 0;// no movie
				watchingcnt[i][j] = 0;//no watch
			}
		} 
		volume=0;//2TB
		numOfRequest=0;
		numOfEdgeRequest=0;
		readTime=0;
		writeTime=0;
		firstWaitTime=0;
		sumClient=0;
		sumReadData=0;
		sumWriteData=0;
	};

	void add_volume(double movieSize){ volume += movieSize; }
	void delete_volume(double movieSize){ volume -= movieSize; }
	double get_volume(){ return volume;}

    void set_movieID(int i, int movie){ movieID[i] = movie; }
    int get_movie(int i){ return movieID[i]; }

	//void set_introLength(int i,double length){ introLength[i] = length; }
    //int get_introLength(int i){ return introLength[i]; }

	void set_moviePiece(int movieID, int nodeID){ moviePiece[movieID][nodeID] = 1; }
	void delete_moviePiece(int movieID, int nodeID){ moviePiece[movieID][nodeID] = 0;}
    int is_moviePieceHit(int movieID, int nodeID){
		if(moviePiece[movieID][nodeID]!=0) return 1;
		else return 0;
	}

	void add_all_watchingcnt(int movieID, int nodeID){ 
		for(int i=nodeID;i<LASTPIECE;i++) watchingcnt[movieID][i] += 1; 
	}
	void add_watchingcnt(int movieID, int nodeID){ 
		watchingcnt[movieID][nodeID] += 1; 
	}
    void delete_watchingcnt(int movieID, int nodeID){ watchingcnt[movieID][nodeID] -= 1; }

	int get_watchingcnt(int movieID, int nodeID){ return watchingcnt[movieID][nodeID]; }

	void set_tmpWatchingcnt(int movieID, int nodeID,int num){ 
		tmpWatchingcnt[movieID][nodeID] = num; 
	}
    void delete_tmpWatchingcnt(int movieID, int nodeID){ tmpWatchingcnt[movieID][nodeID] -= 1; }

	int get_tmpWatchingcnt(int movieID, int nodeID){ return tmpWatchingcnt[movieID][nodeID]; }

	void set_edge_bandWidth(int band){//edge間の帯域幅の初期化
		edgeBandWidth = band;
		tmpEdgeBandWidth= band; 
	}
    double get_edge_bandWidth(){ return tmpEdgeBandWidth; }//エッジ間の帯域幅を得る

	void set_request_bandWidth(int band){//edge-clientまたはcloud-edgeの帯域幅を初期化
		requestBandWidth = band; 
		tmpRequestBandWidth = band;
	}
    double get_request_bandWidth(){ return tmpRequestBandWidth; }//edge-clientまたはcloud-edgeの帯域幅を得る

	void add_numOfEdgeRequest(int num){//edge間のアクセス人数を追加
		numOfEdgeRequest+=num; 
		calculateBandwidth(1);//帯域幅の計算
	}

	void delete_numOfEdgeRequest(int num){//edge間のアクセス人数を削除
		numOfEdgeRequest-=num; 
		calculateBandwidth(1);//帯域幅の計算	
	}

	void add_numOfRequest(int num){//edge-clientまたはcloud-edgeのアクセス人数を追加
		numOfRequest+=num; 
		calculateBandwidth(0);	
	}

	void delete_numOfRequest(int num){//edge-clientまたはcloud-edgeのアクセス人数を削除
		numOfRequest-=num; 
		calculateBandwidth(0);	
	}

    int get_numOfEdgeRequest(){ return numOfEdgeRequest; }//edge間のアクセス人数を得る
	int get_numOfRequest(){ return numOfRequest; }////edge-clientまたはcloud-edgeのアクセス人数を得る

	void calculateBandwidth(int edgeFlag){//帯域幅の計算
		int num = numOfRequest;
		if(edgeFlag) num=numOfEdgeRequest;//edge間通信の帯域幅の計算の場合
		if(num>0){
			tmpRequestBandWidth = requestBandWidth/num;

		}else{//0人の場合
			if(edgeFlag) tmpEdgeBandWidth = edgeBandWidth;
			else tmpRequestBandWidth = requestBandWidth;
		}
	}

	void add_read_data(double data){ sumReadData+=data; }//書き込みデータ量の追加
	double get_sum_readData(){ return sumReadData; }

	void add_write_data(double data){ sumWriteData+=data; }//読み込みデータ量の追加
	double get_sum_writeData(){ return sumWriteData; }

	void add_client_num(int num){ sumClient+=num; }
	int get_sumClient(){ return sumClient; }

	void add_readTime(double time){ readTime+=time; }
	double get_readTime(){ return readTime; }

	void add_writeTime(double time){ writeTime+=time; }
	double get_writeTime(){ return writeTime; }

	void add_firstWaitTime(double time){ writeTime+=time; }
	double get_firstWaitTime(){ return writeTime; }
};


//////////////////////////////////////////////////////////                    STRUCT                          ///////////////////////////////////////////////////////////////
struct event{
	double time;
	int clientID;//clientID
	int where[2];//The nearest edge number
	int state[2];//cold -3~-1, hot 1~4 :: edge-client 1, edges or cloud-edge 0
	int eventID;//arrival start finish   1 2 3 
	int startTime;
	struct event* next;
	struct node* node;
};

struct media{
	double bandWidth;
	double readPowerConsumption;
	double idlePowerConsumption;
	double price;
};

struct node{
	struct node* Next;
	struct node* Previous;
	int	NodeID;
	int movieID; 
	double bitRate;
};


struct event* TopEvent;
struct node* TopNode[NUMBEROFCUSTOMER];
struct event* RequestEvent;
Storage hot[EDGENUM];
Storage cold;
double Storage::tmpEdgeBandWidth;
int Storage::numOfEdgeRequest;

/////////////////////////////////////////////////////////////////////////     FUNCTION        //////////////////////////////////////////////////////////////////////////////

struct event* AddEvent(double time, double startTime, int clientID, int where[2],int state[2], int eventID, node* node) {
	struct event* PreviousEvent = NULL;
	struct event* CurrentEvent;
	struct event* NewEvent;

	NewEvent = (struct event*)malloc(sizeof(event));
	NewEvent->time = time;
	NewEvent->startTime = startTime;
	NewEvent->clientID = clientID;
	NewEvent->where[0] = where[0];
	NewEvent->where[1] = where[1];
	NewEvent->state[0] = state[0];//どこから動画を撮ってくるか　動画を保存するかなどの情報
	NewEvent->state[1] = state[1];//edge-client通信か edge-edge,cloud-edge通信か
	NewEvent->next = NULL;
	NewEvent->eventID = eventID;//REQUESTMOVIE, STARTMOVIE ...
	NewEvent->node = node;

	if (TopEvent == NULL) {//初期イベント
		TopEvent = NewEvent;
		return NewEvent;
	}

	CurrentEvent = TopEvent;
	while (CurrentEvent) {
		if (CurrentEvent->time <= NewEvent->time) {
			if (CurrentEvent->next != NULL) {
				PreviousEvent = CurrentEvent;
				CurrentEvent = CurrentEvent->next;	

			}else {// NewEvent is inserted at the end
				CurrentEvent->next = NewEvent;
				break;
			}

		}else{//CurrentEvent->time > NewEvent->time
			if(PreviousEvent != NULL) {//NewEvents is inserted in the middle
				PreviousEvent->next = NewEvent;
				NewEvent->next = CurrentEvent;

			}else{//NewEvent is inserted first
				NewEvent->next = CurrentEvent;
				TopEvent = NewEvent;
			}
			break;
		}
	}
	return TopEvent;
}

void lfuInitialize() {
    for(int i=0; i<EDGENUM; i++) { 

		for(int j=0; j<MOVIENUM; j++) {

			usedcnt[i][j]=0;//視聴回数
			for(int k=0; k<LASTPIECE; k++) {
				storeTime[i][j][k]=0;//edge i, 動画番号 j, piece k
			
			}
		}
	}
}

int isHitEdge(int edge, int movieID, int nodeID) {
    int hit=0;
	if(hot[edge].is_moviePieceHit(movieID,nodeID)) return hit=1;

    for(int i=0; i<EDGENUM; i++) {
        if(hot[i].is_moviePieceHit(movieID,nodeID)) {
            hit=1;
            break;
        }
    }
    return hit;
}

int getHitEdge(int edge, int movieID, int nodeID) {
    int hit=COLD;
	if(hot[edge].is_moviePieceHit(movieID,nodeID)) return edge;

    for(int i=0; i<EDGENUM; i++) {
        if(hot[i].is_moviePieceHit(movieID,nodeID)) {
            hit=i;
            break;
        }
 
    }
    return hit;
}

int isLfuHit(int edge, int movieID, int nodeID) {
    int hit=0;
    if(hot[edge].is_moviePieceHit(movieID,nodeID)) {//nodeID番目のpieceがあるかどうか
            hit=1;
            return hit;
    }
    return hit;
}

int getLfuHitIndex(int edge, int movieID, int nodeID) {
    int hitind;
    if(storeTime[edge][movieID][nodeID] != 0) {//nodeID番目のpieceが入っているところの添字を確認
        hitind = movieID;//p={0,1.72,0,11.56,0,0,0,0...} 動画3を探索 動画3は11.56秒に保存された
        return hitind;
    }
    return hitind;
}



int lfu(double time, int where[2], int movieID, int nodeID, double bitRate) {
    int least = 9999;
	//int leastList[LEASTNUM];
	//for(int i=0;i<LEASTNUM;i++) leastList[i] = 9999;
	//int leastind=0;
	int renode = -1;
	int removie = -1;
	int flag = 0;
	
    printf("movieID%d edge%d volume %.0f from%d \n",movieID,where[0],hot[where[0]].get_volume(),where[1]);

	//他のエッジからpieceを取得
	if(where[0] != where[1] && where[1] != COLD){
		if(hot[where[0]].get_volume() + MOVIEPIECESIZE*bitRate <= EDGEVOLUME) {//can store
				printf("edge%d can store movieID%d nodeID%d from edge%d\n",where[0],movieID,nodeID,where[1]);
				hot[where[0]].set_moviePiece(movieID,nodeID);
				hot[where[1]].add_read_data(MOVIEPIECESIZE*bitRate);
				hot[where[0]].add_write_data(MOVIEPIECESIZE*bitRate);
				hot[where[0]].add_volume(MOVIEPIECESIZE*bitRate);
				for(int i = hot[TopEvent->where[0]].get_watchingcnt(movieID,nodeID); i>0; i--){//その動画を取っているエッジ間通信を全て削除
					hot[where[1]].delete_numOfEdgeRequest(1);
					hot[where[1]].delete_watchingcnt(movieID,nodeID);
				}
				hot[where[0]].set_tmpWatchingcnt(movieID,nodeID,hot[where[0]].get_watchingcnt(movieID,nodeID));//そのpieceを待っている人数を保存
				storeTime[where[0]][movieID][nodeID] = time;
				return OTHERHOT;
        }else{
			for(int i=LASTPIECE-1; i>=0; i++){
            	for(int j=0; j<MOVIENUM; j++){//lfu実行
                	if(usedcnt[where[0]][j]<least && hot[where[0]].get_watchingcnt(j,i) == 0){
						least=usedcnt[where[0]][j];
						removie=j;
						renode=i;
							
                	}
				}

				if(least!=9999){
					printf("edge%d get piece from edge%d and change movieID%d nodeID%d to movieID%d node%d\n",where[0],where[1],removie,renode,movieID,nodeID);
					hot[where[0]].delete_moviePiece(removie,renode);
					hot[where[0]].set_moviePiece(movieID,nodeID);
					hot[where[1]].add_read_data(MOVIEPIECESIZE*bitRate);
					hot[where[0]].add_write_data(MOVIEPIECESIZE*bitRate);
					for(int i = hot[TopEvent->where[0]].get_watchingcnt(movieID,nodeID); i>0; i--){//そのpieceを取っているエッジ間通信を全て削除
						hot[where[1]].delete_numOfEdgeRequest(1);
						hot[where[1]].delete_watchingcnt(movieID,nodeID);
					}
					hot[where[0]].set_tmpWatchingcnt(movieID,nodeID,hot[where[0]].get_watchingcnt(movieID,nodeID));//そのpieceを待っている人数を保存
					storeTime[where[0]][removie][renode] = 0;
					storeTime[where[0]][movieID][nodeID] = time;
					return OTHERHOT;

				}else{
					printf("edge%d gets movieID%d nodeID%d from directly edge%d",where[0],movieID,nodeID,where[1]);
					hot[where[1]].add_read_data(MOVIEPIECESIZE*bitRate);
					hot[where[1]].delete_watchingcnt(movieID,nodeID);
					return OTHERHOT;
				}
			}
		}

	}else{
		if(hot[where[0]].get_volume() + MOVIEPIECESIZE*bitRate <= EDGEVOLUME) {//can store
				printf("edge%d can store movieID%d nodeID%d from cold\n",where[0],movieID,nodeID);
				hot[where[0]].set_moviePiece(movieID,nodeID);
				cold.add_read_data(MOVIEPIECESIZE*bitRate);
				hot[where[0]].add_write_data(MOVIEPIECESIZE*bitRate);
				hot[where[0]].add_volume(MOVIEPIECESIZE*bitRate);
				for(int i = hot[where[0]].get_watchingcnt(movieID,nodeID); i>0; i--){//その動画を取っているクラウドエッジ間通信を全て削除
					cold.delete_numOfRequest(1);
					cold.delete_watchingcnt(movieID,nodeID);
				}
				hot[where[0]].set_tmpWatchingcnt(movieID,nodeID,hot[where[0]].get_watchingcnt(movieID,nodeID));//そのpieceを待っている人数を保存
				storeTime[where[0]][movieID][nodeID] = time;
				return COLD;
        }else{
			for(int i=LASTPIECE-1; i>=0; i++){
            	for(int j=0; j<MOVIENUM; j++){//lfu実行
                	if(usedcnt[where[0]][j]<least && hot[where[0]].get_watchingcnt(j,i) == 0){
						least=usedcnt[where[0]][j];
						removie=j;
						renode=i;
							
                	}
				}

				if(least!=9999){
					printf("edge%d get piece from cold and change movieID%d nodeID%d to movieID%d node%d\n",where[0],removie,renode,movieID,nodeID);
					hot[where[0]].delete_moviePiece(removie,renode);
					hot[where[0]].set_moviePiece(movieID,nodeID);
					cold.add_read_data(MOVIEPIECESIZE*bitRate);
					hot[where[0]].add_write_data(MOVIEPIECESIZE*bitRate);
					for(int i = hot[where[0]].get_watchingcnt(movieID,nodeID); i>0; i--){//そのpieceを取っているエッジ間通信を全て削除
						cold.delete_numOfRequest(1);
						cold.delete_watchingcnt(movieID,nodeID);
					}
					hot[where[0]].set_tmpWatchingcnt(movieID,nodeID,hot[where[0]].get_watchingcnt(movieID,nodeID));//そのpieceを待っている人数を保存
					storeTime[where[0]][removie][renode] = 0;
					storeTime[where[0]][movieID][nodeID] = time;
					return COLD;

				}else{
					printf("edge%d gets movieID%d nodeID%d from directly cold",where[0],movieID,nodeID);
					cold.add_read_data(MOVIEPIECESIZE*bitRate);
					cold.delete_watchingcnt(movieID,nodeID);
					return COLD;
				}
			}
		}

	}
}


void EvaluatePowerConsumption(struct media media[]);
void EvaluateRatioOfEdge(void);
void EvaluateCost(struct media media[]);
void EventParser();
void Initialize();
void Simulation();
void ExecuteRequestMovie();
void ExecuteStartMovie();
void ExecuteWatchingMovie();
void ExecuteFinishMovie();
double RequestOnRand(int);

////////////////////////////////////////////////////////////////////             MAIN                   ////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{	
	struct media media[MEDIANUM] =  {//transfer Rate,Read,Idle,Price
								{250*8,8,3,5000},//HDD
								{500*8,3,2,10000},//SSD
								{200*8,8,0.4,2000}//Disc
								};//media
	lambda = 0.1;
	//int simulationTime = 4*60*60;
	Simulation();
	//EvaluateRatioOfEdge();//ratio of first length of movie
	//EvaluatePowerConsumption(media);//all power consumption
	//EvaluateCost(media);//cost
	return 0;
}


///////////////////////////////////////////////////////////////////            EVALUATE              ///////////////////////////////////////////////////////////////////////////////////////////


void EvaluateRatioOfEdge() 
{
	double latencyECL;//between Edge and Client
	double allMovieData = 800*8;//movie data size (ex.150MB)
	double EdgeMovieData;
	double bandWidthCCL;//between Cloud and Client
	lambda =0.1;
	int i,j;

	bandWidthCE = 11*8;//(ex.HDD 170MB/s)
	bandWidthECL = 500*8;//(ex.SSD 1940MB/s) not change
	bandWidthCCL = bandWidthCE + bandWidthECL;

	//Simulation(lambda,NUMBEROFCUSTOMER);//start simulation
	
	for(j=0;j<BITRATENUM;j++)
	{
		EdgeMovieData = allMovieData*bitRate[j]*bandWidthECL / ( bitRate[j]*(bandWidthCCL) + bandWidthCCL );	
		latencyECL = EdgeMovieData/bandWidthECL + EdgeMovieData/bitRate[j];
		printf("All Movie Data Size %.0lfMB coldmedia %.0lfMB/s Hotmedia %.0lfMB/s bitRate %.1lfMb/s EdgeMovieData %lf%% \n",allMovieData/8,bandWidthCE/8,bandWidthECL/8, bitRate[j],EdgeMovieData/allMovieData*100);
	}
}

void EvaluatePowerConsumption(struct media media[])
{
	int i;
	double ratioOfRead = 0.8;
	double powerConsumption;

	for(i=0;i<MEDIANUM;i++)
	{
		powerConsumption = media[i].readPowerConsumption * ratioOfRead + media[i].idlePowerConsumption * (1-ratioOfRead);
		printf("averagePowerConsumption %lfW\n",powerConsumption);
	}
}

void EvaluateCost(struct media media[])
{
	int i,j,cost;
	int hotmediaPrice,coldmediaPrice;
	int power,powerPrice;

	for(i=0;i<MEDIANUM;i++)
	{
		for(j=0;j<MEDIANUM;i++)
		{
			hotmediaPrice = media[i].price;
			coldmediaPrice = media[j].price;
			cost = hotmediaPrice + coldmediaPrice + power * 24 * 365 * powerPrice;
		}
	}
}


/////////////////////////////////////////////////////////////////////////         DISTRIBUTION          /////////////////////////////////////////////////////////////////////

double NormalRand() 
{
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

double RequestOnRand(int randType) 
{
	double r;
	long double Poisson;
	int i;

	switch (randType) 
	{
	case 0: //指数分布
		r = -log((double)rand() / (double)RAND_MAX) / lambda;
		break;
	case 1://正規分布
		r = NormalRand() * VarianceArrivalInterval + AverageArrivalInterval;
		break;
	case 2:	//一定分布
		r = AverageArrivalInterval;
		break;
	case 3: //ポアソン乱数
		Poisson = exp(700.0) * rand() / RAND_MAX;//exp(700.0)まで
		for (i = 0; Poisson > 1.0; i++) 
		{
			Poisson *= (double)rand() / RAND_MAX;
		}
		r = (double)i / (700.0 / AverageArrivalInterval);
		break;
	case 4: //一様離散分布
		r = ((double)(rand()*(range+1.0)))/((double)RAND_MAX+1.0);
		break;

	case 5: //zipの法則
		double sum=0;
		double ran=0;
		double s=0.729;//
		double zipVar[MOVIENUM] = {0};
		
		for(int i = 1;i<=range;i++)//range: the number of movies
		{//　Σ[i=1,range] (1/i)^s 　= 　(1/1)^s + (1/2)^s + (1/3)^s + ・・・
			sum += pow(1/(double)i,s);
		}
	
		for(int i = 1;i<=range;i++)
		{//離散型累積分布関数の生成
			if(i==1) zipVar[i-1] = pow(1/(double)i,s) / sum;//zipVar[0] = (1/1)^s /sum
			else zipVar[i-1] = pow(1/(double)i,s) / sum + zipVar[i-2];
		}//zipVar[i-1] = (1/i)^s + Σ[i=0,i-2] zipVar[i]
		ran=(double)rand() / (double)RAND_MAX;
		for(int i =0;i<range;i++)
		{//(e.g.zipVar[0]=0.625,zipVar[1]=0.925で ran=0.6ならばr=0,ran=0.7ならばr=1)
			if(ran<=zipVar[i])
			{
				r=i;//movie number
				break;
			}
		}
		break;
	}
	if (r < 1.0e-8) {
		r = 1.0e-8;
	}
	return r;
}


////////////////////////////////////////////////////////                 SIMULATION                      //////////////////////////////////////////////////////////////////

void Simulation() {
	int where[2];
	int state[2];
	//where[0]:クライアントが動画をとってくるエッジ
	//where[1]:そのエッジがところ (-1:cloud, 0~:edge, 9999:initialize)
	
	Initialize();
	lfuInitialize();

    double t = 0.0;
	range = EDGENUM-1;//一様分布の最大値を決定
    for(int i=0; i<NUMBEROFCUSTOMER; i++){
        t += RequestOnRand(0);//lamdaを元に計算
		where[0] = RequestOnRand(4);//一番近いエッジ　一様分布に従う
		where[1] = 9999;//一番近いエッジが動画をとってくるところ　初期値は9999 クラウド-1 他のエッジ0~
		state[0] = 9999;
		state[1] = 1;//1:edge-client間通信　0:edge間またはcloud-edge間通信
        TopEvent = AddEvent(t,t,i,where,state,REQUESTMOVIE,TopNode[i]);//時間　clientID 動画取得場所[2] 取得状態[2] イベント Node
    }

	while(TopEvent){
		std::cout<< "ID"<< TopEvent->clientID<< " time"<< std::fixed << std::setprecision(4)<< TopEvent->time<< " edge"<< TopEvent->where[0] << " from"<< TopEvent->where[1]<< " state"<< TopEvent->state[0] << " communication" << TopEvent->state[1];
		std::cout<< " event"<< TopEvent->eventID<< " movie"<< TopEvent->node->movieID<< " views" << hot[TopEvent->where[0]].get_watchingcnt(TopEvent->node->movieID,LASTPIECE-1) <<" node" <<TopEvent->node->NodeID;
		std::cout<< " hotClient"<< hot[TopEvent->where[0]].get_numOfRequest()<<" "<< std::fixed << std::setprecision(4)<< hot[TopEvent->where[0]].get_request_bandWidth()<< "Mbps"<< " hotServer"<<  hot[TopEvent->where[0]].get_numOfEdgeRequest()<< " "<< std::fixed << std::setprecision(4)<< hot[TopEvent->where[0]].get_edge_bandWidth()<< "Mbps"<< " cold"<< cold.get_numOfRequest()<< " "<< std::fixed << std::setprecision(4)<< cold.get_request_bandWidth()<< "Mbps\n";
		EventParser();
		//TopEvent=TopEvent->next;
	}
}

void Initialize() {
	struct node* PreviousNode = NULL;
	struct node* CurrentNode;
	struct node* TempNode;

	//クラウドの初期化
	for(int i=0;i<MOVIENUM;i++){
		cold.set_movieID(i,i);//全ての動画の番号をクラウドに保存する
		for(int j=0;j<LASTPIECE;j++)cold.set_moviePiece(i,j);//動画iのPiecejを保存
		cold.set_request_bandWidth(bandWidthCE);//e.g.)200Mbps　クラウドエッジ間の帯域幅 
	}
	
	//エッジの初期化
	for(int i=0;i<EDGENUM;i++){
		hot[i].set_request_bandWidth(bandWidthECL);//エッジクライアント間の帯域幅 e.g.)1000Mbps
		hot[i].set_edge_bandWidth(bandWidthEE);//エッジエッジ間の帯域幅, 全てのエッジで共有 e.g.)10000Mbps
	}

	for(int i = 0; i < NUMBEROFCUSTOMER; i++){//クライアントごとのnodeを作成
		range = BITRATENUM-1;//bitRateをランダムにする際に使用
		int bitRateIndex = 3;//int(RequestOnRand(4)) どのBitRateにするか 
		range = MOVIENUM;//zip's lowの動画の数の決定に使用
		int movieID = int(RequestOnRand(5));//zipf's low

		for (int j = 0; j < LASTPIECE; j++){//(e.g. LASTPIECE = MOVIESIZE / SEGMENTSIZE = 60[s]/5[s] )
			struct node* NewNode = (struct node*)malloc(sizeof(node));
			NewNode->movieID = movieID;
			NewNode->NodeID = j;
			NewNode->bitRate = bitRate[bitRateIndex];//bitRateIndex=3ならばbitRate[3] = 2Mbps

			if (TopNode[i] == NULL) {//make initial node
				TopNode[i] = NewNode;
				CurrentNode = TopNode[i];
				continue;
			}

			if (CurrentNode->Next == NULL) {// NewNode is inserted at the end
				CurrentNode->Next = NewNode;
				CurrentNode = CurrentNode->Next;
				continue;
			}
		}
		//while(TopNode[i]!=NULL){
		//	printf("%d %d %f %d\n",TopNode[i]->NodeID,TopNode[i]->movieID,TopNode[i]->bitRate,TopNode[i]->Piece);
		//	TopNode[i] = TopNode[i]->Next;
		//}
	}
}

/////////////////////////////////////////////////////////               EVENT                 ///////////////////////////////////////////////////////////////////////////
void EventParser() 
{
	switch (TopEvent->eventID) 
	{
	case REQUESTMOVIE:
		ExecuteRequestMovie();
		break;
	case STARTMOVIE:
		ExecuteStartMovie();
		break;
	case WATCHINGMOVIE:
		ExecuteWatchingMovie();
		break;
	case FINISHMOVIE:
		ExecuteFinishMovie();
		break;
	}
}

void ExecuteRequestMovie(){//intro部分の通信
	double time;
	double bitRate = TopEvent->node->bitRate;
	double EdgeMovieData = MOVIEPIECESIZE*bitRate;
	int type = REQUESTMOVIE;
	int state[2];
	int where[2];
	int edge;
	int backFlag=0;

	//initialize communication
	state[0] = TopEvent->state[0];//通信方法
	state[1] = TopEvent->state[1];//エッジクライアント間通信:1 エッジ間・クラウドエッジ間通信:0
	where[0] = TopEvent->where[0];
	where[1] = TopEvent->where[1];

	if(TopEvent->node->NodeID !=0 || state[1]==0 ) {
		//前回の通信が終了
		if(state[0] == HOT) {
			hot[where[0]].delete_numOfRequest(1);//一番近いエッジからデータを取ってきていた
			hot[where[0]].delete_watchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID-1);
			hot[where[0]].add_read_data(EdgeMovieData);
			if(TopEvent->node->NodeID == LASTPIECE){
				TopEvent = TopEvent->next;
				return;
			}

		}else if(state[0] == OTHERHOT) {//他のエッジからデータを取ってきていた
			if(state[1]) {//4番目
				if(waitingFlag[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID-1]){//待っていたリクエストを全て処理
					for(int i=hot[where[0]].get_tmpWatchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID-1);i>0;i--){
						hot[where[0]].delete_numOfRequest(1);//エッジクライアント間通信
						hot[where[0]].delete_watchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID-1);
						hot[where[0]].add_read_data(EdgeMovieData);
					}
					waitingFlag[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID-1] = 0;
					if(TopEvent->node->NodeID == LASTPIECE){
						TopEvent = TopEvent->next;
						return;
					}

				}else{
					if(TopEvent->node->NodeID == LASTPIECE){
						TopEvent = TopEvent->next;
						return;
					}	
				}

			}else{//2番目
				if(TopEvent->where[0] == getHitEdge(where[0],TopEvent->node->movieID,TopEvent->node->NodeID)) {//すでにpieceが保存されていた
					TopEvent->time = storeTime[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID];//時間をエッジに動画が保存し終わった時間に変更
					backFlag=1;
				}else{//一番近いエッジにはまだpieceが保存されていない
					state[0] = lfu(TopEvent->time, TopEvent->where, TopEvent->node->movieID, TopEvent->node->NodeID, TopEvent->node->bitRate);
					
				}
			}

		}else if(state[0] == COLD) {//クラウドからデータを取ってきていた
			if(state[1]) {//4番目
				if(waitingFlag[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID-1]){//待っていたリクエストを全て処理
					for(int i=hot[where[0]].get_tmpWatchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID-1);i>0;i--){
						hot[where[0]].delete_numOfRequest(1);//エッジクライアント間通信
						hot[where[0]].delete_watchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID-1);
						hot[where[0]].add_read_data(EdgeMovieData);
					}
					waitingFlag[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID-1] = 0;
					if(TopEvent->node->NodeID == LASTPIECE){
						TopEvent = TopEvent->next;
						return;
					}

				}else{
					if(TopEvent->node->NodeID == LASTPIECE){
						TopEvent = TopEvent->next;
						return;
					}
				}

			}else{//2番目
				if(TopEvent->where[0] == getHitEdge(where[0],TopEvent->node->movieID,TopEvent->node->NodeID)) {//すでにpieceが保存されていた
					TopEvent->time = storeTime[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID];//時間をエッジに動画が保存し終わった時間に変更
					backFlag=1;
				}else{//一番近いエッジにはまだpieceが保存されていない
					state[0] = lfu(TopEvent->time, TopEvent->where, TopEvent->node->movieID, TopEvent->node->NodeID, TopEvent->node->bitRate);
					
				}
			}

		}
	}

	if(state[1]){
		if(isHitEdge(TopEvent->where[0],TopEvent->node->movieID,TopEvent->node->NodeID) && state[1] == 1){//いづれかのエッジがpieceを持っているかどうか確認
			edge = getHitEdge(TopEvent->where[0],TopEvent->node->movieID,TopEvent->node->NodeID);//pieceを持っているエッジ番号を取得
			if(edge == TopEvent->where[0]) {//一番近いエッジにあった
				where[0] = edge;
				where[1] = edge;
				state[0] = HOT;
				printf("No page fault!\n");

			}else{//他のエッジにあった
				where[0] = TopEvent->where[0];
				where[1] = edge;
				state[0] = OTHERHOT;
			}

		}else{//どのエッジもpieceを持っていない
			where[0] = TopEvent->where[0];
			where[1] = COLD;
			state[0] = COLD;
		}
	}

	//通信開始
	if(state[0] == HOT) {
		hot[where[0]].add_numOfRequest(1);//一番近いエッジからデータを取ってくる
		time = TopEvent->time + EdgeMovieData/hot[where[0]].get_request_bandWidth();
		if(TopEvent->node->NodeID == 0) {
			hot[where[0]].add_all_watchingcnt(TopEvent->node->movieID, TopEvent->node->NodeID);//視聴中
			usedcnt[where[0]][TopEvent->node->movieID] += 1;//動画の視聴回数を+1;
			hot[where[0]].add_firstWaitTime(time - TopEvent->startTime);//動画を見始めるまでの時間
			hot[where[0]].add_client_num(1);//そのエッジから動画を取得したクライアントの人数
		}
		TopEvent->node->NodeID += 1;
		TopEvent = AddEvent(time,TopEvent->startTime,TopEvent->clientID,where,state,type,TopEvent->node)->next;

	}else if(state[0] == OTHERHOT) {//他のエッジからデータを取ってくる
		if(state[1]) {//1番目
			hot[where[1]].add_numOfEdgeRequest(1);//エッジ間通信
			hot[where[1]].add_watchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID);//取ってくる先のpieceを視聴中にする
			time = TopEvent->time + EdgeMovieData/hot[where[1]].get_edge_bandWidth();
			if(TopEvent->node->NodeID == 0) {
				hot[where[0]].add_all_watchingcnt(TopEvent->node->movieID, TopEvent->node->NodeID);//視聴中
				usedcnt[where[0]][TopEvent->node->movieID] += 1;//動画の視聴回数を+1;
			}

			state[1] = 0;//エッジ間通信flag
			TopEvent = AddEvent(time,TopEvent->startTime,TopEvent->clientID,where,state,type,TopEvent->node)->next;

		}else {//3番目
			if(backFlag) {//すでにpieceが保存されていた
				time = TopEvent->time + EdgeMovieData/hot[where[0]].get_request_bandWidth();//エッジクライアント間通信
				TopEvent->node->NodeID += 1;
				state[1] = 1;
				TopEvent = AddEvent(time,TopEvent->startTime,TopEvent->clientID,where,state,type,TopEvent->node)->next;
				
					
			}else{//一番近いエッジにはまだpieceが保存されていない
				for(int i = hot[where[0]].get_watchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID); i>0; i--){
					hot[where[0]].add_numOfRequest(1);//エッジ間通信
				}		
				waitingFlag[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID]=1;
				time = TopEvent->time + EdgeMovieData/hot[where[0]].get_request_bandWidth();//エッジクライアント間通信
				TopEvent->node->NodeID += 1;
				state[1] = 1;
				TopEvent = AddEvent(time,TopEvent->startTime,TopEvent->clientID,where,state,type,TopEvent->node)->next;
			}
		}

	}else if(state[0] == COLD) {//クラウドからデータを取ってくる
		if(state[1]) {//1番目
			cold.add_numOfRequest(1);//クラウドエッジ間通信
			cold.add_watchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID);//取ってくる先のpieceを視聴中にする
			time = TopEvent->time + EdgeMovieData/cold.get_request_bandWidth();
			if(TopEvent->node->NodeID == 0) {
				hot[where[0]].add_all_watchingcnt(TopEvent->node->movieID, TopEvent->node->NodeID);//視聴中
				usedcnt[where[0]][TopEvent->node->movieID] += 1;//動画の視聴回数を+1;
			}

			state[1] = 0;//クラウドエッジ間通信flag
			TopEvent = AddEvent(time,TopEvent->startTime,TopEvent->clientID,where,state,type,TopEvent->node)->next;
			return;

		}else {//3番目
			if(backFlag) {//すでにpieceが保存されていた
				time = TopEvent->time + EdgeMovieData/hot[where[0]].get_request_bandWidth();//エッジクライアント間通信
				TopEvent->node->NodeID += 1;
				state[1] = 1;
				TopEvent = AddEvent(time,TopEvent->startTime,TopEvent->clientID,where,state,type,TopEvent->node)->next;
					
			}else{//一番近いエッジにはまだpieceが保存されていない
				for(int i = hot[where[0]].get_watchingcnt(TopEvent->node->movieID,TopEvent->node->NodeID); i>0; i--){
					hot[where[0]].add_numOfRequest(1);//エッジ間通信
				}		
				waitingFlag[where[0]][TopEvent->node->movieID][TopEvent->node->NodeID]=1;
				time = TopEvent->time + EdgeMovieData/hot[where[0]].get_request_bandWidth();//エッジクライアント間通信
				TopEvent->node->NodeID += 1;
				state[1] = 1;
				TopEvent = AddEvent(time,TopEvent->startTime,TopEvent->clientID,where,state,type,TopEvent->node)->next;
			}
		}

	}
}

void ExecuteStartMovie()
{

}

void ExecuteWatchingMovie()
{

}

void ExecuteFinishMovie()
{

}




