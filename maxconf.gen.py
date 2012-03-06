#!/usr/bin/python

import sys, getopt, uuid, time

try:
    opts, args = getopt.getopt(sys.argv[1:], '', ['install=','model='])
    opts = dict(opts)
except getopt.GetoptError, err:
    print >> sys.stderr, 'Could not parse command line options.'
    sys.exit(1)

if '--install' not in opts:
    print >> sys.stderr, 'Install parameter must be defined.'
    sys.exit(1)

if '--model' not in opts:
    opts['--model'] = 'Robot'


print >> sys.stdout, '''
# Max kernel configuration file
# Created and maintained 2012 by Andrew Klofas <andrew@maxkernel.com>
# parsed by libconfuse

id = "%(id)s"
setpath("%(install)s:%(install)s/modules")
installed = %(time)d
model = "%(model)s"

loadmodule("netui.mo")		# creates a http server to show innards of kernel
loadmodule("console.mo")	# provide kernel console (through unix sockets)
loadmodule("discovery.mo")	# provide auto discovery mechanism
loadmodule("service.mo")	# allow autodetection of services

# uncomment if you want to allow syscalls through the network
#config("console", "enable_network", 1)

# comment out if you want to remain hidden on the network
config("discovery", "enable_discovery", 1)


# now execute a stage 2 script
execdirectory("stitcher")

# EOF
''' % {
'id'        :   str(uuid.uuid1()),
'install'   :   opts['--install'],
'time'      :   time.time(),
'model'     :   opts['--model'],
},

