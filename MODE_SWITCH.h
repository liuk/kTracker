#ifndef _MODE_SWITCH_H
#define _MODE_SWITCH_H

//-------------- kTracker ROOT -------------------
#define KTRACKER_ROOT "/Users/liuk/currentWork/kTracker_dev"

//--------------- Mode controls ------------------
//=== Enable this when running over MC events
//#define MC_MODE

//=== Enable evaluation outputs
//#define EVAL_MODE

//=== Enable alignment mode
//#define ALIGNMENT_MODE

//=== Enable multiple minimizer feature, disabled by default
//#define _ENABLE_MULTI_MINI

//=== Coarse mode, no driftTime info is used, disabled by default
//#define COARSE_MODE

//=== Enable hodo masking with trigger road info
//#define _ENABLE_TRIGGER_MASK

//=== Enable Kalman fitting in fast tracking and alignment, enabled by default
#define _ENABLE_KF

//=== Attach the raw data to the reconstructed events
#define ATTACH_RAW

//=== Enable triming of hodoscope hits by trigger requirements
#define TRIGGER_TRIMING

//=== Enable reading the alignment data from online schema instead of external ascii file
//#define LOAD_ONLINE_ALIGNMENT

//--------------- Geometry version ---------------
#define GEOMETRY_VERSION "geometry_G4_run2"

//-------------- SQL database --------------------
#define MYSQL_SERVER_ADDR "localhost"
//#define MYSQL_SERVER_ADDR "seaquel.physics.illinois.edu"
//#define MYSQL_SERVER_ADDR "e906-db1.fnal.gov"

#define MYSQL_SERVER_PORT 3306
//#define MYSQL_SERVER_PORT 3283

//==== Enable/disable dimuon mode
#define DIMUON_MODE 0

//==== Turn on/off KMag
#define KMAG_ON 1

//==== Enable massive debugging output, disabled by default
//#define _DEBUG_ON
//#define _DEBUG_ON_LEVEL_2

//---------------------FOLLOWING PART SHOULD NOT BE CHANGED FOR NO GOOD REASON -----------------------
//--------------- Fast tracking configuration ----
#define TX_MAX 0.15
#define TY_MAX 0.1
#define X0_MAX 150.
#define Y0_MAX 50.
#define INVP_MIN 0.01
#define INVP_MAX 0.2
#define PROB_LOOSE 0.0
#define PROB_TIGHT 0.001
#define HIT_REJECT 3.

//--------------- Muon identification -----------
#define MUID_REJECT 4.
#define MUID_P0 0.11825 
#define MUID_Z_REF 2028.19
#define MUID_R_CUT 1.5

//--------------- Geometry setup -----------------
#define nChamberPlanes 24
#define nHodoPlanes 16
#define nPropPlanes 8

#define Z_KMAG_BEND 1064.26
#define Z_FMAG_BEND 251.4
#define Z_KFMAG_BEND 375.
#define PT_KICK_FMAG 2.909
#define PT_KICK_KMAG 0.4016
#define ELOSS_KFMAG 8.12
#define ELOSS_ABSORBER 1.81
#define Z_ST2 1347.36
#define Z_ABSORBER 2028.19
#define Z_REF 0.
#define Z_TARGET -129.54 
#define Z_DUMP 40.
#define RESOLUTION_DC 0.07

#define BEAM_SPOT_X 0.5
#define BEAM_SPOT_Y 0.5

//-------------- Coarse swim setup --------------
#define FMAG_HOLE_LENGTH 27.94
#define FMAG_HOLE_RADIUS 1.27
#define FMAG_LENGTH 502.92
#define NSLICES_FMAG 100
#define NSTEPS_TARGET 200
#define ELOSS_FMAG_P0 7.18274
#define ELOSS_FMAG_P1 0.0361447
#define ELOSS_FMAG_P2 -0.000718127
#define ELOSS_FMAG_P3 7.97312e-06 
#define ELOSS_FMAG_P4 -3.05481e-08
#define Z_UPSTREAM -500.
#define Z_DOWNSTREAM 500.

//-------------- Trigger analyzer modes ---------
#define USE_TRIGGER_HIT 1
#define USE_HIT 2

//-------------- Useful marcros -----------------
#define LogInfo(message) std::cout << "DEBUG: " << __FILE__ << "  " << __LINE__ << "  " << __FUNCTION__ << " :::  " << message << std::endl
#define varName(x) #x

#endif
