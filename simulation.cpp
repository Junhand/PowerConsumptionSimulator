
#pragma warning(disable:4100)
//#include "stdafx.h"
#include <stdio.h>
#include "stdlib.h"
#include "math.h"
#include <string.h>
#include <random>
#include <iostream>

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


#define NUMBEROFCUSTOMER 20 //Number of customers
#define EDGENUM 1 //Number of edges
#define EDGEVOLUME 400// volume of edges (e.g.1TB:800Mbit)

#define MOVIENUM 5 //Number of movies
#define MOVIELENGTH 60 //Video length
#define INTROLENGTH 10 //The length of the video stored on the edge
#define MOVIEPIECESIZE 5 //segment size
#define INTROPIECE INTROLENGTH/MOVIEPIECESIZE
#define LASTPIECE MOVIELENGTH/MOVIEPIECESIZE
#define LEASTNUM 1+(INTROLENGTH+MOVIELENGTH-INTROLENGTH-1)/(MOVIELENGTH-INTROLENGTH)

double bandWidthECL=1000;//Bandwidth between Edge and Client (e.g.1Gbps)
double bandWidthCE=200;//Bandwidth between Cloud and Edge (e.g.200Mbps)
double bandWidthEE=10000;//bandwidth between an Edge and the other Edge (e.g.10Gbps)
double bitRate[BITRATENUM] = {0.5, 1, 1.5, 2, 2.5, 5};//bitrate Mbps
double VarianceArrivalInterval;
double AverageArrivalInterval;
double lambda;//average number of views　divides time(s)
int range;//Determine the range of uniform distribution(e.g. 10)
int RandType;//Determine the distribution to be used
double simtime = 0.0;//Time in Simulation
double waittime = 0.0;
int usedcnt[EDGENUM][MOVIENUM];//lfu count
int p[EDGENUM][MOVIENUM][LASTPIECE];//lfu
//int watchingcnt[EDGENUM][MOVIENUM];

//////////////////////////////////////////////////////////////////               CLASS               ////////////////////////////////////////////////////////////////////////
class Storage{
	int movieID[MOVIENUM];// e.g. hikakin 
	//int introLength[MOVIENUM][];//The intro time of the video in storage
	int moviePiece[MOVIENUM][MOVIELENGTH/MOVIEPIECESIZE];//The time of the video in storage
	int watchingcnt[MOVIENUM][MOVIELENGTH/MOVIEPIECESIZE];
	double edgeBandwidth;
	double requestBandWidth;
	static double tmpEdgeBandWidth;//bandWidth
	double tmpRequestBandWidth;//bandWidth
	int numOfRequest;//number of customer
	static int numOfEdgeRequest;//number of Request
	double volume;//edgeの容量
	double sumReadData;//読み込んだデータ量
	double sumWriteData;//書き込まれたデータ量
	double readTime;//total reading time
	double writeTime;//total writing time

  public:
	Storage(){
		for(int i=0;i<MOVIENUM;i++)
		{
			movieID[i]=-1;//no movie
			//introLength[i]=0;//no intro mobie
			for(int j=0;j<LASTPIECE;j++) movieLength[i][j] = 0;// no movie
		} 
		volume=0;//2TB
		numOfRequest=0;
		numOfEdgeRequest=0;
		readTime=0;
		writeTime=0;
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
    int is_moviePieceHit(int movieID, int nodeID){
		if(moviePiece[movieID][nodeID]!=0) return 1;
		else return 0;
	}

	void add_all_watchingcnt(int movieID, int nodeID){ 
		for(int i=nodeID;i<LASTPIECE;i++;) watchingcnt[movieID][i] += 1; 
	}
    void delete_watchingcnt(int movieID, int nodeID){ return watchingcnt[movieID][nodeID] -= 1; }

	void get_watchingcnt(int movieID, int nodeID){ return watchingcnt[movieID][nodeID]; }

	void set_edge_bandWidth(int band){//edge間の帯域幅の初期化
		edgeBandwidth = band;
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
		numOfCustomer+=num; 
		calculateBandwidth(0);	
	}

	void delete_numOfRequest(int num){//edge-clientまたはcloud-edgeのアクセス人数を削除
		numOfCustomer-=num; 
		calculateBandwidth(0);	
	}

    int get_numOfEdgeRequest(){ return numOfEdgeRequest; }//edge間のアクセス人数を得る
	int get_numOfRequest(){ return numOfRequest; }////edge-clientまたはcloud-edgeのアクセス人数を得る

	void calculateBandwidth(int edgeFlag){//帯域幅の計算
		int num = numOfRequest
		if(edgeFlag) num=numOfEdgeRequest;//edge間通信の帯域幅の計算の場合
		if(num>0){
			if(edgeFlag) tmpEdgeBandWidth = edgeBandWidth/numOfEdgeRequest;
			else tmpRequestBandWidth = requestBandWidth/numOfRequest;
		}
		else{//0人の場合
			if(edgeFlag) tmpEdgeBandWidth = edgeBandWidth;
			else tmpRequestBandWidth = requestBandWidth;
		}
	}

	void add_read_data(double data){ sumReadData+=data; }//書き込みデータ量の追加
	double get_sum_readData(){ return sumReadData; }

	void add_write_data(double data){ sumWriteData+=data; }//読み込みデータ量の追加
	double get_sum_writeData(){ return sumWriteData; }

	void add_readTime(double time){ readTime+=time; }
	double get_readTime(){ return readTime; }

	void add_writeTime(double time){ writeTime+=time; }
	double get_writeTime(){ return writeTime; }
};


//////////////////////////////////////////////////////////                    STRUCT                          ///////////////////////////////////////////////////////////////
struct event{
	double time;
	int clientID;//clientID
	int where[2];//The nearest edge number
	int state[2];//cold -3~-1, hot 1~4 :: edge-client 1, edges or cloud-edge 0
	int eventID;//arrival start finish   1 2 3 
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

/*struct movie
{
	int where;//edge1
	double videoTime;
	int	movieID;
	double size;
	int NodeID;
};*/


struct event* TopEvent;
struct node* TopNode[NUMBEROFCUSTOMER];
struct event* RequestEvent;
Storage hot[EDGENUM];
Storage cold;

/////////////////////////////////////////////////////////////////////////     FUNCTION        //////////////////////////////////////////////////////////////////////////////

struct event* AddEvent(double time,int clientID, int where[2],int state[2], int eventID, node* node) {
	struct event* PreviousEvent = NULL;
	struct event* CurrentEvent;
	struct event* NewEvent;

