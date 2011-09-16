#!/usr/bin/perl 
#
# Copyright (c) 2011-*, James Conerly <james@jamesconerly.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

$|++;
use strict;
use warnings;

use JSON;
use Data::Dumper;
use ZeroMQ qw/:all/;

my $cxt  = ZeroMQ::Context->new;
my $sock = $cxt->socket(ZMQ_REQ);
$sock->connect('tcp://127.0.0.1:5555');

$sock->send_as( json => { call => 'get_roots' });

my $reply = $sock->recv_as('json');
print "Got reply: ".Dumper($reply); 

__END__
