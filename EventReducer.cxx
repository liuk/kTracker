#include <iostream>
#include "EventReducer.h"

EventReducer::EventReducer(TString options) : afterhit(false), hodomask(false), outoftime(false), decluster(false), mergehodo(false), triggermask(false), sagitta(false), hough(false), externalpar(false), realization(false), difnim(false)
{
    //parse the reducer setup
    options.ToLower();
    if(options.Contains("a")) afterhit = true;
    if(options.Contains("h")) hodomask = true;
    if(options.Contains("o")) outoftime = true;
    if(options.Contains("c")) decluster = true;
    if(options.Contains("m")) mergehodo = true;
    if(options.Contains("t")) triggermask = true;
    if(options.Contains("s")) sagitta = true;
    if(options.Contains("g")) hough = true;
    if(options.Contains("e")) externalpar = true;
    if(options.Contains("r")) realization = true;
    if(options.Contains("n")) difnim = true;

    //Screen output for all the methods enabled
    if(afterhit)      std::cout << "EventReducer: after-pulse removal enabled. " << std::endl;
    if(hodomask)      std::cout << "EventReducer: hodoscope masking enabled. " << std::endl;
    if(outoftime)     std::cout << "EventReducer: out-of-time hits removal enabled. " << std::endl;
    if(decluster)     std::cout << "EventReducer: hit cluster removal enabled. " << std::endl;
    if(mergehodo)     std::cout << "EventReducer: v1495 hits will be merged with TW-TDC hits. " << std::endl;
    if(triggermask)   std::cout << "EventReducer: trigger road masking enabled. " << std::endl;
    if(sagitta)       std::cout << "EventReducer: sagitta reducer enabled. " << std::endl;
    if(hough)         std::cout << "EventReducer: hough transform reducer enabled. " << std::endl;
    if(externalpar)   std::cout << "EventReducer: will reset the alignment/calibration parameters. " << std::endl;
    if(realization)   std::cout << "EventReducer: realization enabled. " << std::endl;
    if(difnim)        std::cout << "EventReducer: trigger masking will be disabled in NIM events. " << std::endl;

    //initialize services
    p_geomSvc = GeomSvc::instance();
    if(triggermask)
    {
        p_triggerAna = new TriggerAnalyzer();
        p_triggerAna->init();
        p_triggerAna->buildTriggerTree();
    }

    if(hodomask) initHodoMaskLUT();

    //set random seed
    rndm.SetSeed(0);
}

EventReducer::~EventReducer()
{
    if(triggermask)
    {
        delete p_triggerAna;
    }
}

int EventReducer::reduceEvent(SRawEvent* rawEvent)
{
    int nHits_before = rawEvent->getNChamberHitsAll();

    //dump the vector of hits from SRawEvent to a list first
    hitlist.clear();
    hodohitlist.clear();
    for(std::vector<Hit>::iterator iter = rawEvent->fAllHits.begin(); iter != rawEvent->fAllHits.end(); ++iter)
    {
        if(outoftime && (!iter->isInTime())) continue;

        if(iter->detectorID <= 24)    //chamber hits
        {
            if(realization && rndm.Rndm() > 0.94) continue;
            //if(hodomask && (!iter->isHodoMask())) continue;
            //if(triggermask && (!iter->isTriggerMask())) continue;
        }
        else if(iter->detectorID > 24 && iter->detectorID <= 40)
        {
            // if trigger masking is enabled, all the X hodos are discarded
            if(triggermask && p_geomSvc->getPlaneType(iter->detectorID) == 1) continue;
        }

        /*
        //only temporary before the mapping is fixed
        if((iter->detectorID == 17 || iter->detectorID == 18) && iter->elementID >= 97 && iter->elementID <= 104)
        {
            iter->detectorID = iter->detectorID == 17 ? 18 : 17;
            iter->pos = p_geomSvc->getMeasurement(iter->detectorID, iter->elementID);
            //iter->driftDistance = p_geomSvc->getDriftDistance(iter->detectorID, iter->tdcTime);
            //iter->setInTime(p_geomSvc->isInTime(iter->detectorID, iter->tdcTime));
        }
        */

        if(externalpar)
        {
            iter->pos = p_geomSvc->getMeasurement(iter->detectorID, iter->elementID);
            iter->driftDistance = p_geomSvc->getDriftDistance(iter->detectorID, iter->tdcTime);
            //iter->setInTime(p_geomSvc->isInTime(iter->detectorID, iter->tdcTime));
        }

        if(realization && iter->detectorID <= 24) iter->driftDistance += rndm.Gaus(0., 0.04);

        if(iter->detectorID >= 25 && iter->detectorID <= 40)
        {
            hodohitlist.push_back(*iter);
        }
        else
        {
            hitlist.push_back(*iter);
        }
    }

    if(mergehodo)
    {
        for(std::vector<Hit>::iterator iter = rawEvent->fTriggerHits.begin(); iter != rawEvent->fTriggerHits.end(); ++iter)
        {
            if(triggermask && p_geomSvc->getPlaneType(iter->detectorID) == 1) continue;
            hodohitlist.push_back(*iter);
        }
    }

    // manully create the X-hodo hits by the trigger roads
    if(triggermask) p_triggerAna->trimEvent(rawEvent, hodohitlist, mergehodo ? (USE_HIT | USE_TRIGGER_HIT) : USE_TRIGGER_HIT);

    //apply hodoscope mask
    hodohitlist.sort();
    if(hodomask) hodoscopeMask(hitlist, hodohitlist);

    //Remove after hits
    hitlist.sort();
    hitlist.merge(hodohitlist);
    if(afterhit) hitlist.unique();

    //Remove hit clusters
    if(decluster) deClusterize();

    //Remove the hits by sagitta ratio
    if(sagitta) sagittaReducer();

    //Push everything back to SRawEvent
    rawEvent->fAllHits.clear();
    rawEvent->fAllHits.assign(hitlist.begin(), hitlist.end());

    rawEvent->reIndex();
    return nHits_before - rawEvent->getNChamberHitsAll();
}

