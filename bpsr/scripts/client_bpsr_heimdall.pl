#!/usr/bin/env perl

#
# Monitor transient datablock with heimdall
#

#
# Author:   Andrew Jameson
# 


use lib $ENV{"DADA_ROOT"}."/bin";

#
# Include Modules
#
use Bpsr;            # DADA Module for configuration options
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
our $beam : shared;
our %cfg : shared;
our %roach : shared;
our $log_host;
our $log_port;
our $log_sock;

#
# Initialize globals
#
$dl = 1;
$quit_daemon = 0;
$daemon_name = Dada::daemonBaseName($0);
$pwc_id = 0;
$beam = "";
%cfg = Bpsr::getConfig();
%roach = Bpsr::getROACHConfig();
$log_host = $cfg{"SERVER_HOST"};
$log_port = $cfg{"SERVER_SYS_LOG_PORT"};
$log_sock = 0;

#
# Local Variable Declarations
#
my $log_file = "";
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
    $beam = $roach{"BEAM_".$pwc_id};
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
  my ($cmd, $result, $response, $proc_cmd, $proc_dir, $full_cmd);

  $log_file = $cfg{"CLIENT_LOG_DIR"}."/".$daemon_name."_".$pwc_id.".log";
  $pid_file = $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name."_".$pwc_id.".pid";

  # register Signal handlers
  $SIG{INT} = \&sigHandle;
  $SIG{TERM} = \&sigHandle;
  $SIG{PIPE} = \&sigPipeHandle;

  # become a daemon
  Dada::daemonize($log_file, $pid_file);

  # Auto flush output
  $| = 1;

  # Open a connection to the server_sys_monitor.pl script
  $log_sock = Dada::nexusLogOpen($log_host, $log_port);
  if (!$log_sock) {
    print STDERR "Could open log port: ".$log_host.":".$log_port."\n";
  }

  logMsg(1,"INFO", "STARTING SCRIPT");

  # This thread will monitor for our daemon quit file
  $control_thread = threads->new(\&controlThread, $pid_file);

  # for receipt of UDP data
	my $trans_db_key    = Dada::getDBKey($cfg{"DATA_BLOCK_PREFIX"}, $pwc_id, $cfg{"NUM_PWC"}, $cfg{"TRANSIENT_DATA_BLOCK"});

  my $gpu_id = $cfg{"PWC_GPU_ID_".$pwc_id};

	my $curr_raw_header = "";
	my $prev_raw_header = "";
  my %h = ();

  # Main Loop
  while (!$quit_daemon) 
  {
		# next header to read from the transient data_block
    $cmd = "dada_header -k ".$trans_db_key;
    logMsg(2, "INFO", "main: ".$cmd);
    ($result, $curr_raw_header) = Dada::mySystem ($cmd);
    logMsg(3, "INFO", "main: ".$curr_raw_header);

    if ($result ne "ok")
		{
      if ($quit_daemon)
      {
			  logMsg(2, "INFO", "main: dada_header failed, but quit_daemon true");
      }
      else
      {
			  logMsg(1, "WARN", "dada_header failed, and quit_daemon != true");
        $quit_daemon = 1;
      }
		}
    else
    {
      if ($curr_raw_header eq $prev_raw_header)
		  {  
			  logMsg(0, "ERROR", "main: header repeated, jettesioning observation");
        $proc_cmd = "dada_dbnull -k ".$trans_db_key." -s ";
      } 
		  else
		  {
        %h = Dada::headerToHash ($curr_raw_header);

        if ($h{"MODE"} eq "CAL")
        {
          logMsg(0, "INFO", "main: MODE=CAL, ignoring observation");
          $proc_cmd = "dada_dbnull -k ".$trans_db_key." -s";
        }
        elsif (exists($h{"OBS_VAL"}) && (int($h{"OBS_VAL"}) < 30))
        {
          logMsg(0, "INFO", "main: OBS_VAL < 30s, ignoring observation");
          $proc_cmd = "dada_dbnull -k ".$trans_db_key." -s";
        }
        else
        {
          $proc_dir = $cfg{"CLIENT_ARCHIVE_DIR"}."/".$beam."/".$h{"UTC_START"};
          $proc_cmd = "heimdall -k ".$trans_db_key.
                      " -dm 0 3000".
                      " -dm_tol 1.20".
                      " -boxcar_max 4096".
                      " -min_tscrunch_width 8".
                      " -gpu_id ".$gpu_id.
                      " -zap_chans 0 150".
                      " -zap_chans 181 183".
                      " -zap_chans 335 338".
                      " -zap_chans 573 607".
                      " -max_giant_rate 100000".
                      " -beam ".$beam.
                      " -output_dir ".$proc_dir;
                      #"-coincidencer ".$cfg{"SERVER_HOST"}.":".$cfg{"SERVER_COINCIDENCER_PORT"};
        }
      }

      # setup the full processing command
      $full_cmd = $proc_cmd." 2>&1 | ".$cfg{"SCRIPTS_DIR"}."/client_bpsr_src_logger.pl ".$pwc_id." tran";

      logMsg(1, "INFO", "START [tran] ".$proc_cmd);
      ($result, $response) = Dada::mySystem($full_cmd);
      logMsg(1, "INFO", "END   [tran] ".$proc_cmd);
      if ($result ne "ok")
      {
        logMsg(1, "WARN", "main: ".$proc_cmd." failed ".$response);
      }
		}

		$prev_raw_header = $curr_raw_header;	

    if ($quit)
    {
      $quit_daemon = 1;
    }
  }

  logMsg(2, "INFO", "main: joining controlThread");
  $control_thread->join();

  logMsg(0, "INFO", "STOPPING SCRIPT");
  Dada::nexusLogClose($log_sock);

  exit(0);
}