	NewEvent = (struct event*)malloc(sizeof(event));
	NewEvent->time= time;
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
				p[i][j][k]=9999;//edge i, 動画番号 j, piece k
			
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
            break;
    }
    return hit;
}

int getLfuHitIndex(int edge, int movieID, int nodeID) {
    int hitind;
    if(p[edge][movieID][nodeID] != 9999) {//nodeID番目のpieceが入っているところの添字を確認
        hitind = movieID;//p={0,1,9999,3,9999,9999,9999,9999...} 動画3を探索
        break;
    }
    return hitind;
}



int lfu(int where[2], int movieID, int nodeID, double bitRate) {
    int least = 9999;
	int leastList[LEASTNUM];
	for(int i=0;i<LEASTNUM;i++) leastList[i] = 9999;
	int leastind=0;
	int repin = 0;
	int flag = 0;
	
    printf("movieID%d edge%d volume %.0f from%d \n",movieID,where[0],hot[where[0]].get_volume(),where[1]);
    if(isLfuHit(where[0],movieID,nodeID)){//一番近いエッジに要求しているpieceがある
        int hitind=getLfuHitIndex(where[0],movieID);
        usedcnt[where[0]][hitind] += 1;
        printf("No page fault!\n");
		return HOT;

    }else if(where[0] == where[1]) {//どのエッジにも要求しているpieceがない
		if(hot[where[0]].get_volume() + MOVIEPIECESIZE*bitRate <= EDGEVOLUME) {//can store
				printf("edge%d can store movieID%d from cold\n",where[0],movieID);
            	p[where[0]][movieID][nodeID]=nodeID;//p={0,1,9999,9999,4,5,・・・}
            	usedcnt[where[0]][movieID] += 1;//usedcnt={0,0,0,0,0,0,・・・}
				state[0] = COLD;
				//hot[where[0]].delete_volume(INTROLENGTH*bitRate);
				return COLD;
        }

	}else{//どのエッジにも要求しているpieceがない

	}
		if(where[0] == where[1]) {//前回一番近いエッジからpieceを取ってきていたが、今回はpieceがない
			if(hot[where[0]].get_volume() + MOVIEPIECESIZE*bitRate <= EDGEVOLUME) {//can store
				printf("edge%d can store movieID%d from cold\n",where[0],movieID);
            	p[where[0]][movieID]=movieID;//p={0,1,9999,9999,4,5,・・・}
            	usedcnt[where[0]][movieID] += 1;//usedcnt={0,0,0,0,0,0,・・・}
				hot[TopEvent->where[0]].add_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
				//hot[where[0]].delete_volume(INTROLENGTH*bitRate);
				return COLD;
        	}
			else
			{
            	for(int k=0; k<MOVIENUM; k++)
				{
                	if(usedcnt[where[0]][k]<least && watchingcnt[where[0]][k]==0 && p[where[0]][k]!=9999 && k != movieID)
                	{
                    	least=usedcnt[where[0]][k];
                    	repin=k;
                	}
				}
				if(least == 9999) 
				{
					printf("edge%d gets movieID%d from only cold\n",where[0],movieID);
					return ONLYCOLD;
				}
				hot[where[0]].set_movieLength(p[where[0]][repin],INTROLENGTH);//delete movie 
				hot[where[0]].delete_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
				hot[TopEvent->where[0]].add_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
				//hot[where[0]].delete_volume(INTROLENGTH*bitRate);
				printf("edge%d change movieID%d to movieID%d\n",where[0],p[where[0]][repin],movieID);
            	p[where[0]][repin]=9999;
				p[where[0]][movieID]=movieID;
            	usedcnt[where[0]][movieID]+=1;
				return COLD;
			}
			
		}
		else if(where[0] != where[1] && where[1]!=COLD)
		{//get movie from other edge
			if(hot[where[0]].get_volume()+MOVIELENGTH*bitRate <= EDGEVOLUME)
			{
				if(isLfuHit(where[1],movieID,movieSize))
				{
					printf("edge%d can store movieID%d from edge %d\n",where[0],movieID,where[1]);
            		p[where[0]][movieID]=movieID;//p={0,1,9999,9999,4,5,・・・}
            		usedcnt[where[0]][movieID]+=1;//usedcnt={0,0,0,0,0,0,・・・}
					hot[TopEvent->where[0]].add_volume(MOVIELENGTH*bitRate);
					return OTHERHOT;
				}
				else
				{//other edge dont have the remaining part of the movie
					printf("edge%d can store movieID%d from cold\n",where[0],movieID);
            		p[where[0]][movieID]=movieID;//p={0,1,9999,9999,4,5,・・・}
            		usedcnt[where[0]][movieID]+=1;//usedcnt={0,0,0,0,0,0,・・・}
					hot[TopEvent->where[0]].add_volume(MOVIELENGTH*bitRate);
					return COLD;
				}
        	}
			else
			{	//////////////////////////////////////////// lfu //////////////////////////////////////////////////
				for(int i=0; i<LEASTNUM; i++)
				{
					least=9999;
            		for(int k=0; k<MOVIENUM; k++)
					{
                		if(usedcnt[where[0]][k]<least && watchingcnt[where[0]][k]==0 && p[where[0]][k]!=9999 && k != movieID)
                		{
							flag=0;
							for(int i=0;i<LEASTNUM;i++)
							{
								if(leastList[i] == k) flag=-1;
							}
							if(flag!=-1)
							{
								least=usedcnt[where[0]][k];
								repin=k;
							}
                		}
					}

					if(least!=9999)
					{
						for(int i=1;i<LEASTNUM;i++) leastList[i] = leastList[i-1];
						leastList[0] = repin;
						leastind+=1;
					}
				}
				printf("hot leastind%d\n",leastind);
				///////////////////////////////////////////////////////////////////////////////////////////////////

				int maxind=leastind;
				leastind=0;
				if(isLfuHit(where[1],movieID,movieSize))
				{//from other edge
					while(leastind>=0)
					{
					if(MOVIELENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
					{//store all movie 
						for(int i=0;i<leastind;i++)
						{
							hot[where[0]].set_movieLength(p[where[0]][leastList[i]],INTROLENGTH);//delete movie 
							hot[where[0]].delete_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
							printf("movieID%d ",p[where[0]][leastList[i]]);
							p[where[0]][leastList[i]]=9999;
						}

						printf("are deleted to store movieID%d in edge%d from edge%d\n",movieID,where[0],where[1]);
						hot[TopEvent->where[0]].add_volume(MOVIELENGTH*bitRate);
            			p[where[0]][movieID]=movieID;
            			usedcnt[where[0]][movieID]+=1;
						return OTHERHOT;
					}
					else if(leastind<maxind)
					{
						leastind+=1;
						continue;
					}
					else break;
					}

					if(INTROLENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
					{//store intro movie
						for(int i=0;i<leastind;i++)
						{
							hot[where[0]].set_movieLength(p[where[0]][leastList[i]],INTROLENGTH);//delete movie 
							hot[where[0]].delete_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
							printf("movieID%d ",p[where[0]][leastList[i]]);
							p[where[0]][leastList[i]]=9999;
						}
						printf("are deleted to store intro of movieID%d in edge%d from edge%d\n",movieID,where[0],where[1]);
						hot[TopEvent->where[0]].add_volume(INTROLENGTH*bitRate);
						p[where[0]][movieID]=movieID;
            			usedcnt[where[0]][movieID]+=1;
						return INTROHOT;
					}
					else
					{//not store
						printf("edge%d gets movieID%d from only edge%d\n",where[0],movieID,where[1]);
						return ONLYHOT;
					}
				}
				else
				{//from cold
					while(leastind>=0)
					{
					if(MOVIELENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
					{//store all movie 
						for(int i=0;i<leastind;i++)
						{
							hot[where[0]].set_movieLength(p[where[0]][leastList[i]],INTROLENGTH);//delete movie 
							hot[where[0]].delete_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
							printf("movieID%d ",p[where[0]][leastList[i]]);
							p[where[0]][leastList[i]]=9999;
						}
						printf("are deleted to store movieID%d in edge%d from cold\n",movieID,where[0]);
						hot[TopEvent->where[0]].add_volume(MOVIELENGTH*bitRate);
            			p[where[0]][movieID]=movieID;
            			usedcnt[where[0]][movieID]+=1;
						return COLD;
					}
					else if(leastind<maxind)
					{
						leastind+=1;
						continue;
					}
					else break;
					}

					if(INTROLENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
					{//store intro movie
						for(int i=0;i<leastind;i++)
						{
							hot[where[0]].set_movieLength(p[where[0]][leastList[i]],INTROLENGTH);//delete movie 
							hot[where[0]].delete_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
							printf("movieID%d ",p[where[0]][leastList[i]]);
							p[where[0]][leastList[i]]=9999;
						}
						printf("are deleted to store intro of movieID%d in edge%d from cold\n",movieID,where[0]);
						hot[TopEvent->where[0]].add_volume(INTROLENGTH*bitRate);
						p[where[0]][movieID]=movieID;
            			usedcnt[where[0]][movieID]+=1;
						return INTROCOLD;
					}
					else
					{//not store
						printf("edge%d gets movieID%d from only cold\n",where[0],movieID);
						return ONLYCOLD;
					}
				}
			}
		}
		else if(where[1] == COLD)
		{//all edges dont have intro
			if(hot[where[0]].get_volume()+MOVIELENGTH*bitRate <= EDGEVOLUME)
			{
				printf("edge%d can store movieID%d from cold\n",where[0],movieID);
				hot[TopEvent->where[0]].add_volume(MOVIELENGTH*bitRate);
            	p[where[0]][movieID]=movieID;//p={0,1,9999,9999,4,5,・・・}
            	usedcnt[where[0]][movieID]+=1;//usedcnt={0,0,0,0,0,0,・・・}
				return COLD;
        	}
			else
			{	//////////////////////////////////////////// lfu //////////////////////////////////////////////////
				for(int i=0; i<LEASTNUM; i++)
				{
					least=9999;
            		for(int k=0; k<MOVIENUM; k++)
					{
                		if(usedcnt[where[0]][k]<least && watchingcnt[where[0]][k]==0 && p[where[0]][k]!=9999 && k != movieID)
                		{
							flag=0;
							for(int i=0;i<LEASTNUM;i++)
							{
								if(leastList[i] == k) flag=-1;
							}
							if(flag!=-1)
							{
								least=usedcnt[where[0]][k];
								repin=k;
							}
                		}
					}

					if(least!=9999)
					{
						for(int i=1;i<LEASTNUM;i++) leastList[i] = leastList[i-1];
						leastList[0] = repin;
						leastind+=1;
					}
				}
				///////////////////////////////////////////////////////////////////////////////////////////////////
				printf("cold leastind%d\n",leastind);
				int maxind=leastind;
				leastind = 0;

				while(leastind>=0)
				{
				if(MOVIELENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
				{//store all movie 
					for(int i=0;i<leastind;i++)
					{
						hot[where[0]].set_movieLength(p[where[0]][leastList[i]],INTROLENGTH);//delete movie 
						hot[where[0]].delete_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
						printf("movieID%d ",p[where[0]][leastList[i]]);
						p[where[0]][leastList[i]]=9999;
					}
					printf("are deleted to store movieID%d in edge%d from cold\n",movieID,where[0]);
					hot[TopEvent->where[0]].add_volume(MOVIELENGTH*bitRate);
            		p[where[0]][movieID]=movieID;
            		usedcnt[where[0]][movieID]+=1;
					return COLD;
				}
				else if(leastind<maxind)
				{
					leastind+=1;
					continue;
				}
				else break;
				}

				if(INTROLENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
				{//store intro movie
					for(int i=0;i<leastind;i++)
					{
						hot[where[0]].set_movieLength(p[where[0]][leastList[i]],INTROLENGTH);//delete movie 
						hot[where[0]].delete_volume((MOVIELENGTH-INTROLENGTH)*bitRate);
						printf("movieID%d ",p[where[0]][leastList[i]]);
						p[where[0]][leastList[i]]=9999;
					}
					printf("are deleted to store intro of movieID%d in edge%d from cold\n",movieID,where[0]);
					hot[TopEvent->where[0]].add_volume(INTROLENGTH*bitRate);
					p[where[0]][movieID]=movieID;
            		usedcnt[where[0]][movieID]+=1;
					return INTROCOLD;
				}
				else
				{//not store
					printf("edge%d gets movieID%d from only cold\n",where[0],movieID);
					return ONLYCOLD;
				}
			}
			
		}
		else 
		{
			printf("program is mising");
			return 9999;
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
	lambda = 5;
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
    for(int i=0; i<NUMBEROFCUSTOMER; i++;){
        t += RequestOnRand(0);//lamdaを元に計算
		where[0] = RequestOnRand(4);//一番近いエッジ　一様分布に従う
		where[1] = 9999;//一番近いエッジが動画をとってくるところ　初期値は9999 クラウド-1 他のエッジ0~
		state[0] = 9999;
		state[1] = 1;//1:edge-client間通信　0:edge間またはcloud-edge間通信
        TopEvent = AddEvent(t,i,where,state,REQUESTMOVIE,TopNode[i]);//時間　clientID 動画取得場所[2] 取得状態[2] イベント Node
    }

	while(TopEvent){
		std::cout<< "ID"<< TopEvent->clientID<< "time"<< std::setprecision(3)<< TopEvent->time<< "edge"<< TopEvent->where[0] << "from"<< TopEvent->where[1]<< "storage"<< std::setprecision(2)<< TopEvent->storage;
		std::cout<< "event"<< TopEvent->eventID<< "movie"<< TopEvent->node->movieID<< "views" << hot[TopEvent->where[0]].getwatchingcnt[TopEvent->node->movieID][LASTPIECE-1] <<"node" <<TopEvent->node->NodeID;
		std::cout<< "hotClient"<< hot[TopEvent->where[0]].get_numOfCustomer()<< std::setprecision(1)<< hot[TopEvent->where[0]].get_client_bandWidth()<< "Mbps"<< "hotServer"<<  hot[TopEvent->where[0]].get_numOfRequest()<< std::setprecision(1)<< hot[TopEvent->where[0]].get_request_bandWidth()<< "Mbps"<< "cold"<< cold.get_numOfRequest()<< std::setprecision(1)<< cold.get_request_bandWidth()<< "Mbps\n";
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
	double EdgeMovieData;
	int type = REQUESTMOVIE;
	int state[2];
	int where[2];
	int edge;

	if(TopEvent->node->NodeID == INTROPIECE-1){type = STARTMOVIE;}
	//initialize communication
	state[0] = TopEvent->state[0];
	state[1] = TopEvent->state[1];

	if(TopEvent->node->NodeID !=0 ) {
		//前回の通信が終了
		if(state[0] == HOT) hot[TopEvent->where[0]].delete_numOfRequest(1);//一番近いエッジからデータを取ってきていた
		else if(state[0] == OTHERHOT) {//他のエッジからデータを取ってきていた
			if(state[1]) hot[TopEvent->where[0]].delete_numOfRequest(1);//エッジクライアント間通信
			else hot[TopEvent->where[1]].delete_numOfEdgeRequest(1);//エッジ間通信

		}else if(state[0] == ONLYHOT) {//他のエッジから直接データを取ってきていた。動画を保存しない
			hot[TopEvent->where[0]].delete_numOfRequest(1);//エッジクライアント間通信
			hot[TopEvent->where[1]].delete_numOfEdgeRequest(1);//エッジ間通信

		}else if(state[0] == COLD) {//クラウドからデータを取ってきていた
			if(state[1]) {
				hot[TopEvent->where[0]].delete_numOfRequest(1);//エッジクライアント間通信
				hot[TopEvent->where[0]].add_volume(MOVIEPIECESIZE*bitRate);//pieceをエッジに保存
			}
			else cold.delete_numOfRequest(1);//クラウドエッジ間通信

		}else if(state[0] == ONLYCOLD) {//クラウドから直接データを取ってきていた
			hot[TopEvent->where[0]].delete_numOfRequest(1);
			cold.delete_numOfRequest(1);
		}
	}

	if(isHitEdge(TopEvent->where[0],TopEvent->node->movieID,TopEvent->node->NodeID))//いづれかのエッジがpieceを持っているかどうか確認
		edge = getHitEdge(TopEvent->where[0],TopEvent->node->movieID,TopEvent->node->NodeID);//pieceを持っているエッジ番号を取得
		if(edge == TopEvent->where[0]) {//一番近いエッジであった
			where[0] = edge;
			where[1] = edge;

		}else{//他のエッジであった
			where[0] = TopEvent->where[0];
			where[1] = edge;
		}

	}else{//どのエッジもpieceを持っていない
		where[0] = TopEvent->where[0];
		where[1] = COLD;
	}

	state[0] = lfu(where, TopEvent->node->movieID, TopEvent->node->NodeID, TopEvent->node->bitRate, TopEvent->state);
	if(TopEvent->node->NodeID !=0 ) {
		//今回の通信
		if(state[0] == HOT) {
			hot[TopEvent->where[0]].delete_numOfRequest(1);//一番近いエッジからデータを取ってきていた

		}else if(state[0] == OTHERHOT) {//他のエッジからデータを取ってきていた
			if(state[1]) hot[TopEvent->where[0]].delete_numOfRequest(1);//エッジクライアント間通信
			else hot[TopEvent->where[1]].delete_numOfEdgeRequest(1);//エッジ間通信

		}else if(state[0] == ONLYHOT) {//他のエッジから直接データを取ってきていた。動画を保存しない
			hot[TopEvent->where[0]].delete_numOfRequest(1);//エッジクライアント間通信
			hot[TopEvent->where[1]].delete_numOfEdgeRequest(1);//エッジ間通信

		}else if(state[0] == COLD) {//クラウドからデータを取ってくる
			if(state[1]) {
				where[0] = TopEvent->where[0];
				where[1] = COLD;
				state[1] = 0;
				TopEvent = AddEvent(time,TopEvent->clientID,where,COLD,type,TopEvent->node)->next;
			}
			else cold.delete_numOfRequest(1);//クラウドエッジ間通信

		}else if(state[0] == ONLYCOLD) {//クラウドから直接データを取ってきていた
			hot[TopEvent->where[0]].delete_numOfRequest(1);
			cold.delete_numOfRequest(1);
		}
	}

	if(isHitEdge(TopEvent->where[0],TopEvent->node->movieID,TopEvent->node->NodeID))//intro exist
	{
		edge = getHitEdge(TopEvent->where[0],TopEvent->node->movieID,TopEvent->node->NodeID);//check if an edge have the movie

		if(edge == TopEvent->where[0])
		{//communicate with only edge
			where[0] = edge;
			where[1] = edge;
			hot[edge].add_numOfCustomer(1);// nunber of request in edge +1
			EdgeMovieData = MOVIEPIECESIZE*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[edge].get_client_bandWidth();//10G 1G ...
			TopEvent->node->NodeID += MOVIEPIECESIZE;
			TopEvent = AddEvent(time,TopEvent->clientID,where,HOT,type,TopEvent->node)->next;
		}
		else
		{//communicate with other edge and nearest edge

			where[0] = TopEvent->where[0];
			where[1] = edge;
			if(TopEvent->node->NodeID==0)
			{
				hot[TopEvent->where[0]].add_numOfCustomer(1); // nunber of request in edge +1
				hot[edge].add_numOfCustomer(1);// nunber of request in edge +1
			}
			EdgeMovieData = MOVIEPIECESIZE*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[edge].get_bandWidth() + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();//10G 1G ...
			TopEvent->node->NodeID += MOVIEPIECESIZE;
			TopEvent = AddEvent(time,TopEvent->clientID,where,OTHERHOT,type,TopEvent->node)->next;
		}
	}
	else//intro not exist
	{
		where[0] = TopEvent->where[0];
		where[1] = COLD;
		if(TopEvent->node->NodeID==0) 
		{
			hot[TopEvent->where[0]].add_numOfCustomer(1);// nunber of request in edge +1
			cold.add_numOfCustomer(1);// nunber of request in cloud +1
		}
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/cold.get_bandWidth() + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID,where,COLD,type, TopEvent->node)->next;
	}
}

void ExecuteStartMovie()
{
	double time;
	double bitRate = TopEvent->node->bitRate;
	int communication = TopEvent->storage;
	int where[2];
	double EdgeMovieData;
	int state;
	
	//initialize communication
	if(communication == HOT) hot[TopEvent->where[0]].delete_numOfCustomer(1);
	else if(communication == OTHERHOT)
	{
		hot[TopEvent->where[0]].delete_numOfCustomer(1);
		hot[TopEvent->where[1]].delete_numOfCustomer(1);
	}
	else if(communication == COLD)
	{
		hot[TopEvent->where[0]].delete_numOfCustomer(1);
		cold.delete_numOfCustomer(1);
	}

	state = lfu(TopEvent->where, TopEvent->node->movieID, TopEvent->node->NodeID+MOVIEPIECESIZE, TopEvent->node->bitRate);
	if(state == HOT)
	{
		hot[TopEvent->where[0]].add_numOfCustomer(1);
		watchingcnt[TopEvent->where[0]][TopEvent->node->movieID]+=1;
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where, HOT, WATCHINGMOVIE, TopEvent->node)->next;
	}
	else if(state == OTHERHOT)
	{
		if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<INTROLENGTH)
		{
			//hot[TopEvent->where[0]].add_volume(INTROLENGTH*bitRate);
			hot[TopEvent->where[0]].set_movieID(TopEvent->node->movieID,TopEvent->node->movieID);
			hot[TopEvent->where[0]].set_introLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
			hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
			hot[TopEvent->where[1]].add_numOfCustomer(1);
			watchingcnt[TopEvent->where[1]][TopEvent->node->movieID]+=1;
		}
		hot[TopEvent->where[0]].add_numOfCustomer(1);
		watchingcnt[TopEvent->where[0]][TopEvent->node->movieID]+=1;
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[1]].get_bandWidth() + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where, OTHERHOT, WATCHINGMOVIE, TopEvent->node)->next;
	}
	else if(state == ONLYHOT)
	{
		hot[TopEvent->where[1]].add_numOfCustomer(1);
		where[0] = TopEvent->where[1];
		where[1] = TopEvent->where[1];
		watchingcnt[TopEvent->where[1]][TopEvent->node->movieID]+=1;
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[1]].get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID, where, ONLYHOT, WATCHINGMOVIE, TopEvent->node)->next;
	}
	else if(state == INTROHOT)
	{
		hot[TopEvent->where[1]].add_numOfCustomer(1);
		where[0] = TopEvent->where[1];
		where[1] = TopEvent->where[1];
		watchingcnt[TopEvent->where[1]][TopEvent->node->movieID]+=1;
		if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<INTROLENGTH)
		{
			//hot[TopEvent->where[0]].add_volume(INTROLENGTH*bitRate);
			hot[TopEvent->where[0]].set_movieID(TopEvent->node->movieID,TopEvent->node->movieID);
			hot[TopEvent->where[0]].set_introLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
			hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
		}
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[1]].get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID, where, ONLYHOT, WATCHINGMOVIE, TopEvent->node)->next;
	}
	else if(state == COLD)
	{
		hot[TopEvent->where[0]].add_numOfCustomer(1);
		where[0] = TopEvent->where[0];
		where[1] = COLD;
		watchingcnt[TopEvent->where[0]][TopEvent->node->movieID]+=1;
		if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<INTROLENGTH)
		{
			//hot[TopEvent->where[0]].add_volume(INTROLENGTH*bitRate);
			hot[TopEvent->where[0]].set_movieID(TopEvent->node->movieID,TopEvent->node->movieID);
			hot[TopEvent->where[0]].set_introLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
			hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
			cold.add_numOfCustomer(1);
		}
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/cold.get_bandWidth()+ EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID,where,COLD,WATCHINGMOVIE, TopEvent->node)->next;
	}
	else if(state == ONLYCOLD)
	{
		cold.add_numOfCustomer(1);
		where[0] = COLD;
		where[1] = COLD;
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/cold.get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID,where,ONLYCOLD,WATCHINGMOVIE, TopEvent->node)->next;
	}
	else if(state == INTROCOLD)
	{
		cold.add_numOfCustomer(1);
		where[0] = COLD;
		where[1] = COLD;
		if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<INTROLENGTH)
		{
			//hot[TopEvent->where[0]].add_volume(INTROLENGTH*bitRate);
			hot[TopEvent->where[0]].set_movieID(TopEvent->node->movieID,TopEvent->node->movieID);
			hot[TopEvent->where[0]].set_introLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
			hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,INTROLENGTH);//finish storeing intro to the edge
		}
		EdgeMovieData = MOVIEPIECESIZE*bitRate;
		time = TopEvent->time + EdgeMovieData/cold.get_bandWidth();
		TopEvent->node->NodeID += MOVIEPIECESIZE;
		TopEvent = AddEvent(time,TopEvent->clientID,where,ONLYCOLD,WATCHINGMOVIE, TopEvent->node)->next;
	}
	else printf("miss start movie");
}

