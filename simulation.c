//#include <iostream>

#pragma warning(disable:4100)
//#include "stdafx.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include <string.h>
#define PI 3.14159265358979323846264338327950288419716939937510 
#define bitRateSize 6

void EvaluatePowerConsumption(void);
void EvaluateRatioOfEdge(void);
double bandWidthECL;//between Edge and Client
double bandWidthCE;//between Cloud and Edge 
double bitRate[bitRateSize] = {0.5, 1, 1.5, 2, 2.5, 3};//bitrate
double VarianceArrivalInterval;
double AverageArrivalInterval;
double lambda;
int RandType;
double simtime = 0.0;//simulationTime

struct event{
	double	time;
	int where;
	struct event* next;
};

struct storage {
	double bandWidth;
	double powerConsumption;
	double price;
};

struct event* TopEvent;
struct event* AddEvent(double time, int where) {
	struct event* CurrentEvent;
	struct event* NewEvent;

	NewEvent = new struct event;
	NewEvent->time= time;
	NewEvent->where = where;

	if (TopEvent == NULL) {
		TopEvent = NewEvent;
		TopEvent->next = NULL;
		return NewEvent;
	}

	CurrentEvent = TopEvent;
	while (CurrentEvent) {
		if (CurrentEvent->time < NewEvent->time) {
			if (CurrentEvent->next != NULL) {
				CurrentEvent = CurrentEvent->next;	
			}
			else {
				CurrentEvent->next = NewEvent;
			}
		}
	}
	return TopEvent;
}


int main(int argc, char* argv[]){
	EvaluateRatioOfEdge();
	//EvaluatePowerConsumption();
	return 0;
}

void EvaluateRatioOfEdge() {
	double latencyECL;//between Edge and Client
	double allMovieData = 800*8;//movie data size (ex.150MB)
	double EdgeMovieData;
	double bandWidthCCL;//between Cloud and Client
	int i,j;

	bandWidthCE = 250*8;//(ex.HDD 170MB/s)
	bandWidthECL = 1940*8;//(ex.SSD 1940MB/s) not change
	bandWidthCCL = bandWidthCE + bandWidthECL;

	
	for(j=0;j<bitRateSize;j++){
		EdgeMovieData = allMovieData*bitRate[j]*bandWidthECL / ( bitRate[j]*(bandWidthCCL) + bandWidthCCL );	
		latencyECL = EdgeMovieData/bandWidthECL + EdgeMovieData/bitRate[j];
		printf("All Movie Data Size %dMB bitRate %lf EdgeMovieData %lf\n",int(allMovieData/8), bitRate[j],EdgeMovieData/allMovieData);
	}
}

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

double RequestOnRand(int randType) {
	double r;
	long double Poisson;
	int i;

	switch (randType) {
	case 0: //指数分布
		r = -log(((double)rand() + 1.0) / ((double)RAND_MAX + 2.0)) / lambda;
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

void Simulation(double arrivalRate, int tmpK=1, int tmpnumServer=1, int n = 1000) {
	int numCustomers;
	int numProcessNumber;
	int K;

    lambda = arrivalRate;
    numCustomers = n;
    numProcessNumber = tmpnumServer;
    K = tmpK;

    if (lambda < 0.0 || lambda > 1.0) printf("負荷は1.0未満で");
    double t = 0.0;
    for (int i = 0; i < numCustomers; i++){
        t += RequestOnRand(0);
        TopEvent = AddEvent(t,0);
    }
}



