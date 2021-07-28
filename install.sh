#!/bin/bash

CLONE_HPVM=false
HPVM_DIR=""
NT=1
CUR_DIR=`pwd`

usage() {
  echo "Usage: $0 [-i] [-d path/to/hpvm] [-j n]" 1>&2
  echo "  -c  Clone HPVM and run its installer before installing Trireme"
  echo "  -d  Path where HPVM is installed or will be installed (if -i also provided)"
  echo "  -j  Number of threads to use when building HPVM; default is 1"
  echo "  -h  Print this help message"
  exit 1;
}

while getopts ':hcd:j:' opt; do
  case ${opt} in 
    h ) 
      usage
      ;;
    c )
      CLONE_HPVM=true
      ;;
    j )
      NT=$OPTARG
      ;;
    d )
      HPVM_DIR=$OPTARG
      ;;
    \? )
      echo "Invalid option: -$OPTARG" 1>&2
      usage
      ;;
    : )
      echo "Invalid option: -$OPTARG requires an argument" 1>&2
      usage
      ;;
  esac
done
shift $((OPTIND -1))

if [[ $HPVM_DIR == "" ]]; then
  echo "HPVM path must be provided!" 1>&2
  usage
fi

SKIP_CLONE=false

if [[ $CLONE_HPVM == true ]]; then
  echo "Trying to clone HPVM to $HPVM_DIR" 1>&2
  if [ -d $HPVM_DIR ]; then
    if [ -d $HPVM_DIR/hpvm ]; then
      echo "HPVM has already been cloned in $HPVM_DIR."
      read -p "Would you like to continue Trireme installation? [y/n] " -n 1 -r
      if [[ ! $REPLY =~ ^[Yy]$ ]]
      then
        echo "Aborting!"
        exit 1;
      fi
      echo "Continuing with Trireme Installation (skipping cloning and installing HPVM)!"
      SKIP_CLONE=true
    fi
  fi
  if [[ $SKIP_CLONE == false ]]; then 
    echo "Cloning HPVM into $HPVM_DIR"
    echo "git clone git@gitlab.engr.illinois.edu:llvm/hpvm-release.git $HPVM_DIR"
    git clone git@gitlab.engr.illinois.edu:llvm/hpvm-release.git $HPVM_DIR
    cd $HPVM_DIR/hpvm
    bash ./install.sh -j $NT -t X86 --no-pypkg --no-params
    if [ $? -ne 0 ]; then
      echo "Something is wrong. Installation unsuccesful. Exiting!"
      exit 1
    else
      echo "HPVM installation successful. Now installing trireme."
    fi
  fi
fi

cd $CUR_DIR
echo "Copying hpvm-trireme as an HPVM project then re-building HPVM."
cp -r $CUR_DIR/hpvm-trireme $HPVM_DIR/hpvm/projects
cd $HPVM_DIR/hpvm/build
make -j $NT
echo "Done..."
cd $CUR_DIR
echo "Creating paths script..."
echo "export HPVM_BUILD_DIR=$HPVM_DIR/hpvm/build" > set_paths.sh
echo "export PATH=$HPVM_BUILD_DIR/bin:$PATH" >>set_paths.sh
echo "**********************************************************"
echo " Installation script complete! Please set up required env"
echo " variables by running the following command in terminal:"
echo "    source ./set_paths.sh"
echo "**********************************************************"
exit 0


