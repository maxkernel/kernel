#!/usr/bin/perl

use Getopt::Long;
use Digest::MD5 md5_hex;

my $path = ".";

GetOptions( "install=s" => \$install );

print STDOUT "# Max kernel configuration file\n";
print STDOUT "# Created and maintained 2009 by Andrew Klofas <aklofas\@gmail.com>\n";
print STDOUT "# parsed by libconfuse\n\n";
print STDOUT "id = " . md5_hex( time() . rand() ) . "\n";
print STDOUT "path = " . $install . ":" . $install . "/modules\n";
print STDOUT "installed = " . time() . "\n\n";

print STDOUT <<END;

loadmodule("netui.mo")		# creates a http server to show innards of kernel
loadmodule("console.mo")	# provide kernel console (through unix sockets)
loadmodule("discovery.mo")	# provide auto discovery mechanism
#loadmodule("service.mo")	# allow autodetection of services

# uncomment if you want to allow syscalls through the network
#config("console", "enable_network", 1)

# comment out if you want to remain hidden on the network
config("discovery", "enable_discovery", 1)


# now execute a stage 2 script
execlua("stitcher.lua")

# EOF
END

