#!/bin/sh

# The following function tests if revision is in list and returns success (0) or fail (1)
# so it could be directly used in 'if' statement.
# argument 1: revision 
# argument 2: list of revisions as string separated by spaces
rev_in_list() {
    # count lines of list which contain revision
    line_cnt=$( echo $2 | grep $1 | wc -l )

    # revision not in list? fail (1)
    if [ "$line_cnt" -eq "0" ]; then
        return 1        # fail
    fi

    # revision is in list, success (0)
    return 0            # success
}

model="unknown"

# check if we do have this file
if [ ! -f /proc/cpuinfo ] ; then    # don't have this? pretend first revision
    rev_hexa="0002"
else                                # got this? get revision from cpuinfo as hexadecimal number
    rev_hexa=$( cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}' | sed 's/^1000//' )
fi

# RPi model 1
if rev_in_list "$rev_hexa" "0002 0003 0004 0005 0006 0007 0008 0009 000d 000e 000f 0010 0011 0012 0013 0014 0015"; then
    model="1"
fi

# RPi model 2
if rev_in_list "$rev_hexa" "a01040 a01041 a21041 a22042"; then
    model="2"
fi

# RPi Zero, so model 1
if rev_in_list "$rev_hexa" "900021 900032 900092 900093 920093 9000c1"; then
    model="1"
fi

# RPi model 3
if rev_in_list "$rev_hexa" "a02082 a020a0 a22082 a32082 a020d3 9020e0 a02100"; then
    model="3"
fi

# RPi model 4
if rev_in_list "$rev_hexa" "a03111 b03111 c03111"; then
    model="4"
fi

# output model to stdout, so it can be used in other scripts
echo $model
