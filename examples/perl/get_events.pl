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

$sock->send_as(json => { 
    call => 'get_events',
    path => $path, 

    # A count of 0 (zero) means to grab *all* events
    # currently in the queue for this root path.
    count => 0,
 });

my $reply = $sock->recv_as('json');
print "Got reply: ".Dumper($reply); 

__END__
