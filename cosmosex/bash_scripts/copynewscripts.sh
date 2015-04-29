function compareAndCopy 
{
    # create md5 checksums
    sum_loc=($( cat $1 2> /dev/null | md5sum ))
    sum_new=($( cat $2 2> /dev/null | md5sum ))
    
    # if checksums don't match, copy the new file
    if [ $sum_loc != $sum_new ]; then
        echo "Copying new file to $1"
        rm -f $1        # delete local file
        cp $2 $1        # copy new file to local file
        chmod 755 $1    # make new local file executable
    else 
        echo "Skipping file $1"
    fi
}  

echo "Will compare with old and copy new files..."

#compareAndCopy  local file                      new file
compareAndCopy   "/ce/cesuper.sh"                "/tmp/newscripts/cesuper.sh"
compareAndCopy   "/ce/wifisuper.sh"              "/tmp/newscripts/wifisuper.sh"
compareAndCopy   "/ce/ce_conf.sh"                "/tmp/newscripts/ce_conf.sh"
compareAndCopy   "/ce/ce_start.sh"               "/tmp/newscripts/ce_start.sh"
compareAndCopy   "/ce/ce_start.sh"               "/tmp/newscripts/ce_start.sh"
compareAndCopy   "/ce/ce_stop.sh"                "/tmp/newscripts/ce_stop.sh"
compareAndCopy   "/ce/ce_firstfw.sh"             "/tmp/newscripts/ce_firstfw.sh"
compareAndCopy   "/ce/ce_update.sh"              "/tmp/newscripts/ce_update.sh"
compareAndCopy   "/ce/update/test_xc9536xl.xsvf" "/tmp/newscripts/test_xc9536xl.xsvf"
compareAndCopy   "/ce/update/test_xc9572xl.xsvf" "/tmp/newscripts/test_xc9572xl.xsvf"
compareAndCopy   "/ce/update/update_app.sh"      "/tmp/newscripts/update_app.sh"
compareAndCopy   "/ce/update/update_franz.sh"    "/tmp/newscripts/update_franz.sh"
compareAndCopy   "/ce/update/update_hans.sh"     "/tmp/newscripts/update_hans.sh"
compareAndCopy   "/ce/update/update_xilinx.sh"   "/tmp/newscripts/update_xilinx.sh"
                
echo "Doing sync..."
sync
echo "All files should be up to date now."
