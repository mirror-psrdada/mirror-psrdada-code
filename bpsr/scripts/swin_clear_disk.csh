#!/bin/csh

#
# Usage: $0 shrek21??
#

if ( $1 == "" ) then
  echo "Error: must provide an argument"
  echo "Usage $0 shrek21[0a|0b|1a|1b] [delete]"
  exit 1
endif

if ($2 == "delete") then
  echo "Actually Deleting files"
  set deletethem = 1
else 
  echo "Not deleting files, add 'delete' argument to delete files"
  set deletethem = 0
endif

set basedir = "/export/$1/on_tape"

set n_delete = 0
set n_skip = 0
set n_nodelete = 0

cd /nfs/cluster/pulsar/hitrun

# list all files processed by llevin
find . -name "*llevin*" | awk -F. '{print $1"/"$2}' > /tmp/llevin.done

# list all files processed by sbates
find . -name "*sbates*" | awk -F. '{print $1"/"$2}' > /tmp/sbates.done

# list all files processed by both
cd /tmp; cat llevin.done sbates.done | sort | uniq -d > both.done

foreach processed ( both llevin sbates recent )

  echo "Clearing data that have been processed by $processed"

  unlink /tmp/hitrun_delete_$1
  touch /tmp/hitrun_delete_$1

  foreach beamdir ( `cat /tmp/${processed}.done` ) 

    set obsdir = `dirname $beamdir`

    # If the observation dir exists on this disk
    if ( -d $basedir/$obsdir ) then

      # count the number of dirs to check for fully deleted beams
      set num_items = `ls -1 $basedir/$obsdir | wc -l`
      if ($num_items == 2) then
        if ($deletethem == 1 ) then
          echo "rm -rf $basedir/$obsdir"
          rm -rf $basedir/$obsdir
        else 
          echo "$obsdir would be deleted with: rm -rf $basedir/$obsdir"
        endif

      # We have a beam dir in this observation at least
      else 

        # If the beam dir in question also exists
        if ( -d "$basedir/$beamdir" ) then
          @ n_delete = $n_delete + 1
          if ( $deletethem == 1 ) then
            echo "$beamdir" >> /tmp/hitrun_delete_$1
          endif
        else
          @ n_skip = $n_skip + 1
        endif

      endif 

    # if the obs does not exist on this disk (must be on another one) 
    else 
      @ n_skip = $n_skip + 1
    endif

  end

  echo "Found $n_delete eligble for deletion on $1"
  echo "Found $n_skip eligble, but not on disk $1"
  echo "Found $n_nodelete ineligble for deletion $1"

  if ($deletethem == 1) then

    echo To be deleted: `wc -l /tmp/hitrun_delete_$1`

    cd $basedir

    foreach deleteable ( `cat /tmp/hitrun_delete_$1` )
  
      if ( -d $deleteable ) then

        date
        echo Deleting $deleteable
        time rm -rf $deleteable

        echo Sleeping 3 seconds
        sleep 3 

      endif

    end

  endif 

end

