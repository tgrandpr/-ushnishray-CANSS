/*
 * base.cpp
 *
 *  Created on: Aug 19, 2014
 *      Author: ushnish
 *
 	Copyright (c) 2014 Ushnish Ray
	All rights reserved.
 */


#include "dmc.h"
#include "RunParameters.h"
#include "SNSMover.h"

using namespace std;
using namespace __gnu_cxx;


int setup(int rank, string baseSpecFile, int argc, char* argv[])
{
	RunParameters runParams;

	int status = runParams.loadFile(baseSpecFile);
	if(argc==3)
	{
		runParams.beta = atof(argv[2]);
		runParams.trans.lmc = exp(runParams.beta);
		runParams.trans.rmc = exp(-runParams.beta);
	}

	if(status == FILENOTFOUND)
		return FAIL;
	else if(status == DIMERROR)
		return DIMERROR;
	if(rank == 0)
	{
		cout<<"Supplied Argument count: "<<argc<<endl;
		runParams.display();
	}

	//Output Log
	stringstream sp;
	sp << rank;
	string logf = runParams.logFile + "_" + sp.str();
	FILE* log = fopen(logf.c_str(),"a");

	//COMM WORLD
	int totalProcs;
	MPI_Comm_size(MPI_COMM_WORLD,&totalProcs);
	int totalWalkers = (totalProcs-1)*runParams.walkerCount;
	//Time scaler
	double dt = 1.0/(runParams.L+2);

	// All ranks need a collector
	// For rank 0 the global collector and the mpi collector are used exclusively to receive
	// For slave ranks, global collector and mpi-collector are used for local assembly (i.e. per node)

	Weight* freeEnergy = new Weight(0.0);
	WalkerState<int,stringstream>* gwstate = new WalkerState<int,stringstream>(runParams.dimension,*freeEnergy,log);
	vector<Observable<int,stringstream>*> globalObs;
	//Parallel Observables
	vector<MPIObservable*> mpiGlobalObs;

	fprintf(log,"Setting up observables.\n");
	fflush(log);

	for(int i=0;i<runParams.observableCount;i++)
	{
		if(runParams.observableType[i].compare("Basic")==0)
		{
			BasicObs<int,stringstream>* oo = new BasicObs<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log,dt);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("Density")==0)
		{
			Density<int,stringstream>* oo = new Density<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("Qhistogram")==0)
		{
			Qhistogram<int,stringstream>* oo = new Qhistogram<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log,dt);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("Whistogram")==0)
		{
			Whistogram<int,stringstream>* oo = new Whistogram<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log,dt);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("AutoCorr")==0)
		{
			AutoCorr<int,stringstream>* oo = new AutoCorr<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("AutoCorrFull")==0)
		{
			AutoCorrFull<int,stringstream>* oo = new AutoCorrFull<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("AutoCorrI")==0)
		{
			AutoCorrI<int,stringstream>* oo = new AutoCorrI<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("Phistogram")==0)
		{
			Phistogram<int,stringstream>* oo = new Phistogram<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log,dt);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
		else if(runParams.observableType[i].compare("CloneMult")==0)
		{
			CloneMultiplicity<int,stringstream>* oo = new CloneMultiplicity<int,stringstream>(rank,totalProcs,totalWalkers,*gwstate,runParams.observableName[i],log);
			globalObs.push_back(oo);
			mpiGlobalObs.push_back(oo);
		}
	}

	fprintf(log,"Observables setup.\n");
	fflush(log);

	//Other observables
	NumMap<Walker<int,stringstream>> walkerCollection;

	//Setup MPI Runner
	MPIBasicRunner<int,stringstream> *brunner;

	if(rank == 0) //Reserve Master
	{
		fprintf(log,"Starting master run.\n");
		fflush(log);

		gsl_rng_env_setup();
		int procSeed;
		if(runParams.rinitseed == -1)
			runParams.rinitseed = (int) time(NULL);
		gsl_rng* rgenref = gsl_rng_alloc(gsl_rng_mt19937);
		gsl_rng_set(rgenref,runParams.rinitseed + rank);
		fprintf(log,"Seed: %d\n",runParams.rinitseed + rank);

		brunner = new MPIBasicRunner<int,stringstream>(log,totalProcs,
				runParams.eSteps,runParams.bins,runParams.nSteps,runParams.branchStep,
				runParams.walkerCount,globalObs,mpiGlobalObs,rgenref);
		brunner->masterRun();
	}
	else
	{
		//Setup Random Seed for process
		gsl_rng_env_setup();
		int procSeed;
		if(runParams.rinitseed == -1)
			runParams.rinitseed = (int) time(NULL);

		gsl_rng* rgenref = gsl_rng_alloc(gsl_rng_mt19937);
		gsl_rng_set(rgenref,runParams.rinitseed + rank);
		fprintf(log,"Seed: %d\n",runParams.rinitseed + rank);
		fprintf(log,"Number of walkers for current process: %d\n",runParams.walkerCount);
		fflush(log);

		//Setup Mover
		SNSMover<int,stringstream>* mov;
		if(runParams.moverName.compare("SNSMover")==0)
			mov = new SNSMover<int,stringstream>(log,runParams);
		else
			return FAIL;

		//Total number of walkers
		//long totalnwalkers = (totalProcs-1)*runParams.walkerCount;
		double initweight = 1.0;

		//setup walker threads
		for(int w=0;w<runParams.walkerCount;w++)
		{
			//////////////////////////////////////////////////////////////////////////////////////////
			//Prepare Walker State Objects
			//////////////////////////////////////////////////////////////////////////////////////////
			int wid = (rank-1)*runParams.walkerCount + w;

			//State File
			Weight* wt = new Weight(initweight);
			WalkerState<int,stringstream>* wstate = new WalkerState<int,stringstream>(runParams.dimension,*wt,log,wid);

			//Observable Files
			vector<Observable<int,stringstream>*>* localObs = new vector<Observable<int,stringstream>*>;
			for(int i=0;i<runParams.observableCount;i++)
			{
				if(runParams.observableType[i].compare("Basic")==0)
					localObs->push_back(new BasicObs<int,stringstream>(*wstate,runParams.observableName[i],log,dt));
				else if(runParams.observableType[i].compare("Density")==0)
					localObs->push_back(new Density<int,stringstream>(*wstate,runParams.observableName[i],log));
				else if(runParams.observableType[i].compare("Qhistogram")==0)
					localObs->push_back(new Qhistogram<int,stringstream>(*wstate,runParams.observableName[i],log,dt));
				else if(runParams.observableType[i].compare("Whistogram")==0)
					localObs->push_back(new Whistogram<int,stringstream>(*wstate,runParams.observableName[i],log,dt));
				else if(runParams.observableType[i].compare("AutoCorr")==0)
					localObs->push_back(new AutoCorr<int,stringstream>(*wstate,runParams.observableName[i],log));
				else if(runParams.observableType[i].compare("AutoCorrFull")==0)
					localObs->push_back(new AutoCorrFull<int,stringstream>(*wstate,runParams.observableName[i],log));
				else if(runParams.observableType[i].compare("AutoCorrI")==0)
					localObs->push_back(new AutoCorrI<int,stringstream>(*wstate,runParams.observableName[i],log));
				else if(runParams.observableType[i].compare("Phistogram")==0)
					localObs->push_back(new Phistogram<int,stringstream>(*wstate,runParams.observableName[i],log,dt));
				else if(runParams.observableType[i].compare("CloneMult")==0)
					localObs->push_back(new CloneMultiplicity<int,stringstream>(*wstate,runParams.observableName[i],log));
			}

			Walker<int,stringstream>* lwalker = new Walker<int,stringstream>(rgenref,*wstate,*localObs);
			walkerCollection[w] = lwalker;
			//////////////////////////////////////////////////////////////////////////////////////////
		}

		brunner = new MPIBasicRunner<int,stringstream>(log,totalProcs,mov,runParams.eSteps,runParams.bins,
				runParams.nSteps,runParams.branchStep,
				runParams.walkerCount, 1.0e8,1.0e-8,
				&walkerCollection,globalObs,mpiGlobalObs);
		fprintf(log,"Starting spawn runs.\n");

		//Now run
		brunner->run();
	}

	//This is important. Wait for master to finish
	MPI_Barrier(MPI_COMM_WORLD);
	fclose(log);
}

