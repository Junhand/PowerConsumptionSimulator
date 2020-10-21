#include "Lfu.h"



void lfuInitialize()
{
    for(int i=0; i<EDGENUM; i++)
	{ 
		for(int j=0; j<MOVIENUM; j++) 
		{
			p[i][j]=9999;
			usedcnt[i][j]=0;
		}
	}
}

int getHitEdge(int edge, int movieID)
{
    int hit=COLD;
	if(hot[edge].get_movieLength(movieID) != 0) return edge;

    for(int i=0; i<EDGENUM; i++)
    {
        if(hot[i].get_movieLength(movieID) != 0)
        {
            hit=i;
            break;
        }
 
    }
    return hit;
}

int isLfuHit(int edge, int movieID, int movieSize)
{
    int hit=0;
    for(int i=0; i<MOVIENUM; i++)
    {
        if(hot[edge].get_movieLength(movieID) >= movieSize)
        {
            hit=1;
            break;
        }
 
    }
    return hit;
}

int getLfuHitIndex(int edge, int movieID)
{
    int hitind;
    for(int k=0; k<MOVIENUM; k++)
    {
        if(p[edge][k]==movieID)
        {
            hitind=k;
            break;
        }
    }
    return hitind;
}



int lfu(int where[2], int movieID, int movieSize, double bitRate)
{
    int least = 9999;
	int leastList[LEASTNUM];
	for(int i=0;i<LEASTNUM;i++) leastList[i] = 9999;
	int leastind=0;
	int repin = 0;
	int flag = 0;
	
    printf("movieID%d edge%d volume %.0f from%d \n",movieID,where[0],hot[where[0]].get_volume(),where[1]);
    if(isLfuHit(where[0],movieID,movieSize))
    {//exist all movie in edge
        int hitind=getLfuHitIndex(where[0],movieID);
        usedcnt[where[0]][hitind] += 1;
        printf("No page fault!\n");
		return HOT;
    }
    else
    {
		if(where[0] == where[1])
		{//get from the edge but now edge dont have remianing part of movie 
			if(hot[where[0]].get_volume()+(MOVIELENGTH-INTROLENGTH)*bitRate <= EDGEVOLUME)
			{//can store
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

				if(isLfuHit(where[1],movieID,movieSize))
				{//from other edge
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
					else if(INTROLENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
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
					else if(INTROLENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
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
				else if(INTROLENGTH*bitRate <= EDGEVOLUME - hot[where[0]].get_volume() + leastind*(MOVIELENGTH-INTROLENGTH)*bitRate)
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