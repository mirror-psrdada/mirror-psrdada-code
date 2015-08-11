#!/usr/bin/env perl

# 
# MOPSR Superb AQ -> DBEVENT -> DUMP script
#
# Author:   Andrew Jameson
# 

use lib $ENV{"DADA_ROOT"}."/bin";

#
# Include Modules
#
use Mopsr;          # DADA Module for configuration options
use strict;          # strict mode (like -Wall)
use File::Basename; 
use threads;         # standard perl threads
use threads::shared; # standard perl threads
use IO::Socket;      # Standard perl socket library
use IO::Select;      # Allows select polling on a socket
use Net::hostent;

sub usage() 
{
  print "Usage: ".basename($0)." PWC_ID\n";
  print "   PWC_ID   The Primary Write Client ID this script will process\n";
}

#
# Global Variable Declarations
#
our $dl : shared;
our $quit_daemon : shared;
our $daemon_name : shared;
our $pwc_id : shared;
our $db_key : shared;
our %cfg : shared;
our $log_host;
our $sys_log_port;
our $src_log_port;
our $sys_log_sock;
our $src_log_sock;
our $sys_log_file;
our $src_log_file;
our $client_logger;

#
# Initialize globals
#
$dl = 1;
$quit_daemon = 0;
$daemon_name = Dada::daemonBaseName($0);
$pwc_id = 0;
$db_key = "";
%cfg = Mopsr::getConfig();
$log_host = $cfg{"SERVER_HOST"};
$sys_log_port = $cfg{"SERVER_SYS_LOG_PORT"};
$src_log_port = $cfg{"SERVER_SRC_LOG_PORT"};
$sys_log_sock = 0;
$src_log_sock = 0;
$sys_log_file = "";
$src_log_file = "";

#
# Local Variable Declarations
#
my $sys_log_file = "";
my $pid_file = "";
my $control_thread = 0;
my $prev_header = "";
my $quit = 0;

