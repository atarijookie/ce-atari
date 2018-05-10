#!/bin/sh

# create a storage place for all the lists
mkdir -p /ce/lists

# download the current list of lists into tmp
list="/tmp/listoflists.csv"
cd /tmp
rm -f $list
wget -q http://joo.kie.sk/cosmosex/update/listoflists.csv

# if we will find some file which we will download, set this flag and later generate the merged list
something_updated=0

# go throught all the lines list of lists, extract version and url, download all that is newer or missing
while read -r line
do
    # extract version, url, filename
    web_version=$( echo "$line" | awk -F, '{print $1}' | cut -c1-10 )
    url=$( echo "$line" | awk -F, '{print $2}' )
    filename=$( basename "$url" )

    # check if local file exists
    localpath="/ce/lists/$filename"

    should_dn=0

    # if local file exists, check version
    if [ -f "$localpath" ]; then
        local_version=$( head -n 1 "$localpath" | cut -c1-10 )

        # if web and local versions differ, download it
        if [ "$web_version" != "$local_version" ]; then
            should_dn=1
        fi
    else    # if local file doesn't exist, download it
        should_dn=1
        echo "local file doesn't exist"
    fi

    # if should download file
    if [ "$should_dn" -eq "1" ]; then
        something_updated=1     # mark that we need to create the merged list in the end

        cd /ce/lists            # go to lists dir
        rm -f "$localpath"      # delete localfile if exists
        echo "downloading: $url -> $localpath"
        wget -q "$url"          # get it from internet

        # replace <br> with newline chars
        cat "$localpath" | sed 's/<br>/\n/g' > "$localpath.new"
        mv "$localpath.new" "$localpath"
    fi

done < "$list"

# check if something updated or not
if [ "$something_updated" -eq "0" ]; then
    echo "no changes in the image lists, quit"
    exit 0
fi

# go throught all the lines list of lists, create one merged list
rm -f /ce/lists/merged.csv

while read -r line
do
    # extract filename
    url=$( echo "$line" | awk -F, '{print $2}' )
    filename=$( basename "$url" )

    # create local path to file
    localpath="/ce/lists/$filename"

    # remove first line from the file and merge it into final file
    tail -n +2 "$localpath" >> /ce/lists/merged.csv
done < "$list"

echo "lists merged into one large list"
