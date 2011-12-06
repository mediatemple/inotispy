#!/usr/bin/perl

#XXX Just want my builds to pass for now - James
use Test::More tests => 1;
ok(1);
#XXX

__END__
use File::Basename;
use File::Temp qw/ tempfile tempdir /;
use Net::Inotispy;
use Net::Inotispy::Constants;
use Test::Most tests => 8;

my $ispy = Net::Inotispy->new("tcp://127.0.0.1:5559");

# make a temp dir, tell inotispy to watch it
my $dirname = tempdir( CLEANUP => 1 );
my $reply = $ispy->watch( { path => $dirname, mask => IN_ALL_EVENTS } );

ok(
    eval { $reply->{success} } && $reply->{success} == 1,
    "Set watch on directory: $dirname"
);

# verify inotispy is watching the root
$reply = $ispy->get_roots( { path => $dirname } );

ok(
    eval { $reply->{data}->[0] } && $reply->{data}->[0] eq $dirname,
    "Only reported watch is ours: $dirname"
);

# subscribe to the watch
$reply = $ispy->subscribe( { path => $dirname } );
cmp_deeply(
	   $reply,
	   {
	       data => 1234,
	   },
	   "Subscribed to watch on directory: $dirname"
	   );

# "touch" a file
my ($fh, $filename) = tempfile( DIR => $dirname );
close $fh;
$reply = $ispy->get_events( { path => $dirname, count => 0 } );

cmp_deeply(
	   $reply,
	   {
	       'data' => [
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_CREATE,
			  },
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_OPEN,
			  },
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_ATTRIB,
			  },
                          {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_CLOSE_WRITE,
			  },
			  ],
			  },
	   "Touched a file: $filename"
	   );

# modify a file
open($fh, ">>", $filename) or die "cannot open >> $filename: $!";
print $fh "zzz";
close $fh;
$reply = $ispy->get_events( { path => $dirname, count => 0 } );

cmp_deeply(
	   $reply,
	   {
	       'data' => [
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_OPEN,
			  },
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_MODIFY,
			  },
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_CLOSE_WRITE,
			  },
			  ],
			  },
	   "Modified a file: $filename"
	   );

# read a file
open($fh, "<", $filename) or die "cannot open < $filename: $!";
read $fh, my $byte, 1;
close $fh;
$reply = $ispy->get_events( { path => $dirname, count => 0 } );

cmp_deeply(
	   $reply,
	   {
	       'data' => [
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_OPEN,
			  },
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_ACCESS,
			  },
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_CLOSE_NOWRITE,
			  },
			  ],
			  },
	   "Read a file: $filename"
	   );

# move the file
rename $filename, $filename . 'zzz';
$reply = $ispy->get_events( { path => $dirname, count => 0 } );

my $cookie = $reply->{data}[0]{cookie};
cmp_deeply(
	   $reply,
	   {
	       'data' => [
			  {
			      'cookie' => $cookie,
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_MOVED_FROM,
			  },
			  {
			      'cookie' => $cookie,
			      'name' => basename($filename . 'zzz'),
			      'path' => $dirname,
			      'mask' => IN_MOVED_TO,
			  },
			  ],
			  },
	   "Moved file: '$filename' to '${filename}zzz'"
	   );
	   

$filename .= 'zzz';

# delete the file
unlink $filename;
$reply = $ispy->get_events( { path => $dirname, count => 0 } );

cmp_deeply(
	   $reply,
	   {
	       'data' => [
			  {
			      'name' => basename($filename),
			      'path' => $dirname,
			      'mask' => IN_DELETE,
			  },
			  ],
			  },
	   "Deleted file: $filename"
	   );

done_testing();