#
# Check command line arguments is 1
#
if ($#ARGV != 0) 
{
  usage();
  exit(1);
}
$pwc_id  = $ARGV[0];

# ensure that our pwc_id is valid 
if (($pwc_id >= 0) &&  ($pwc_id < $cfg{"NUM_PWC"}))
{
  # and matches configured hostname
  if ($cfg{"PWC_".$pwc_id} eq Dada::getHostMachineName())
  {
    # determine the relevant PWC based configuration for this script 
    $db_key = Dada::getDBKey($cfg{"DATA_BLOCK_PREFIX"}, $pwc_id, $cfg{"NUM_PWC"}, $cfg{"RECEIVING_DATA_BLOCK"});
  }
  else
  {
    print STDERR "PWC_ID did not match configured hostname\n";
    usage();
    exit(1);
  }
}
else
{
  print STDERR "PWC_ID was not a valid integer between 0 and ".($cfg{"NUM_PWC"}-1)."\n";
  usage();
  exit(1);
}


#
# Sanity check to prevent multiple copies of this daemon running
#
Dada::preventDuplicateDaemon(basename($0)." ".$pwc_id);


#
# Main
#
{
  my ($cmd, $result, $response, $full_cmd);

  $sys_log_file = $cfg{"CLIENT_LOG_DIR"}."/".$daemon_name."_".$pwc_id.".log";
  $src_log_file = $cfg{"CLIENT_LOG_DIR"}."/".$daemon_name."_".$pwc_id.".src.log";
  $pid_file = $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name."_".$pwc_id.".pid";

  # register Signal handlers
  $SIG{INT} = \&sigHandle;
  $SIG{TERM} = \&sigHandle;
  $SIG{PIPE} = \&sigPipeHandle;

  # become a daemon
  Dada::daemonize($sys_log_file, $pid_file);

  # Auto flush output
  $| = 1;

  # Open a connection to the server_sys_monitor.pl script
  $sys_log_sock = Dada::nexusLogOpen($log_host, $sys_log_port);
  if (!$sys_log_sock) {
    print STDERR "Could open log port: ".$log_host.":".$sys_log_port."\n";
  }

  $src_log_sock = Dada::nexusLogOpen($log_host, $src_log_port);
  if (!$src_log_sock) {
    print STDERR "Could open src log port: ".$log_host.":".$src_log_port."\n";
  }

  logMsg(1,"INFO", "STARTING SCRIPT");

  logMsg(2, "INFO", "main: log_file=".$sys_log_file);
  logMsg(2, "INFO", "main: pid_file=".$pid_file);

  # This thread will monitor for our daemon quit file
  $control_thread = threads->new(\&controlThread, $pid_file);

  logMsg(2, "INFO", "main: receiving datablock key global=".$db_key);

	my $recv_db_key = Dada::getDBKey($cfg{"DATA_BLOCK_PREFIX"}, $pwc_id, $cfg{"NUM_PWC"}, $cfg{"RECEIVING_DATA_BLOCK"});
	my $dump_db_key = Dada::getDBKey($cfg{"DATA_BLOCK_PREFIX"}, $pwc_id, $cfg{"NUM_PWC"}, $cfg{"DUMP_DATA_BLOCK"});

  # port on which to listen for event dumping requests
  my $event_port = (int($cfg{"CLIENT_EVENT_BASEPORT"}) + int($pwc_id));

	my $curr_raw_header = "";
	my $prev_raw_header = "";
	my %header = ();
  my $nant = 0;

  # Main Loop
  while (!$quit_daemon) 
  {
		%header = ();

		# next header to read from the receiving data_block
    $cmd =  "dada_header -k ".$recv_db_key;
    logMsg(2, "INFO", "main: ".$cmd);
    $curr_raw_header = `$cmd 2>&1`;
    logMsg(2, "INFO", "main: ".$cmd." returned");

		if ($? != 0)
		{
      if ($quit_daemon)
      {
			  logMsg(0, "INFO", "dada_header failed, but quit_daemon true");
      }
      else
      {
			  logMsg(0, "ERROR", "dada_header failed: ".$curr_raw_header);
        $quit_daemon = 1;
      }
		}
    else
    {
      if ($curr_raw_header eq $prev_raw_header)
      {
        logMsg(0, "ERROR", "main: header repeated, jettesioning observation");
        $cmd = "dada_dbnull -k ".$recv_db_key." -s -z";
      }
	  	else
		  { 
        $cmd = "dada_dbevent ".$recv_db_key." ".$dump_db_key.
               " -p ".$event_port." -t 96 -b 90";
      }

      logMsg(1, "INFO", "START ".$cmd);
      ($result, $response) = Dada::mySystemPiped ($cmd, $src_log_file, 
                                                  $src_log_sock, "src", 
                                                  sprintf("%02d",$pwc_id), $daemon_name, 
                                                  "evnt");
      if ($result ne "ok")
      {
        logMsg(1, "WARN", "cmd failed: ".$response);
      }
      logMsg(1, "INFO", "END   ".$cmd);
		}

		$prev_raw_header = $curr_raw_header;	

    if ($quit) {
      $quit_daemon = 1;
    }
  }

  logMsg(2, "INFO", "main: joining controlThread");
  $control_thread->join();

  logMsg(0, "INFO", "STOPPING SCRIPT");
  Dada::nexusLogClose($sys_log_sock);
  Dada::nexusLogClose($src_log_sock);

  exit(0);
}

sub controlThread($) 
{
  my ($pid_file) = @_;

  logMsg(2, "INFO", "controlThread : starting");

  my $host_quit_file = $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name.".quit";
  my $pwc_quit_file  =  $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name."_".$pwc_id.".quit";

  my $cmd = "";
  my $result = "";
  my $response = "";

  while ((!$quit_daemon) && (!(-f $host_quit_file)) && (!(-f $pwc_quit_file))) {
    sleep(1);
  }

  $quit_daemon = 1;

  $cmd = "^dada_header -k ".$db_key;
  Dada::logMsg(2, $dl ,"controlThread: killProcess(".$cmd.", mpsr)");
  ($result, $response) = Dada::killProcess($cmd, "mpsr");
  Dada::logMsg(2, $dl ,"controlThread: killProcess() ".$result." ".$response);

  $cmd = "^dada_dbevent ".$db_key;
  Dada::logMsg(2, $dl ,"controlThread: killProcess(".$cmd.", mpsr)");
  ($result, $response) = Dada::killProcess($cmd, "mpsr");
  Dada::logMsg(2, $dl ,"controlThread: killProcess() ".$result." ".$response);

  if ( -f $pid_file) {
    logMsg(2, "INFO", "controlThread: unlinking PID file");
    unlink($pid_file);
  } else {
    logMsg(1, "INFO", "controlThread: PID file did not exist on script exit");
  }

  logMsg(2, "INFO", "controlThread: exiting");

}


#
# Logs a message to the nexus logger and print to STDOUT with timestamp
#
sub logMsg($$$) {

  my ($level, $type, $msg) = @_;

  if ($level <= $dl) {

    # remove backticks in error message
    $msg =~ s/`/'/;

    my $time = Dada::getCurrentDadaTime();
    if (!($sys_log_sock)) {
      $sys_log_sock = Dada::nexusLogOpen($log_host, $sys_log_port);
    }
    if ($sys_log_sock) {
      Dada::nexusLogMessage($sys_log_sock, $pwc_id, $time, "sys", $type, "superb", $msg);
    }
    print "[".$time."] ".$msg."\n";
  }
}


sub sigHandle($) 
{
  my $sigName = shift;
  print STDERR $daemon_name." : Received SIG".$sigName."\n";

  # if we CTRL+C twice, just hard exit
  if ($quit_daemon) {
    print STDERR $daemon_name." : Recevied 2 signals, Exiting\n";
    exit 1;

  # Tell threads to try and quit
  } else {

    $quit_daemon = 1;
    if ($sys_log_sock) {
      close($sys_log_sock);
    }
  }
}

sub sigPipeHandle($) 
{
  my $sigName = shift;
  print STDERR $daemon_name." : Received SIG".$sigName."\n";
  $sys_log_sock = 0;
  if ($log_host && $sys_log_port) {
    $sys_log_sock = Dada::nexusLogOpen($log_host, $sys_log_port);
  }
}

