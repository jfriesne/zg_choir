#! /bin/bash -ex

CHOIR_APP_NAME="ZGChoir"
CHOIR_APP_VERSION=`grep CHOIR_VERSION_STRING ChoirProtocol.h | cut -d \" -f 2`

SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ZGCHOIR_FOR_MACOSX_DIR_NAME="${CHOIR_APP_NAME}_v${CHOIR_APP_VERSION}_for_MacOSX"
ZGCHOIR_FOR_MACOSX_DIR_PATH="${SCRIPT_PATH}/${ZGCHOIR_FOR_MACOSX_DIR_NAME}"
ZGCHOIR_OUTPUT_PATH="${ZGCHOIR_FOR_MACOSX_DIR_PATH}/${CHOIR_APP_NAME}_${CHOIR_APP_VERSION}"

DMG_FILE_NAME="${ZGCHOIR_FOR_MACOSX_DIR_NAME}.dmg"
DMG_FILE_PATH="${SCRIPT_PATH}/${DMG_FILE_NAME}"

# Uncomment this if you don't want to deal with codesigning issues and just want an un-signed build
# export DISABLE_CODESIGN=1

# Workaround for a bug in Qt 5.9.0's macdeployqt tool that sometimes leaves
# non-kosher LC_RPATHs in the Qt*.frameworks, causing Gatekeeper to balk
function RemoveBogusLCRPaths {
    filePath="$1"
    outputText=`otool -l $filePath | grep everywhere || true`
    while read -r line; do
       path=`echo $line | cut -d " " -f 2`
       echo "PATH=$path"
       if [[ $path == /* ]] ;
       then
          echo "FogBugz #11027 work-around:  Removing bogus LC_RPATH" $path "from file" $filePath
          install_name_tool -delete_rpath "$path" $filePath
       fi
    done <<< "$outputText"
}

# Equivalent to codesign, except if we have codesigning disabled then it's a no-op
function DoCodeSign {
  if [ "$DISABLE_CODESIGN" != "1" ]; then
     codesign "$@"
  else
     echo "Code signing disabled, not executing:  codesign $@"
  fi
}

# Sign the application to keep Gatekeeper off our back
function SignApp {
   # see http://stackoverflow.com/questions/19637131/sign-a-framework-for-osx-10-9
   ID="Developer ID Application: Jeremy Friesner"
   if [ "$DISABLE_CODESIGN" != "1" ]; then
      echo "Signing application: " $1 "with id:" ${ID}
   else
      echo "Pretending to sign application: " $1 "with id:" ${ID}
   fi

   pushd "${1}/Contents/Frameworks"
      for f in Qt*.framework; do
         RemoveBogusLCRPaths ${f}/Versions/Current/Qt*  # Workaround for Qt 5.9.0 macdeployqt bug
         DoCodeSign -f -v -s "${ID}" "${f}/Versions/5"
      done
   popd

   pushd "${1}/Contents/PlugIns"
      for f in $(find . -name '*.dylib' -type f); do
         DoCodeSign -f -v -s "${ID}" "${f}"
      done
   popd
   
   DoCodeSign -f -v -s "${ID}" "$1"   # No modifications to the .app folder allowed after this!
}

pushd "${SCRIPT_PATH}"
  # Get rid of any previous output folder, to avoid problems
  rm -f "${DMG_FILE_PATH}"
  rm -rf "${ZGCHOIR_FOR_MACOSX_DIR_PATH}"
  mkdir -p "${ZGCHOIR_OUTPUT_PATH}"

  # Make sure we've got a fresh ZGChoir.app built and ready to go
  rm -rf "${CHOIR_APP_NAME}.app"
  make -j4

  # Deploy ZGChoir.app and move it into the output folder
  macdeployqt "./${CHOIR_APP_NAME}.app" -verbose=1 -executable="./${CHOIR_APP_NAME}.app/Contents/MacOS/${CHOIR_APP_NAME}"
  SignApp "${CHOIR_APP_NAME}.app"

  mv "./${CHOIR_APP_NAME}.app" "${ZGCHOIR_OUTPUT_PATH}"

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
  DoCodeSign -f -v -s "Developer ID Application: Jeremy Friesner" "${DMG_FILE_PATH}"
popd  # pop back out to (wherever the user's current working directory originally was)

echo "The .dmg file should be at: " ${DMG_FILE_PATH}
