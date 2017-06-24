#! /bin/bash -ex

CHOIR_APP_NAME="ZGChoir"
CHOIR_APP_VERSION=`grep CHOIR_VERSION_STRING ChoirProtocol.h | cut -d \" -f 2`

SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SOURCE_PACKAGE_DIR_NAME="${CHOIR_APP_NAME}_v${CHOIR_APP_VERSION}_Source"
SOURCE_PACKAGE_DIR_PATH="${SCRIPT_PATH}/${SOURCE_PACKAGE_DIR_NAME}"
ZGCHOIR_OUTPUT_PATH="${SOURCE_PACKAGE_DIR_PATH}/${CHOIR_APP_NAME}_${CHOIR_APP_VERSION}"

ZIP_FILE_NAME="${SOURCE_PACKAGE_DIR_NAME}.zip"
ZIP_FILE_PATH="${SCRIPT_PATH}/${ZIP_FILE_NAME}"

pushd "${SCRIPT_PATH}"
  # Get rid of any previous output folder, to avoid problems
  rm -f "${ZIP_FILE_PATH}"
  rm -rf "${SOURCE_PACKAGE_DIR_PATH}"
  mkdir -p "${SOURCE_PACKAGE_DIR_PATH}"

  pushd "${SOURCE_PACKAGE_DIR_PATH}"
     pushd "${SCRIPT_PATH}/../.."
     git archive --format zip --output "${SOURCE_PACKAGE_DIR_PATH}/zg_choir.zip" master
     popd   # pop back to ${SOURCE_PACKAGE_DIR_PATH}
     mkdir zg_choir 
     pushd zg_choir
        unzip -q ../zg_choir.zip ; rm ../zg_choir.zip
        svn export https://github.com/jfriesne/muscle/trunk
        mv trunk muscle
     popd   # pop back out of zg_choir
     zip -r "${ZIP_FILE_PATH}" zg_choir
  popd   # pop back to the ${SCRIPT_PATH}
popd  # pop back out to (wherever the user's current working directory originally was)

echo "The .zip file should be at: " ${ZIP_FILE_PATH}
