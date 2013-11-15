#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>

#include <TROOT.h>
#include <TFile.h>
#include <TTree.h>

#include "GeomSvc.h"
#include "SRawEvent.h"

using namespace std;

int main(int argc, char *argv[])
{
  //Initialize geometry service
  GeomSvc* geometrySvc = GeomSvc::instance();
  geometrySvc->init(GEOMETRY_VERSION);

  //Old data
  SRawEvent* event_old = new SRawEvent();

  TFile* dataFile = new TFile(argv[1], "READ");
  TTree* dataTree = (TTree *)dataFile->Get("save");

  dataTree->SetBranchAddress("rawEvent", &event_old);
 
  SRawEvent* event = new SRawEvent();

  TFile *saveFile = new TFile(argv[2], "recreate");
  TTree *saveTree = new TTree("save", "save");

  saveTree->Branch("rawEvent", &event, 256000, 99);
 
  double h_center[16] = {634., 634., 638., 638., 646., 646., 638., 642., 730., 732., 738., 738., 738., 736., 728., 730.};
  for(int i = 0; i < dataTree->GetEntries(); i++)
    {
      dataTree->GetEntry(i);
      event->reIndex("a");

      event->setEventInfo(event_old->getRunID(), event_old->getSpillID(), event_old->getEventID());
      std::vector<Hit> hits_old = event_old->getAllHits();
    
      for(unsigned int j = 0; j < hits_old.size(); j++)
	{
	  if(hits_old[j].detectorID >= 25 && hits_old[j].detectorID <= 40)
	    {
	      if(fabs(hits_old[j].tdcTime - h_center[hits_old[j].detectorID - 25]) > 15.) continue;
	    }

	  if(hits_old[j].detectorID > 40)
	    {
	      if(hits_old[j].tdcTime < 1000. || hits_old[j].tdcTime > 1900.) continue;
	    }

	  Hit h = hits_old[j];
	  event->insertHit(h);
	}

      saveTree->Fill();
      
      event_old->clear();
      event->clear();
    }

  saveFile->cd();
  saveTree->Write();
  saveFile->Close();

  if(argc > 3)
    {
      char cmd[300];
      sprintf(cmd, "mv %s %s", argv[2], argv[1]);
      cout << cmd << endl;
      system(cmd);
    }

  return 1;
}