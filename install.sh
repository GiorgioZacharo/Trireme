#!/bin/bash

CLONE_HPVM=false
HPVM_DIR=""
NT=1
CUR_DIR=`pwd`
UPDATE_BASHRC=false

usage() {
  echo "Usage: $0 [-c] [-d path/to/hpvm] [-j n] [-b]" 1>&2
  echo "  -c  Clone HPVM and run its installer before installing Trireme"
  echo "  -d  Path where HPVM is installed or will be cloned (if -c also provided)"
  echo "  -j  Number of threads to use when building HPVM; default is 1"
  echo "  -b  Add environment variables to user bashrc"
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
    b )
      UPDATE_BASHRC=true
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

if [[ ! "$HPVM_DIR" = /* ]]; then
  echo "Transforming relative path to absolute:"
  echo "$HPVM_DIR -> $(readlink -f $HPVM_DIR)"
  HPVM_DIR=$(readlink -f $HPVM_DIR)
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
if [[ $(diff -r ./hpvm-trireme $HPVM_DIR/hpvm/projects/hpvm-trireme) ]]; then
  echo "Copying hpvm-trireme into HPVM source tree..."
  cp -r $CUR_DIR/hpvm-trireme $HPVM_DIR/hpvm/projects
fi
echo "Patching HPVM source files..."
export HPVM_SRC_ROOT=$HPVM_DIR/hpvm
bash ./hpvm_patches/patch_hpvm.sh
echo "Rebuilding HPVM..."
cd $HPVM_DIR/hpvm/build
make -j $NT
echo "Done..."
cd $CUR_DIR
echo "Creating paths script..."
echo "export HPVM_SRC_ROOT=$HPVM_DIR/hpvm" > set_paths.sh
echo 'export HPVM_BUILD_DIR=$HPVM_SRC_ROOT/build' >> set_paths.sh
echo 'export PATH=$HPVM_BUILD_DIR/bin:$PATH' >>set_paths.sh
echo "**********************************************************"
echo " Installation script complete! Please set up required env"
echo " variables by running the following command in terminal:"
echo "    source ./set_paths.sh"
echo "**********************************************************"

if [[ UPDATE_BASHRC == true ]]; then
  echo "Adding Environment Variables to bashrc (~/.bashrc)"
  echo "export HPVM_SRC_ROOT=$HPVM_DIR" >> $HOME/.bashrc
  echo "export HPVM_BUILD_DIR=$HPVM_DIR/hpvm/build" >> $HOME/.bashrc
  echo 'export PATH=$HPVM_BUILD_DIR/bin:$PATH' >> $HOME/.bashrc
  echo "Done adding environment variales!"
fi

exit 0


