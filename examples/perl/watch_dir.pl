#!/usr/bin/perl 

$|++;
use strict;
use warnings;

use JSON;
use Data::Dumper;
use ZeroMQ qw/:all/;

my $path = shift ||
    die "Usage: $! <PATH>\n";

my $cxt  = ZeroMQ::Context->new;
my $sock = $cxt->socket(ZMQ_REQ);
$sock->connect('tcp://127.0.0.1:5555');

$sock->send_as( json => {
    call => 'watch',
    path => $path,
});

# Alternatively, if you want to set the different inotify alerts
# (create, delete, modify, etc) for each root path then do the
# following:
#
#    use Linux::Inotify2;
#    
#    $sock->send_as( json => {
#        call => 'watch',
#        path => $path,
#        mask => IN_CREATE | IN_MODIFY,
#    });

my $reply = $sock->recv_as('json');
print "Got reply: ".Dumper($reply); 

__END__
