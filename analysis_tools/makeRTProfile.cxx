#include <iostream>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>

#include <TROOT.h>
#include <TFile.h>
#include <TTree.h>
#include <TProfile.h>
#include <TCanvas.h>
#include <TSQLServer.h>
#include <TSQLResult.h>
#include <TSQLRow.h>
#include <TClonesArray.h>
#include <TF1.h>
#include <TH1.h>
#include <Math/Factory.h>
#include <Math/Functor.h>
#include <Math/Minimizer.h>

#include "GeomSvc.h"
#include "SRawEvent.h"
#include "FastTracklet.h"

using namespace std;

bool acceptTracklet(Tracklet& tracklet)
{
  if(tracklet.getNHits() < 16) return false;
  if(tracklet.getProb() < 0.8) return false;
  if(1./tracklet.invP < 25.) return false;

  return true; 
}

int main(int argc, char *argv[])
{
  GeomSvc *p_geomSvc = GeomSvc::instance();
  p_geomSvc->init(GEOMETRY_VERSION);

  ///Retrieve the maximum and minimum of tdc time spectra
  TSQLServer *con = TSQLServer::Connect("mysql://seaquel.physics.illinois.edu", "seaguest","qqbar2mu+mu-");
  char query[300];
  double tmin[24], tmax[24], tmid[24];
  double rmin[24], rmax[24];
  double r_cutoff_low[24], r_cutoff_high[24];
  double t_cutoff_low[24], t_cutoff_high[24];
  double t0[24] = {1195., 1195., 1195., 1195., 1195., 1195., 1165., 1165., 1165., 1165., 1165., 1165., 1035., 1035., 1035., 1035., 1035., 1035., 1158., 1158., 1158., 1158., 1158., 1158.};
  int nbin[24];
  for(int i = 0; i < 24; i++)
    {
      string detectorName = p_geomSvc->getDetectorName(i+1);
      sprintf(query, "SELECT MIN(tdcTime),MAX(tdcTime) FROM run_002167_R003.Hit WHERE detectorName LIKE '%s' AND inTime=1", detectorName.c_str());

      TSQLResult *res = con->Query(query);
      TSQLRow *row = res->Next();

      tmin[i] = atoi(row->GetField(0));
      tmax[i] = atoi(row->GetField(1));
      tmid[i] = tmin[i] + 2./3.*(tmax[i] - tmin[i]);

      double ratio = 9./10.;
      if(i < 6)
	{
	  ratio = 8./10.;
	}

      t_cutoff_low[i] = tmin[i] + 1./10.*(tmax[i] - tmin[i]);
      t_cutoff_high[i] = tmin[i] + ratio*(tmax[i] - tmin[i]);

      rmin[i] = 0.;
      rmax[i] = p_geomSvc->getPlaneSpacing(i+1)/2.;

      r_cutoff_low[i] = rmin[i] + 1./10.*(rmax[i] - rmin[i]);
      r_cutoff_high[i] = rmin[i] + ratio*(rmax[i] - rmin[i]);

      double binSize = 5.;
      if(detectorName.find("D3p") != string::npos)
	{
	  binSize = 10.;
	}
      nbin[i] = int((tmax[i] - tmin[i] + 0.1)/binSize);
     
      Log(detectorName << ": " << tmin[i] << "  " << rmin[i] << " <====> " << tmax[i] << "  " << rmax[i] << " : " << nbin[i] << " : " << r_cutoff_low[i] << "   " << r_cutoff_high[i]);
      delete res;
      delete row;
    }

  delete con;

  //Profiles of R-T and T-R
  TProfile* prof_RT[24];
  //TProfile* prof_TR[24];
  for(int i = 0; i < 24; i++)
    {
      char name[50];
      sprintf(name, "%s_RT", p_geomSvc->getDetectorName(i+1).c_str());
      prof_RT[i] = new TProfile(name, name, nbin[i], tmin[i], tmax[i]);

      //sprintf(name, "%s_TR", p_geomSvc->getDetectorName(i+1).c_str());
      //prof_TR[i] = new TProfile(name, name, nbin[i], rmin[i], rmax[i]);
    }

  //Load tracks
  TClonesArray* tracklets = new TClonesArray("Tracklet");
  tracklets->Clear();

  TFile* dataFile = new TFile(argv[1], "READ");
  TTree* dataTree = (TTree*)dataFile->Get("save");

  dataTree->SetBranchAddress("tracklets", &tracklets);

  int detectorID;
  double r, r_exp, t;

  TFile* saveFile = new TFile("cali_eval.root", "recreate");
  TTree* saveTree = new TTree("save", "save"); 

  saveTree->Branch("detectorID", &detectorID, "detectorID/I");
  saveTree->Branch("r_exp", &r_exp, "r_exp/D");
  saveTree->Branch("r", &r, "r/D");
  saveTree->Branch("t", &t, "t/D");

  int nEvtMax = dataTree->GetEntries();
  for(int i = 0; i < nEvtMax; i++)
    {
      dataTree->GetEntry(i);

      int nTracklets = tracklets->GetEntries();
      for(int j = 0; j < nTracklets; j++)
	{
	  Tracklet* trk = (Tracklet*)tracklets->At(j);
	  if(!acceptTracklet(*trk)) continue;

	  for(std::list<SignedHit>::iterator iter = trk->hits.begin(); iter != trk->hits.end(); ++iter)
	    {
	      if(iter->hit.index < 0) continue;

	      detectorID = iter->hit.detectorID;
	      r = iter->hit.driftDistance;
	      t = iter->hit.tdcTime;
	      r_exp = trk->getExpPositionW(detectorID) - iter->hit.pos;

	      //Log(detectorID << " " << trk->getExpPositionW(detectorID) << " " << iter->hit.pos);

	      prof_RT[detectorID-1]->Fill(t, fabs(r_exp));
	      //prof_TR[detectorID-1]->Fill(fabs(r_exp), t);
	    
	      saveTree->Fill();
	    }
	}
       
      tracklets->Clear();
    }

  //Output the temporary knots
  fstream fout;
  fout.open(argv[2], ios::out);
  for(int i = 0; i < 24; i++)
    {
      //Extract data
      int nKnots = 0;
      double R[100], T[100];
      for(int j = 1; j <= nbin[i]; j++)
	{
	  double center = prof_RT[i]->GetBinCenter(j);
	  //if(center < t_cutoff_low[i] || center > t_cutoff_high[i]) continue;
	  if(center > t_cutoff_high[i]) continue;

	  R[nKnots] = prof_RT[i]->GetBinContent(j);
	  T[nKnots] = prof_RT[i]->GetBinCenter(j);
	  ++nKnots;
	}

      //Add constrains at Tmax
      R[nKnots] = rmin[i];
      T[nKnots] = tmax[i];
      ++nKnots;

      //Enforce smoothness
      /*
      //Simple method 1
      for(int j = 1; j < nKnots - 1; j++)
	{
	  if((R[j-1] - R[j])*(R[j] - R[j+1]) < 0.) R[j] = 0.5*(R[j-1] + R[j+1]);
	}
      */

      //ROOT TH1 method
      TH1::SmoothArray(nKnots, R, 2);
      //if(i == 0 || i == 1) TH1::SmoothArray(nKnots, R, 50);

      /*
      //Recursive method 2
      bool isMono = false;
      while(!isMono)
	{
	  isMono = true;
	  for(int j = 1; j < nKnots - 1; j++)
	    {
	      if((R[j-1] - R[j])*(R[j] - R[j+1]) < 0.)
		{
		  R[j] = 0.5*(R[j-1] + R[j+1]);
		  isMono = false;
		}
	    }
	}
      */

      //Final output
      fout << i+1 << "  " << nKnots << "  " << tmin[i] << "  " << tmax[i] << "  " << p_geomSvc->getDetectorName(i+1) << endl;
      for(int j = 0; j < nKnots; j++)
	{
	  fout << j << "  " << T[j] << "  " << R[j] << endl;
	}
    }
  fout.close();

  //
  p_geomSvc->loadCalibration(argv[2]);

  TCanvas *c1 = new TCanvas();
  c1->Divide(4, 6);
  for(int i = 0; i < 24; i++)
    {
      c1->cd(i+1);

      prof_RT[i]->Draw();
      prof_RT[i]->GetYaxis()->SetRangeUser(-0.1*rmax[i], 1.1*rmax[i]);
      p_geomSvc->getRTCurve(i+1)->Draw("lcpsame");
    }
  c1->SaveAs("QACali.eps");

  saveFile->cd();
  saveTree->Write();
  saveFile->Close();
  
  if(argc == 3) return 1;

  fout.open(argv[3], ios::out);
  for(int i = 0; i < 24; i++)
    {
      fout << p_geomSvc->getDetectorName(i+1) << endl;
      double tdcTime = tmin[i];
      while(tdcTime <= tmax[i])
	{
	  fout << tdcTime << "  " << p_geomSvc->getRTCurve(i+1)->Eval(tdcTime) << endl;
	  tdcTime += 2.5;
	}
    }
  fout.close();

  if(argc == 4) return 1;
  for(int i = 0; i < 24; i++)
    {
      fout.open((p_geomSvc->getDetectorName(i+1) + string(".tsv")).c_str(), ios::out);
      double tdcTime = tmin[i];
      while(tdcTime <= tmax[i])
	{
	  fout << t0[i] + 12. - tdcTime << "  " << p_geomSvc->getRTCurve(i+1)->Eval(tdcTime) << "  " << p_geomSvc->getPlaneResolution(i+1) << endl;
	  tdcTime += 2.5;
	}
      fout.close();
    }

  return 1;
}