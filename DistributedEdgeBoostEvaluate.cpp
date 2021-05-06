void EvaluateLambda() {
	int k, l, n;
	double i,j;
	char FileName[64];
	FILE* ResultFile;
	double CloudEdgeBandwidth, EdgeEdgeBandwidth, EdgeClientBandwidth;
	double AveInterruptDuration, AveNumInterrupt, MaxInterrupt, MinInterrupt, MinAveInterrupt, EdgeVolume;
	double TotalPowerConsumption=0;

	RandType = 0;//0:一定、1:指数
	CloudEdgeBandwidth =  1000000000.0;//1Gbps
	EdgeEdgeBandwidth =   1000000000.0;//1Gbps
	EdgeClientBandwidth =  900000000.0;//1Gbps 非同期通信のために帯域幅が上2つと同じ場合は少し早くすると良い。しなければバッファがないためクラウドエッジ・エッジクラウドで同期通信のようになってしまう

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
		for (j = 112.5/8*0.5; j <= 112.5/8*0.5; j+=112.5/8*0.5) {//15
			HotCacheNumPieces = (double)j*1000000000/PieceSize; //NumPrePieces = (l + 1) * 10;
			
			MinAveInterrupt = 1.0e32;
			//fprintf(ResultFile, "%lf\t\n", AverageArrivalInterval);
			for (l = 2; l <= 10; l++) {//3
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
				fprintf(ResultFile, "%lf\t%.0lf\t%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%lf\t%lf",AverageArrivalInterval,EdgeVolume, AveInterruptDuration, AveNumInterrupt, NumReceivedClients, TotalEdgeClientReadBytes,TotalEdgeEdgeWriteBytes,TotalEdgeEdgeReadBytes,TotalCloudEdgeWriteBytes,TotalCloudEdgeReadBytes);
				fprintf(ResultFile, "\t%.2lf",CloudServer.CloudPowerConsumption);
				TotalPowerConsumption += CloudServer.CloudPowerConsumption;
				for(int k=0; k< NumEdges;k++){
					fprintf(ResultFile, "\t%.2lf",CloudServer.EdgePowerConsumption[k]);
					TotalPowerConsumption += CloudServer.EdgePowerConsumption[k];
				}
				fprintf(ResultFile,"\t%.2lf",TotalPowerConsumption);
				TotalPowerConsumption = 0;

				fprintf(ResultFile,"\n");
				fflush(ResultFile);
				fclose(LogFile);

			}
			fprintf(ResultFile, "\n");
			fprintf(ServerResultFile, "\n");
			n++;
		}
	
		fprintf(ResultFile, "\n");
		fprintf(ResultFile, "\n");
		fprintf(ServerResultFile, "\n");
		fprintf(ServerResultFile, "\n");
		
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
	int i, j, k, l, m, n;
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
				for(m=0;m<NumEdges;m++){
					fprintf(ResultFile,"%.0lf\t",CloudServer.EdgePowerConsumption[m]);
				}
				fprintf(ResultFile,"%.0lf\t",CloudServer.CloudPowerConsumption);
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