void EventReducer::sagittaReducer()
{
    //find index for D1, D2, and D3
    int nHits_D1 = 0;
    int nHits_D2 = 0;
    int nHits_D3 = 0;
    for(std::list<Hit>::iterator iter = hitlist.begin(); iter != hitlist.end(); ++iter)
    {
        if(iter->detectorID > 24) break;
        if(iter->detectorID <= 6)
        {
            ++nHits_D1;
        }
        else if(iter->detectorID <= 12)
        {
            ++nHits_D2;
        }
        else
        {
            ++nHits_D3;
        }
    }
    int idx_D1 = nHits_D1;
    int idx_D2 = nHits_D1 + nHits_D2;
    int idx_D3 = nHits_D1 + nHits_D2 + nHits_D3;

    //Loop over all hits
    std::vector<Hit> hitTemp;
    hitTemp.assign(hitlist.begin(), hitlist.end());

    std::vector<int> flag(hitTemp.size(), -1);
    for(int i = idx_D2; i < idx_D3; ++i)
    {
        double z3 = p_geomSvc->getPlanePosition(hitTemp[i].detectorID);
        double slope_target = hitTemp[i].pos/(z3 - Z_TARGET);
        double slope_dump = hitTemp[i].pos/(z3 - Z_DUMP);
        for(int j = idx_D1; j < idx_D2; ++j)
        {
            if(p_geomSvc->getPlaneType(hitTemp[i].detectorID) != p_geomSvc->getPlaneType(hitTemp[j].detectorID)) continue;

            double z2 = p_geomSvc->getPlanePosition(hitTemp[j].detectorID);
            if(fabs((hitTemp[i].pos - hitTemp[j].pos)/(z2 - z3)) > TX_MAX) continue;
            double s2_target = hitTemp[j].pos - slope_target*(z2 - Z_TARGET);
            double s2_dump = hitTemp[j].pos - slope_dump*(z2 - Z_DUMP);

            for(int k = 0; k < idx_D1; ++k)
            {
                if(p_geomSvc->getPlaneType(hitTemp[i].detectorID) != p_geomSvc->getPlaneType(hitTemp[k].detectorID)) continue;
                if(flag[i] > 0 && flag[j] > 0 && flag[k] > 0) continue;

                double z1 = p_geomSvc->getPlanePosition(hitTemp[k].detectorID);
                double pos_exp_target = SAGITTA_TARGET_CENTER*s2_target + slope_target*(z1 - Z_TARGET);
                double pos_exp_dump = SAGITTA_DUMP_CENTER*s2_dump + slope_dump*(z1 - Z_DUMP);
                double win_target = fabs(s2_target*SAGITTA_TARGET_WIN);
                double win_dump = fabs(s2_dump*SAGITTA_DUMP_WIN);

                double p_min = std::min(pos_exp_target - win_target, pos_exp_dump - win_dump);
                double p_max = std::max(pos_exp_target + win_target, pos_exp_dump + win_dump);

                if(hitTemp[k].pos > p_min && hitTemp[k].pos < p_max)
                {
                    flag[i] = 1;
                    flag[j] = 1;
                    flag[k] = 1;
                }
            }
        }
    }

    int idx = 0;
    for(std::list<Hit>::iterator iter = hitlist.begin(); iter != hitlist.end(); )
    {
        if(flag[idx] < 0)
        {
            iter = hitlist.erase(iter);
        }
        else
        {
            ++iter;
        }

        ++idx;
        if(idx >= idx_D3) break;
    }
}

void EventReducer::deClusterize()
{
    std::vector<std::list<Hit>::iterator> cluster;
    cluster.clear();
    for(std::list<Hit>::iterator hit = hitlist.begin(); hit != hitlist.end(); ++hit)
    {
        //if we already reached the hodo part, stop
        if(hit->detectorID > 24) break;

        if(cluster.size() == 0)
        {
            cluster.push_back(hit);
        }
        else
        {
            if(hit->detectorID != cluster.back()->detectorID)
            {
                processCluster(cluster);
                cluster.push_back(hit);
            }
            else if(hit->elementID - cluster.back()->elementID > 1)
            {
                processCluster(cluster);
                cluster.push_back(hit);
            }
            else
            {
                cluster.push_back(hit);
            }
        }
    }
}

