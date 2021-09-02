#!/bin/sh
echo "Generating doxygen documentation.  "
echo "You will need to have doxygen installed in"
echo "order for this script to work."
echo ""
export ZG_VERSION=$(grep ZG_VERSION_STRING ../../include/zg/ZGConstants.h | cut -c 28-31)
doxygen zg.dox
echo "Done!"
