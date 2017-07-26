#!/usr/bin/env perl

##############################################################################
#  
#     Copyright (C) 2016 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################
#
# client_mopsr_bf_dump.pl 
#
# Writes dumped transient event data from DB to disk
# 
###############################################################################

use lib $ENV{"DADA_ROOT"}."/bin";

use IO::Socket;
use Getopt::Std;
use File::Basename;
use Mopsr;
use strict;
use threads;
use threads::shared;

sub usage() 
{
  print "Usage: ".basename($0)." BF_ID\n";
}

#
# Global Variables
#
our $dl : shared;
our $quit_daemon : shared;
our $daemon_name : shared;
our %cfg : shared;
our $localhost : shared;
our $bf_id : shared;
our $db_key : shared;
our $log_host;
our $sys_log_port;
our $src_log_port;
our $sys_log_sock;
our $src_log_sock;
our $sys_log_file;
our $src_log_file;

#
# Initialize globals
#
$dl = 1;
$quit_daemon = 0;
$daemon_name = Dada::daemonBaseName($0);
%cfg = Mopsr::getConfig("bf");
$bf_id = -1;
$db_key = "dada";
$localhost = Dada::getHostMachineName(); 
$log_host = $cfg{"SERVER_HOST"};
$sys_log_port = $cfg{"SERVER_BF_SYS_LOG_PORT"};
$src_log_port = $cfg{"SERVER_BF_SRC_LOG_PORT"};
$sys_log_sock = 0;
$src_log_sock = 0;
$sys_log_file = "";
$src_log_file = "";


