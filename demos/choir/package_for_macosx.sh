#! /bin/bash -ex

CHOIR_APP_NAME="ZGChoir"
CHOIR_APP_VERSION=`grep CHOIR_VERSION_STRING ChoirProtocol.h | cut -d \" -f 2`

SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ZGCHOIR_FOR_MACOSX_DIR_NAME="${CHOIR_APP_NAME}_v${CHOIR_APP_VERSION}_for_MacOSX"
ZGCHOIR_FOR_MACOSX_DIR_PATH="${SCRIPT_PATH}/${ZGCHOIR_FOR_MACOSX_DIR_NAME}"
ZGCHOIR_OUTPUT_PATH="${ZGCHOIR_FOR_MACOSX_DIR_PATH}/${CHOIR_APP_NAME}_${CHOIR_APP_VERSION}"

DMG_FILE_NAME="${ZGCHOIR_FOR_MACOSX_DIR_NAME}.dmg"
DMG_FILE_PATH="${SCRIPT_PATH}/${DMG_FILE_NAME}"

pushd "${SCRIPT_PATH}"
  # Get rid of any previous output folder, to avoid problems
  rm -f "${DMG_FILE_PATH}"
  rm -rf "${ZGCHOIR_FOR_MACOSX_DIR_PATH}"
  mkdir -p "${ZGCHOIR_OUTPUT_PATH}"

  # Make sure we've got a ZGChoir.app built and ready to go
  make -j4

  # Deploy ZGChoir.app and move it into the output folder
  macdeployqt "./${CHOIR_APP_NAME}.app" -verbose=1 -executable="./${CHOIR_APP_NAME}.app/Contents/MacOS/${CHOIR_APP_NAME}"
  mv "./${CHOIR_APP_NAME}.app" "${ZGCHOIR_OUTPUT_PATH}"
  # TODO:  codesign the .app here!

  pushd "${ZGCHOIR_FOR_MACOSX_DIR_PATH}"
    pushd "${ZGCHOIR_OUTPUT_PATH}"
      README_NAME="README.html"
      cp "${SCRIPT_PATH}/html/${README_NAME}" .
      cp -R "${SCRIPT_PATH}/html/images" ./images
      cp -R "${SCRIPT_PATH}/songs" ./songs
    popd   # pop back to the ${ZGCHOIR_FOR_MACOSX_DIR_PATH}
    ln -s /Applications "Drag here to install"
  popd   # pop back to the ${SCRIPT_PATH}
  hdiutil create -srcfolder "${ZGCHOIR_FOR_MACOSX_DIR_NAME}" "${DMG_FILE_PATH}"
  # TODO:  codesign the .dmg here!
popd  # pop back out to (wherever the user's current working directory originally was)

echo "The .dmg file should be at: " ${DMG_FILE_PATH}