void ExecuteWatchingMovie()
{
	double time;
	double bitRate = TopEvent->node->bitRate;
	double EdgeMovieData;
	int state = TopEvent->storage;
	
	if(TopEvent->node->NodeID < MOVIELENGTH-MOVIEPIECESIZE)//continue request
	{
		if(state == HOT)
		{
			EdgeMovieData = MOVIEPIECESIZE*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
			TopEvent->node->NodeID += MOVIEPIECESIZE;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where, HOT, WATCHINGMOVIE, TopEvent->node)->next;
		}
		else if(state == OTHERHOT)
		{
			if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<TopEvent->node->NodeID)
			{
				//hot[TopEvent->where[0]].add_volume(MOVIEPIECESIZE*bitRate);
				hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,TopEvent->node->NodeID);//finish storeing intro to the edge
			}
			EdgeMovieData = MOVIEPIECESIZE*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[1]].get_bandWidth() + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
			TopEvent->node->NodeID += MOVIEPIECESIZE;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where, OTHERHOT, WATCHINGMOVIE, TopEvent->node)->next;
		}
		else if(state == ONLYHOT)
		{
			EdgeMovieData = MOVIEPIECESIZE*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[1]].get_bandWidth();
			TopEvent->node->NodeID += MOVIEPIECESIZE;
			TopEvent = AddEvent(time,TopEvent->clientID, TopEvent->where, ONLYHOT, WATCHINGMOVIE, TopEvent->node)->next;
		}
		else if(state == COLD)
		{
			if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<TopEvent->node->NodeID)
			{
				//hot[TopEvent->where[0]].add_volume(MOVIEPIECESIZE*bitRate);
				hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,TopEvent->node->NodeID);//finish storeing intro to the edge
			}
			EdgeMovieData = MOVIEPIECESIZE*bitRate;
			time = TopEvent->time + EdgeMovieData/cold.get_bandWidth()+ EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
			TopEvent->node->NodeID += MOVIEPIECESIZE;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where,COLD,WATCHINGMOVIE, TopEvent->node)->next;
		}
		else if(state == ONLYCOLD)
		{
			EdgeMovieData = MOVIEPIECESIZE*bitRate;
			time = TopEvent->time + EdgeMovieData/cold.get_bandWidth();
			TopEvent->node->NodeID += MOVIEPIECESIZE;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where,ONLYCOLD,WATCHINGMOVIE, TopEvent->node)->next;
		}
		else printf("miss watching movie");
	}
	else //last request
	{
		if(state == HOT)
		{
			EdgeMovieData = (MOVIELENGTH - TopEvent->node->NodeID)*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
			TopEvent->node->NodeID = MOVIELENGTH;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where, HOT, FINISHMOVIE, TopEvent->node)->next;
		}
		else if(state == OTHERHOT)
		{
			if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<TopEvent->node->NodeID)
			{
				//hot[TopEvent->where[0]].add_volume(MOVIEPIECESIZE*bitRate);
				hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,TopEvent->node->NodeID);//finish storeing intro to the edge
			}
			EdgeMovieData = (MOVIELENGTH - TopEvent->node->NodeID)*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[1]].get_bandWidth() + EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
			TopEvent->node->NodeID = MOVIELENGTH;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where, OTHERHOT, FINISHMOVIE, TopEvent->node)->next;
		}
		else if(state == ONLYHOT)
		{
			EdgeMovieData = (MOVIELENGTH - TopEvent->node->NodeID)*bitRate;
			time = TopEvent->time + EdgeMovieData/hot[TopEvent->where[1]].get_bandWidth();
			TopEvent->node->NodeID = MOVIELENGTH;
			TopEvent = AddEvent(time,TopEvent->clientID, TopEvent->where, ONLYHOT, FINISHMOVIE, TopEvent->node)->next;
		}
		else if(state == COLD)
		{
			if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<TopEvent->node->NodeID)
			{
				//hot[TopEvent->where[0]].add_volume(MOVIEPIECESIZE*bitRate);
				hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID,TopEvent->node->NodeID);//finish storeing intro to the edge
			}
			EdgeMovieData = (MOVIELENGTH - TopEvent->node->NodeID)*bitRate;
			time = TopEvent->time + EdgeMovieData/cold.get_bandWidth()+ EdgeMovieData/hot[TopEvent->where[0]].get_bandWidth();
			TopEvent->node->NodeID = MOVIELENGTH;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where,COLD,FINISHMOVIE, TopEvent->node)->next;
		}
		else if(state == ONLYCOLD)
		{
			EdgeMovieData = (MOVIELENGTH - TopEvent->node->NodeID)*bitRate;
			time = TopEvent->time + EdgeMovieData/cold.get_bandWidth();
			TopEvent->node->NodeID = MOVIELENGTH;
			TopEvent = AddEvent(time,TopEvent->clientID,TopEvent->where,ONLYCOLD,FINISHMOVIE, TopEvent->node)->next;
		}
		else printf("miss last watching movie");
	}
}

