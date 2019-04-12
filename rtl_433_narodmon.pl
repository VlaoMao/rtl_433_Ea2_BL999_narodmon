#!/usr/bin/perl

# change params
# rtl_433 -R 31 -F json:/tmp/data &
# then run this script

binmode STDOUT, ":utf8";
use utf8;
use JSON;
use IO::Socket::INET;
use File::ReadBackwards;

my $LAT = 53.201280;
my $LNG = 44.994961;
my $ELE = 0;
my $DEVICE_MAC='xxxxxxxxxxxx';
my $DEVICE_NAME='rtl_433';
my $SENSOR_DESC_1='Температура на улице';
my $SENSOR_DESC_2='Влажность';



my $socket = new IO::Socket::INET (
    PeerHost => 'narodmon.ru',
    PeerPort => '8283',
    Proto => 'tcp',
);

binmode $socket, ":utf8";

die "cannot connect to the server $!\n" unless $socket;
print "connected to the server\n";

my $bw = File::ReadBackwards->new('/tmp/data') or
    die "can't read file";

my $last_line = $bw->readline;
if( not defined($last_line)) {
    print "Line is empty\n";
    exit 0;
}

my $data = decode_json($last_line);
my $temp = $data->{'temperature_C'};

if( not defined($temp)) {
    $last_line = $bw->readline;
    $data = decode_json($last_line);
    $temp = $data->{'temperature_C'};
    if( not defined($temp)) {
	print "Temp is empty\n";
	exit 0;
    }
}

truncate "/tmp/data", 0;

my $str = "#$DEVICE_MAC#$DEVICE_NAME" . "\n";

my $humidity = $data->{'humidity'};

if( not defined($humidity)) {
    print "Humidity is empty\n";
    exit 0;
}
$str = $str . "#T1#$temp#$SENSOR_DESC_1" . "\n" . 
		"#H1#$humidity#$SENSOR_DESC_2" . "\n";

$str = $str . "#LAT#$LAT" . "\n" .
	"#LNG#$LNG" . "\n" . "##" . "\n";

print $str;

my $raw = pack "C0a*", $str;

my $size = $socket->send($raw);
print "sent data of length $size\n";
my $response = "";
$socket->recv($response, 1024);
print "received response: $response\n";

shutdown($socket, 1);

$socket->close();