void EventReducer::processCluster(std::vector<std::list<Hit>::iterator>& cluster)
{
    unsigned int clusterSize = cluster.size();

    //size-2 clusters, retain the hit with smaller driftDistance
    if(clusterSize == 2)
    {
        double w_max = 0.9*0.5*(cluster.back()->pos - cluster.front()->pos);
        double w_min = w_max/9.*4.; //double w_min = 0.6*0.5*(cluster.back()->pos - cluster.front()->pos);

        if((cluster.front()->driftDistance > w_max && cluster.back()->driftDistance > w_min) || (cluster.front()->driftDistance > w_min && cluster.back()->driftDistance > w_max))
        {
            cluster.front()->driftDistance > cluster.back()->driftDistance ? hitlist.erase(cluster.front()) : hitlist.erase(cluster.back());
        }
        else if(fabs(cluster.front()->tdcTime - cluster.back()->tdcTime) < 8. && cluster.front()->detectorID >= 13 && cluster.front()->detectorID <= 18)
        {
            hitlist.erase(cluster.front());
            hitlist.erase(cluster.back());
        }
    }

    //size-larger-than-3, discard entirely
    if(clusterSize >= 3)
    {
        double dt_mean = 0.;
        for(unsigned int i = 1; i < clusterSize; ++i)
        {
            dt_mean += fabs(cluster[i]->tdcTime - cluster[i-1]->tdcTime);
        }
        dt_mean = dt_mean/(clusterSize - 1);

        if(dt_mean < 10.)
        {
            //electric noise, discard them all
            for(unsigned int i = 0; i < clusterSize; ++i)
            {
                hitlist.erase(cluster[i]);
            }
        }
        else
        {
            /*
            double dt_rms = 0.;
             	  for(unsigned int i = 1; i < clusterSize; ++i)
              {
                 double dt = fabs(cluster[i]->tdcTime - cluster[i-1]->tdcTime);
                 dt_rms += ((dt - dt_mean)*(dt - dt_mean));
              }
            dt_rms = sqrt(dt_rms/(clusterSize - 1));

            //delta ray, keep the first and last
            if(dt_rms < 5.)*/
            {
                for(unsigned int i = 1; i < clusterSize - 1; ++i)
                {
                    hitlist.erase(cluster[i]);
                }
            }
        }
    }

    cluster.clear();
}

void EventReducer::initHodoMaskLUT()
{
    h2celementID_lo.clear();
    h2celementID_hi.clear();

    int hodoIDs[8] = {25, 26, 31, 32, 33, 34, 39, 40};
    int chamIDs[8][12] = { {1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0},
                           {1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0},
                           {7, 8, 9, 10, 11, 12, 0, 0, 0, 0, 0, 0},
                           {7, 8, 9, 10, 11, 12, 0, 0, 0, 0, 0, 0},
                           {13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
                           {13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

    for(int i = 0; i < 8; ++i)
    {
        int nPaddles = p_geomSvc->getPlaneNElements(hodoIDs[i]);
        for(int j = 1; j <= nPaddles; ++j)
        {
            //for each paddle, there is a group of 6/12 chamber planes to work with
            int uniqueID = hodoIDs[i]*1000 + j;
            for(int k = 0; k < 12; ++k)
            {
                if(chamIDs[i][k] < 1) continue;
                h2celementID_lo[uniqueID].push_back(0);
                h2celementID_hi[uniqueID].push_back(999);
            }
        }
    }

    //reverse the LUT
    c2helementIDs.clear();
    for(LUT::iterator iter = h2celementID_lo.begin(); iter != h2celementID_lo.end(); ++iter)
    {
        int hodoUID = iter->first;
        for(unsigned int i = 0; i < h2celementID_lo[hodoUID].size(); ++i)
        {
            int chamUID_lo = iter->second[i];
            int chamUID_hi = h2celementID_hi[hodoUID][i];
            for(int j = chamUID_lo; j <= chamUID_hi; ++j)
            {
                c2helementIDs[j].push_back(hodoUID);
            }
        }
    }
}

void EventReducer::hodoscopeMask(std::list<Hit>& chamberhits, std::list<Hit>& hodohits)
{
    for(std::list<Hit>::iterator iter = chamberhits.begin(); iter != chamberhits.end(); )
    {
        if(iter->detectorID > 40)
        {
            ++iter;
            continue;
        }

        int uniqueID = iter->uniqueID();

        bool masked = false;
        for(std::vector<int>::iterator jter = c2helementIDs[uniqueID].begin(); jter != c2helementIDs[uniqueID].end(); ++jter)
        {
            if(std::find(hodohits.begin(), hodohits.end(), Hit(*jter)) != hodohits.end())
            {
                masked = true;
                break;
            }
        }

        if(masked)
        {
            ++iter;
        }
        else
        {
            iter = chamberhits.erase(iter);
        }
    }
}