void ExecuteFinishMovie()
{
	int state = TopEvent->storage;
	if(state == HOT)
	{
		watchingcnt[TopEvent->where[0]][TopEvent->node->movieID]-=1;
		hot[TopEvent->where[0]].delete_numOfCustomer(1);
	}
	else if(state == OTHERHOT)
	{
		watchingcnt[TopEvent->where[0]][TopEvent->node->movieID]-=1;
		hot[TopEvent->where[0]].delete_numOfCustomer(1);
		if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<MOVIELENGTH)
		{
			watchingcnt[TopEvent->where[1]][TopEvent->node->movieID]-=1;
			hot[TopEvent->where[1]].delete_numOfCustomer(1);
			hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID, MOVIELENGTH);
			//hot[TopEvent->where[0]].add_volume(MOVIEPIECESIZE*TopEvent->node->bitRate);
		}
	}
	else if(state == ONLYHOT)
	{
		watchingcnt[TopEvent->where[1]][TopEvent->node->movieID]-=1;
		hot[TopEvent->where[1]].delete_numOfCustomer(1);
	}
	else if(state == COLD)
	{
		watchingcnt[TopEvent->where[0]][TopEvent->node->movieID]-=1;
		hot[TopEvent->where[0]].delete_numOfCustomer(1);
		if(hot[TopEvent->where[0]].get_movieLength(TopEvent->node->movieID)<MOVIELENGTH)
		{
			cold.delete_numOfCustomer(1);
			hot[TopEvent->where[0]].set_movieLength(TopEvent->node->movieID, MOVIELENGTH);
			//hot[TopEvent->where[0]].add_volume(MOVIEPIECESIZE*TopEvent->node->bitRate);
		}
	}
	else if(state == ONLYCOLD)
	{
		cold.delete_numOfCustomer(1);
	}
	else printf("miss last watching movie");

	TopEvent = TopEvent->next;
}




