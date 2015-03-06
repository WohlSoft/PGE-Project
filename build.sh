#/bin/bash
bak= ~+
cd $PWD

source "$PWD/_paths.sh"

#=======================================================================
errorofbuid()
{
	printf "\n\n=========ERROR!!===========\n\n"
	cd $bak
	exit 1
}

checkState()
{
	if [ $? -eq 0 ]
	then
	  echo "[good]"
	else
	  errorofbuid
	fi
}

#=======================================================================
# build translations of the editor
cd Editor
$LRelease *.pro
checkState
cd ..

#=======================================================================
# build all components
$QMake CONFIG+=release CONFIG-=debug
checkState

#=======================================================================
make
checkState

#=======================================================================
# copy data and configs into the build directory
make install
checkState

#=======================================================================
printf "\n\n=========BUILT!!===========\n\n"
cd $bak
read -n 1
exit 0