sub controlThread($) 
{
  my ($pid_file) = @_;

  logMsg(2, "INFO", "controlThread : starting");

  my $host_quit_file = $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name.".quit";
  my $pwc_quit_file  =  $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name."_".$pwc_id.".quit";
	my $trans_db_key    = Dada::getDBKey($cfg{"DATA_BLOCK_PREFIX"}, $pwc_id, $cfg{"NUM_PWC"}, $cfg{"TRANSIENT_DATA_BLOCK"});

  my ($cmd, $result, $response, $process, $user);
  my @processes_to_kill = ();

  while ((!$quit_daemon) && (!(-f $host_quit_file)) && (!(-f $pwc_quit_file))) {
    sleep(1);
  }

  $quit_daemon = 1;

  $process = "^dada_header -k ".$trans_db_key;
  $user = "bpsr";

  logMsg(1, "INFO", "controlThread: killProcess(".$process.", ".$user.", ".$$.")");
  ($result, $response) = Dada::killProcess($process, $user, $$);
  logMsg(1, "INFO", "controlThread: killProcess ".$result." ".$response);
  if ($result ne "ok")
  {
    logMsg(1, "WARN", "controlThread: killProcess for ".$process." failed: ".$response);
  }

  sleep (1);

  # now try to kill any other binary processes that might be running
  push @processes_to_kill, "^heimdall -k ".$trans_db_key;
  push @processes_to_kill, "^dada_dbnull -k ".$trans_db_key;

  foreach $process ( @processes_to_kill)
  {
    logMsg(2, "INFO", "controlThread: killProcess(".$process.", ".$user.", ".$$.")");
    ($result, $response) = Dada::killProcess($process, $user, $$);
    logMsg(2, "INFO", "controlThread: killProcess ".$result." ".$response);
    if ($result ne "ok")
    {
      logMsg(1, "WARN", "controlThread: killProcess for ".$process." failed: ".$response);
    }
  }

  logMsg(2, "INFO", "controlThread: checking if PID file [".$pid_file."] exists");
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
    if (!($log_sock)) {
      $log_sock = Dada::nexusLogOpen($log_host, $log_port);
    }
    if ($log_sock) {
      Dada::nexusLogMessage($log_sock, $pwc_id, $time, "sys", $type, "tran", $msg);
    }
    print STDERR "[".$time."] ".$msg."\n";
  }
}



sub sigHandle($) {

  my $sigName = shift;
  print STDERR $daemon_name." : Received SIG".$sigName."\n";

  # if we CTRL+C twice, just hard exit
  if ($quit_daemon) {
    print STDERR $daemon_name." : Recevied 2 signals, Exiting\n";
    exit 1;

  # Tell threads to try and quit
  } else {

    $quit_daemon = 1;
    if ($log_sock) {
      close($log_sock);
    }
  }
}

sub sigPipeHandle($) 
{
  my $sigName = shift;
  print STDERR $daemon_name." : Received SIG".$sigName."\n";
  $log_sock = 0;
  if ($log_host && $log_port) {
    $log_sock = Dada::nexusLogOpen($log_host, $log_port);
  }
}

sub sigChildHandle ()
{
  my $stiff;
  while (($stiff = waitpid(-1, &WNOHANG)) > 0) 
  {
    # do something with $stiff if you want
  }

  # install *after* calling waitpid
  $SIG{CHLD} = \&igPipeHandle;
}
