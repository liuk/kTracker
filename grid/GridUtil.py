import re
import os
import subprocess
import time
import threading
from datetime import datetime
from ROOT import TFile
from ROOT import TTree

import warnings
warnings.filterwarnings(action = 'ignore', category = RuntimeWarning)

workDir = os.getenv('HOME') + '/tmp'
version = os.getenv("SEAQUEST_RELEASE")
stopGrid = threading.Event()

inputPrefix = {'track' : 'digit', 'vertex' : 'track'}
auxPrefix = {'track' : 'track_from_digit', 'vertex' : 'vertex_from_track'}

class JobConfig:
    """Container of all possible runKTracker script arguments that are not specific to a run"""

    def __init__(self, configFile = ''):
        self.attr = {}
        if not os.path.exists(configFile):
            self.inited = False
            return

        self.inited = True
        for line in open(configFile).readlines():
            if '#' in line:
                continue

            vals = [val.strip() for val in line.strip().split('=')]
            if len(vals) != 2:
                continue
            self.attr[vals[0]] = vals[1]

    def __getattr__(self, attr):
        return self.attr[attr]

    def __str__(self):
        suffix = ' '
        for key in self.attr:
            suffix = suffix + '--%s=%s ' % (key, self.attr[key])
        return suffix

def getNEvents(name):
	"""Find the number of events in either ROOT file or MySQL schema"""

	if 'root' in name:
		dataFile = TFile(name, 'READ')
		if dataFile.GetListOfKeys().Contains('save'):
		  	return dataFile.Get('save').GetEntries()
		else:
		  	return 0
	else:
		return 0

def getOptimizedSize(name, nEvtMax):
    """Optimize how a run is splitted into serveral jobs"""

    nEvents = getNEvents(name)
    nJobs = nEvents/nEvtMax + 1
    if nJobs > 10:
        nJobs = 10
    nEvtMax_opt = nEvents/nJobs

    sizes = []
    for i in range(nJobs-1):
        sizes.append((nEvtMax_opt*i, nEvtMax_opt))
    sizes.append((nEvtMax_opt*(nJobs-1), nEvtMax_opt+1000))

    return sizes

def getSubDir(runID):
    """Return the sub-director name by runID"""

    runNum = int(runID)
    return ('%06d' % runNum)[0:2] + '/' + ('%06d' % runNum)[2:4]

def getJobAttr(optfile):
    """Parse the name and content of job option file to get the basic information"""

    runID = int(re.findall(r'_(\d{6})_', optfile)[0])

    outtag = ''
    if len(re.findall(r'_(\d+).opts', optfile)) > 0:
        outtag = re.findall(r'_(\d+).opts', optfile)[0]

    for line in open(optfile, 'r').readlines():
		if '#' in line:
			continue

		opt = line.strip().split()
		if len(opt) != 2:
			continue
		elif opt[0] == 'N_Events':
			nEvents = int(opt[1])
		elif opt[0] == 'FirstEvent':
			firstEvent = int(opt[1])

    return (runID, outtag, firstEvent, nEvents)

def gridInit():
    """Initialize grid crendial"""
    os.environ['KRB5CCNAME'] = '/e906/app/users/%s/.krbcc/my_cert' % os.getenv('USER')
    if len(os.popen('klist').readlines()) == 0:
        os.system('kinit -A -r7d %s@FNAL.GOV' % os.getenv('USER'))

    startGridGuard()

def gridGuard():
    """Renew the grid credetial every hour"""
    print 'Grid guard is started. '
    nMinutes = 0
    while not stopGrid.isSet():
        time.sleep(60)
        nMinutes = nMinutes + 1
        if nMinutes % 240 == 0:
            print 'Renew the ticket. '
            os.system('kinit -R -c /e906/app/users/%s/.krbcc/my_cert' % os.getenv('USER'))
    print 'Grid guard is terminated after %s minutes. ' % nMinutes

def startGridGuard():
    """Start a new thread used to refresh the gridGuard"""
    guardThread = threading.Thread(name = 'gridGuard', target = gridGuard)
    guardThread.start()

def stopGridGuard():
    """Stop the thread before exiting"""
    stopGrid.set()

def makeCommand(jobType, runID, conf, firstEvent = -1, nEvents = -1, outtag = '', infile = ''):
    """Make a runKTracker.py command according to the configurations"""
    cmd = 'runKTracker.py --grid --%s ' % jobType

    # if input is runID and no infile is specified, it's for data
    if infile == '':
        cmd = cmd + '--run=%d ' % runID
    else:
        cmd = cmd + '--run=%d --input=%s ' % (runID, infile)

    # if start/end eventID is set
    if firstEvent >= 0 and nEvents > 0 and outtag != '':
        cmd = cmd + '--n-events=%d --first-event=%d --outtag=%s' % (nEvents, firstEvent, outtag)

    # add the rest universal ones as defined by config file
    if conf.inited:
        cmd = cmd + str(conf)

    return cmd

def makeCommandFromOpts(jobType, opts, conf):
    """Make a runKTracker command using configuration and opts file"""

    runID, outtag, firstEvent, nEvents = getJobAttr(opts)
    cmd = 'runKTracker.py --grid --%s --run=%d --first-event=%d --n-events=%d --outtag=%s' % (jobType, runID, firstEvent, nEvents, outtag)

    if conf.inited:
        cmd = cmd + str(conf)

    return cmd

def submitOneJob(cmd):
	"""Actually submit the jobs and parse the return info to see if it's successful"""

	output, err = subprocess.Popen(cmd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, shell = True).communicate()
	try:
		jobID = re.findall(r'cluster (\d*)', output)[0]
		print cmd, 'successful, jobID =', jobID
		return True
	except:
		print cmd, 'failed, will try again later'
		return False

def submitAllJobs(cmds, errlog):
    """Submit all jobs, retry the failed ones, and save the jobs in errlog when a jobs failed more than once"""

    while len(cmds) != 0:

        cmds_success = []
        for i, cmd in enumerate(cmds):
            print '%d/%d' % (i+1, len(cmds)),
            if submitOneJob(cmd):
                cmds_success.append(cmd)

        if len(cmds_success) == 0:
            fout = open(os.path.join(workDir, errlog), 'w')
            fout.write(str(datetime.now()) + '\n')
            for cmd in cmds:
                fout.write(cmd + '\n')
            fout.close()
            return False

        for cmd in cmds_success:
            cmds.remove(cmd)

        if len(cmds) == 0:
            return True

        print 'Sleep for 10 seconds ... '
        time.sleep(10)

def getJobStatus(rootdir, jobType, runID):
    """Get the status of all jobs associated with one runID"""

    optfiles = [os.path.join(rootdir, 'opts', version, getSubDir(runID), f) for f in os.listdir(os.path.join(rootdir, 'opts', version, getSubDir(runID))) if ('%06d' % runID) in f and auxPrefix[jobType] in f]

    nFinished = 0
    failedOpts = []
    for optfile in optfiles:
        logfile = os.path.join(rootdir, 'log', version, getSubDir(runID), '%s_%06d_%s' % (auxPrefix[jobType], runID, version))

        outtag = re.findall(r'_(\d+).opts', optfile)
        if len(outtag) == 0:
            logfile = logfile + '.log'
        else:
            logfile = logfile + '_%s.log' % outtag[0]

        if not os.path.exists(logfile):
            continue

        nFinished = nFinished + 1
        if 'successfully' not in open(logfile).readlines()[-3]:
            failedOpts.append(optfile)

    return (len(optfiles), nFinished, failedOpts)

def getTimeStamp():
    return datetime.now().strftime('%y%m%d-%H%M');