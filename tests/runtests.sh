#!/usr/bin/env bash

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

cd "$parent_path"

mkdir -p bin
mkdir -p out

rm bin/* 2> /dev/null
rm out/* 2> /dev/null

function run_test {
	COLOR=`tput setaf 2`
	f=$1
	TEST_SUCCESS="true"
	NAME=$(basename "$f" .bjou)
	../build/bjou "test/$NAME.bjou" -I ../modules -o "bin/$NAME"
	if [ $? -ne 0 ]
  	then
    	TEST_SUCCESS="false"
  	fi

    # Uncomment this block to test optimizations as well.

    # if [ "$TEST_SUCCESS" == "true" ]
    # then
    #     ../build/bjou "test/$NAME.bjou" -I ../modules -O -o "bin/$NAME"
    #     if [ $? -ne 0 ]
    #     then
    #         TEST_SUCCESS="false"
    #     fi
    # fi
	
	if [ "$TEST_SUCCESS" == "true" ]
	then
		"bin/$NAME" > "out/$NAME.txt"
		if [ $? -ne 0 ]
  		then
    		TEST_SUCCESS="false"
  		fi

        if [ -f "check/$NAME.txt" ]
        then
            DIFF=$(diff <(sed 's/(nil)/0x0/g' "out/$NAME.txt") <(sed 's/(nil)/0x0/g' "check/$NAME.txt"))
            if [ "$DIFF" != "" ]
            then
                TEST_SUCCESS="false"
            fi
        else
            COLOR=`tput setaf 3`
            echo ${COLOR}$NAME missing check to diff${RESET}
        fi
	fi
	RESET=`tput sgr0`
	if [ "$TEST_SUCCESS" == "false" ]
	then
		COLOR=`tput setaf 1`
	fi
	echo ${COLOR}$NAME${RESET}
}

if [ "$#" -eq 0 ]
then
    export -f run_test
    ls test/*.bjou | xargs -L 1 -P "$(ls test/*.bjou | wc | awk '{ print $1; }')" -I FILE bash -c "run_test FILE"
else
	run_test test/$1.bjou
fi



##