# Check command line argument
if ($#ARGV != 0)
{
  usage();
  exit(1);
}

$bf_id  = $ARGV[0];

# ensure that our bf_id is valid 
if (($bf_id >= 0) &&  ($bf_id < $cfg{"NRECV"}))
{
  # and matches configured hostname
  if ($cfg{"RECV_".$bf_id} ne Dada::getHostMachineName())
  {
    print STDERR "RECV_".$bf_id." did not match configured hostname [".Dada::getHostMachineName()."]\n";
    usage();
    exit(1);
  }
}
else
{
  print STDERR "bf_id was not a valid integer between 0 and ".($cfg{"NRECV"}-1)."\n";
  usage();
  exit(1);
}

#
# Sanity check to prevent multiple copies of this daemon running
#
Dada::preventDuplicateDaemon(basename($0)." ".$bf_id);

###############################################################################
#
# Main
#
{
  # Register signal handlers
  $SIG{INT} = \&sigHandle;
  $SIG{TERM} = \&sigHandle;
  $SIG{PIPE} = \&sigPipeHandle;

  $sys_log_file = $cfg{"CLIENT_LOG_DIR"}."/".$daemon_name."_".$bf_id.".log";
  $src_log_file = $cfg{"CLIENT_LOG_DIR"}."/".$daemon_name."_".$bf_id.".src.log";
  my $pid_file =  $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name."_".$bf_id.".pid";

  # Autoflush STDOUT
  $| = 1;

  # become a daemon
  Dada::daemonize($sys_log_file, $pid_file);

  # Open a connection to the server_sys_monitor.pl script
  $sys_log_sock = Dada::nexusLogOpen($log_host, $sys_log_port);
  if (!$sys_log_sock) {
    print STDERR "Could open sys log port: ".$log_host.":".$sys_log_port."\n";
  }

  $src_log_sock = Dada::nexusLogOpen($log_host, $src_log_port);
  if (!$src_log_sock) {
    print STDERR "Could open src log port: ".$log_host.":".$src_log_port."\n";
  }

  msg (0, "INFO", "STARTING SCRIPT");

  my ($cmd, $result, $response);
  my ($dump_header);

  my $control_thread = threads->new(\&controlThread, $pid_file);

  # this is stream we will be dumping from
  $db_key = Dada::getDBKey($cfg{"DATA_BLOCK_PREFIX"}, $bf_id, $cfg{"NUM_BF"}, $cfg{"DUMP_DATA_BLOCK"});

  my $dump_dir = $cfg{"CLIENT_DUMP_DIR"}."/".sprintf("BF%02d", $bf_id);

  # create the dumping dir if it does not exist
  if (!(-d $dump_dir))
  {
    msg(1, "INFO", "main: creating ".$dump_dir);
    ($result, $response) = Dada::mkdirRecursive ($dump_dir, 0755);
    if ($result ne "ok")
    {
      msg(0, "ERROR", "main: could not create ".$dump_dir.": ".$response);
      $quit_daemon = 1;
    }
  }

  $cmd = "touch ".$dump_dir."/test.touch";
  msg(2, "INFO", "main: ".$cmd);
  ($result, $response) = Dada::mySystem($cmd);
  msg(3, "INFO", "main: ".$result." ".$response);
  if ($result ne "ok")
  {
    msg(0, "ERROR", "main: could not write to ".$dump_dir.": ".$response);
    $quit_daemon = 1;
  }
  else
  {
    msg(2, "INFO", "main: unlink ".$dump_dir."/test.touch");
    unlink $dump_dir."/test.touch";
  }

  # continuously run mopsr_dbib for this PWC
  while (!$quit_daemon)
  {
    $cmd = "dada_header -k ".$db_key;
    msg(2, "INFO", "main: ".$cmd);
    ($result, $dump_header) = Dada::mySystem($cmd);
    msg(3, "INFO", "main: ".$cmd." returned");
    if (($result ne "ok") || ($dump_header eq ""))
    { 
      if ($quit_daemon)
      { 
        msg(2, "INFO", "main: dada_header -k ".$db_key." failed, ".
                          "and quit_daemon true");
      }
      else
      { 
        msg(1, "WARN", "main: dada_header -k ".$db_key." failed, ".
                          "but quit_daemon not true");
        sleep (1);
      }
    }
    else
    {
      msg(3, "INFO", "main: HEADER=[".$dump_header."]");
      $cmd = "dada_dbdisk -k ".$db_key." -D ".$dump_dir." -s";
      
      msg(1, "INFO", "START ".$cmd);
      ($result, $response) = Dada::mySystemPiped ($cmd, $src_log_file, 
                                                  $src_log_sock, "src", 
                                                  sprintf("%02d",$bf_id), $daemon_name, "dump");
      if ($result ne "ok")
      { 
        msg(1, "WARN", "cmd failed: ".$response);
      }
      msg(1, "INFO", "END   ".$cmd);
    }
  }

  # Rejoin our daemon control thread
  msg(2, "INFO", "joining control thread");
  $control_thread->join();

  msg(0, "INFO", "STOPPING SCRIPT");

  # Close the nexus logging connection
  Dada::nexusLogClose($sys_log_sock);
  Dada::nexusLogClose($src_log_sock);

  exit (0);
}

#
# Logs a message to the nexus logger and print to STDOUT with timestamp
#
sub msg($$$)
{
  my ($level, $type, $msg) = @_;

  if ($level <= $dl)
  {
    my $time = Dada::getCurrentDadaTime();
    if (!($sys_log_sock)) {
      $sys_log_sock = Dada::nexusLogOpen($log_host, $sys_log_port);
    }
    if ($sys_log_sock) {
      Dada::nexusLogMessage($sys_log_sock, sprintf("%02d",$bf_id), $time, "sys", $type, "bfevent", $msg);
    }
    print "[".$time."] ".$msg."\n";
  }
}

sub controlThread($)
{
  (my $pid_file) = @_;

  msg(2, "INFO", "controlThread : starting");

  my $host_quit_file = $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name.".quit";
  my $pwc_quit_file  = $cfg{"CLIENT_CONTROL_DIR"}."/".$daemon_name."_".$bf_id.".quit";

  while ((!$quit_daemon) && (!(-f $host_quit_file)) && (!(-f $pwc_quit_file)))
  {
    sleep(1);
  }

  $quit_daemon = 1;

  my ($cmd, $result, $response);

  $cmd = "^dada_header -k ".$db_key;
  msg(2, "INFO" ,"controlThread: killProcess(".$cmd.", mpsr)");
  ($result, $response) = Dada::killProcess($cmd, "mpsr");
  msg(3, "INFO" ,"controlThread: killProcess() ".$result." ".$response);

  $cmd = "^dada_dbnull -k ".$db_key;
  msg(2, "INFO" ,"controlThread: killProcess(".$cmd.", mpsr)");
  ($result, $response) = Dada::killProcess($cmd, "mpsr");
  msg(3, "INFO" ,"controlThread: killProcess() ".$result." ".$response);

  $cmd = "dada_dbdisk -k ".$db_key;
  msg(2, "INFO" ,"controlThread: killProcess(".$cmd.", mpsr)");
  ($result, $response) = Dada::killProcess($cmd, "mpsr");
  msg(3, "INFO" ,"controlThread: killProcess() ".$result." ".$response);

  if ( -f $pid_file) {
    msg(2, "INFO", "controlThread: unlinking PID file");
    unlink($pid_file);
  } else {
    msg(1, "WARN", "controlThread: PID file did not exist on script exit");
  }

  msg(2, "INFO", "controlThread: exiting");

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

