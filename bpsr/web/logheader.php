<?PHP

include ("bpsr.lib.php");

$inst = new bpsr();

?>
<html>
<?

$inst->print_head("BPSR Log Header Window", 0);

$config = $inst->config;

$server_daemons = $inst->serverLogInfo();
$server_daemon_ids = array_keys($server_daemons);

$client_daemons = $inst->clientLogInfo();
$client_daemon_ids = array_keys($client_daemons);

?>
<body>
  <script type="text/javascript">

    function autoScroll() {
      var scroll
      scroll = document.getElementById("auto_scroll").checked
      parent.logwindow.auto_scroll = scroll
    }

    function changeLogWindow() {

      var i
      var length    // Length of the logs
      var daemon    // The daemon/log
      var machine   // Machine in question
      var scroll
      var filter

      i = document.getElementById("active_machine").selectedIndex
      machine = document.getElementById("active_machine").options[i].value

      scroll = document.getElementById("auto_scroll").checked
      filter = document.getElementById("filter").value

      if (machine == "nexus") {
<?
        for ($i=0; $i<count($server_daemon_ids); $i++) {
          if ($server_daemons[$server_daemon_ids[$i]]["type"] == "servver") {
            echo "        Select_Value_Enable(document.getElementById(\"daemon\"), \"".$server_daemon_ids[$i]."\", false);\n";
          }
        }
?>
      } else {
<?
        for ($i=0; $i<count($server_daemon_ids); $i++) {
          if ($server_daemons[$server_daemon_ids[$i]]["type"] == "servver") {
            echo "        Select_Value_Enable(document.getElementById(\"daemon\"), \"".$server_daemon_ids[$i]."\", true);\n";
          }
        }
?>
      }

      i = document.getElementById("daemon").selectedIndex
      daemon  = document.getElementById("daemon").options[i].value

      i = document.getElementById("loglevel").selectedIndex
      level  = document.getElementById("loglevel").options[i].value

      i = document.getElementById("loglength").selectedIndex
      length = document.getElementById("loglength").options[i].value

      var newurl= "logwindow.php?machine="+machine+"&daemon="+daemon+"&loglength="+length+"&loglevel="+level+"&autoscroll="+scroll+"&filter="+filter;
      parent.logwindow.document.location = newurl
  
    }

    function Select_Value_Enable(SelectObject, Value, State) {
      for (index = 0; index < SelectObject.length; index++) {
        if (SelectObject[index].value == Value) {
          SelectObject[index].disabled = State;
          //alert("Setting " + SelectObject[index].value + ".disabled = "+State);
        }
      }
    }

  </script>

  <table border=0 width="100%" height="100%" cellpadding=3 cellspacing=0>
    <tr valign="center">
      <td align="center" colspan=3>
      <!-- Select Node -->
       Node: <select id="active_machine" class="smalltext" onchange="changeLogWindow()">
               <option value="nexus">nexus</option>
<?
for ($i=0;$i<$config["NUM_PWC"];$i++) {
  echo "              <option value=".$config["PWC_".$i].">".$config["PWC_".$i]."</option>";
}
?>
             </select>&nbsp;&nbsp;&nbsp;&nbsp;

      <!-- Select log type -->
      <span id="daemon_span" style="visibility: visible;">Log: <select id="daemon" class="smalltext" onchange="changeLogWindow()">
        <option value="bpsr_pwc_monitor">PWC</option>
        <option value="bpsr_src_monitor">SRC</option>
<?
        foreach ($client_daemons as $key => $value) {
          if ($value["logfile"] == "nexus.src.log") {
            echo "        <option value=\"".$key."\">&nbsp;&nbsp;&nbsp;&nbsp;".$value["name"]."</option>\n";
          }
        }
?>
        <option value="bpsr_sys_monitor">SYS</option>
<?
        foreach ($client_daemons as $key => $value) {
          if ($value["logfile"] == "nexus.sys.log") {
            echo "        <option value=\"".$key."\">&nbsp;&nbsp;&nbsp;&nbsp;".$value["name"]."</option>\n";
          }
        }

?>
        <option value="srv" disabled=true visible=true>SERVER ONLY</option>
<?
        foreach ($server_daemons as $key => $value) {
          if ($value["tag"] == "server") {
            echo "        <option value=\"".$key."\">&nbsp;&nbsp;&nbsp;&nbsp;".$value["name"]."</option>\n";
          }
        }
?>

      </select></span>&nbsp;&nbsp;&nbsp;&nbsp;

      <!-- Select log level -->
      <span id="log_level_span" style="visibility: visible;">Level: <select id="loglevel" class="smalltext" onchange="changeLogWindow()">
        <option value="all">All</option>
        <option value="error">Errors</option>
        <option value="warn">Warnings</option>
      </select></span>&nbsp;&nbsp;&nbsp;&nbsp;

      <!-- Select log period -->
      <span id="log_length_span" style="visibility: visible;">Period: <select id="loglength" class="smalltext" onchange="changeLogWindow()">
        <option value=1>1 hour</option>
        <option value=3>3 hours</option>
        <option value=6 selected>6 hours</option>
        <option value=12>12 hours</option>
        <option value=24>24 hours</option>
        <option value=168>7 days</option>
        <option value=334>14 days</option>
        <option value=all>Ever</option>
      </select></span>
    </td>
      
  </tr>

  <tr>
    <td class="smalltext" valign="middle"><input type="checkbox" id="auto_scroll" onchange="autoScroll()">Auto scroll?</input></td>
    <td class="smalltext" valign="middle">Filter: <input type="text" id="filter" onchange="changeLogWindow()"></input></td>
    <td align="center">
        <table cellpadding=0 cellspacing=0><tr><td>
        <div class="btns" align="center">
          <a href="javascript:changeLogWindow()"  class="btn" > <span>Reload Logs</span> </a>
          <a href="machine_status.php?machine=nexus" target="logwindow" class="btn" > <span>System Status</span> </a>
        </div>
        </center>
        </td></tr></table>
      </td>
    </tr>
  </table>
</body>
</html>

