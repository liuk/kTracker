#include "JobOptsSvc.h"

#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <wordexp.h> //to expand environmentals
#include <boost/algorithm/string.hpp> //for strip

using namespace std;


//todo a msg service
//#define DEBUG_JobOptsSvc  //comment out for quiet

bool debug()
{
#ifdef DEBUG_JobOptsSvc
  return true;
#else
  return false;
#endif
}

JobOptsSvc* JobOptsSvc::p_jobOptsSvc = NULL;

JobOptsSvc* JobOptsSvc::instance()
{
  if( debug() )
    cout << "JobOptsSvc::instance()" << endl;

  if(NULL == p_jobOptsSvc )
  {
    p_jobOptsSvc = new JobOptsSvc;
    if( debug() )
      cout << " creating new JobOptsSvc object" << endl;
    p_jobOptsSvc->init();
  }


  return p_jobOptsSvc;
}

void JobOptsSvc::close()
{
  if(debug())
    cout << "   JobOptsSvc::close()" << endl;
  if(NULL != p_jobOptsSvc)
  {
    delete p_jobOptsSvc;
  }
  else
  {
    std::cout << "Error: no instance of job options service found! " << std::endl;
  }
}

bool JobOptsSvc::init()
{
  if(debug())
    cout << "JobOptsSvc::init()" << endl;

  //hardcoded default standard job options file
  return init( "$KTRACKER_ROOT/opts/default.opts" );
}

bool JobOptsSvc::init( const char* configfile )
{
  if(debug())
    cout << "JobOptsSvc::init( " << configfile << " )" << endl;

  // expand any environment variables in the file name
  wordexp_t exp_result;
  if (wordexp(configfile, &exp_result, 0) != 0)
  {
    //this is a fatal error so throw
    cout << "JobOptsSvc::init - ERROR - Your config file '" << configfile << "' cannot be understood!" << endl;
    throw 1;
  }

  //todo: maybe this isn't bad and we can use hardcoded defaults in code here?
  std::ifstream fin(exp_result.we_wordv[0]);
  if(!fin) {
    cout << "JobOptsSvc::init - ERROR - Your config file '" << configfile << "' cannot be opened!" << endl;
    throw 1;
  }

  m_configFile.assign( configfile );
  if(debug())
    cout << "My config file is :" << m_configFile << endl;

  //define the types of options to look for
  map<string,string*> stringOpts;
  stringOpts["AlignmentFile_Hodo"] = &m_alignmentFileHodo;
  stringOpts["AlignmentFile_Mille"] = &m_alignmentFileMille;
  stringOpts["AlignmentFile_Prop"] = &m_alignmentFileProp;
  stringOpts["AlignmentFile_Chamber"] = &m_alignmentFileChamber;

  stringOpts["CalibrationsFile"] = &m_calibrationsFile;

  stringOpts["fMagFile"] = &m_fMagFile;
  stringOpts["kMagFile"] = &m_kMagFile;

  stringOpts["MySQL_Server"] = &m_mySQLServer;
  stringOpts["Geometry_Version"] = &m_geomVersion;

  map<string,int*> intOpts;
  intOpts["MySQL_Port"] = &m_mySQLPort;


  map<string,bool*> boolOpts;
  boolOpts["MCMode_enable"] = &m_mcMode;
  boolOpts["AlignmentMode_enable"] = &m_alignmentMode;
  boolOpts["MultiMiniMode_enable"] = &m_multiMini;
  boolOpts["CoarseMode_enable"] = &m_coarseMode;
  boolOpts["DiMuonMode_enable"] = &m_diMuonMode;
  boolOpts["KF_enable"] = &m_enableKF;
  boolOpts["TriggerMask_enable"] = &m_enableTriggerMask;
  boolOpts["kMag_enable"] = &m_enableKMag;

  //read the file and find matching options
  string line;
  while( getline(fin,line) ){
    if(debug())
      cout << "line = " << line << endl;
    boost::algorithm::trim(line);

    //skip empty and comment lines
    if( line.empty() || line[0]=='#' ) 
      continue;

    stringstream ss(line);
    string key, val;
    ss >> key >> val;

    if(debug())
      cout << " key = " << key << ", val = " << val << endl;

    //is this a string option?
    {
      map<string,string*>::iterator it = stringOpts.find(key);
      if( stringOpts.end() != it )
      {
        it->second->assign(val);
        if(debug())
          cout << " ... assign string key" << endl;
        continue;
      }
    }

    //is this an int option?
    {
      map<string,int*>::iterator it = intOpts.find(key);
      if( intOpts.end() != it )
      {
        *(it->second) = atoi(val.c_str());
        if(debug())
          cout << " ... assign int key" << endl;
        continue;
      }
    }


    //is this a bool option?
    {
      map<string,bool*>::iterator it = boolOpts.find(key);
      if( boolOpts.end() != it )
      {
        *(it->second) = atoi(val.c_str()); //todo:support true/false
        if(debug())
          cout << " ... assign bool key" << endl;
        continue;
      }
    }

    if(debug())
      cout << " ... key not found.  handle error?" << endl;

  }

  return true;
}