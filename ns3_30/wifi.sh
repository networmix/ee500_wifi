#!/bin/sh

set -e

INPUT_NAME1="distance"
INPUT1="0 10 20 30 40"
INPUT_NAME2=""
INPUT2=""
TRIALS=1
DURATION=30

# INPUT_NAME and INPUT can be given as command line argument in the following way:
# ./wifi.sh input_name1=desiredDataRate input1="1000 2000 5000 10000 15000 30000" input_name2=distance input2="0 10 20 30 40" trials=1 duration=30
# Otherwise, the default values are used. Remaining arguments are passed to the simulation.

for arg in "$@"
do
  case $arg in
    --input_name1=*)
      INPUT_NAME1="${arg#*=}"
      shift
      ;;
    --input1=*)
      INPUT1="${arg#*=}"
      shift
      ;;
    --input_name2=*)
      INPUT_NAME2="${arg#*=}"
      shift
      ;;
    --input2=*)
      INPUT2="${arg#*=}"
      shift
      ;;
    --trials=*)
      TRIALS="${arg#*=}"
      shift
      ;;
    --duration=*)
      DURATION="${arg#*=}"
      shift
      ;;
    *)
      ;;
  esac
done

# Print the configuration.
echo "Input name: $INPUT_NAME"
echo "Inputs: $INPUTS"
echo "Trials: $TRIALS"
echo "Duration: $DURATION"
echo "Remaining arguments: $@"

# Check if data.db exists and ask if it should be deleted.
if [ -e ./data.db ]
then
  echo
  echo "data.db already exists."
  echo "Kill data.db? (y/n)"
  read answer
  if [ "$answer" = "y" ]
  then
    rm data.db
  fi
fi

# Clean-up previous runs.
rm -f *.txt *.pcap

CWD="$PWD"
BASE=$(basename "$PWD")
NS3DIR="/home/networmix/ee500/ns-allinone-3.30/ns-3.30"
cd $NS3DIR

for trial in $(seq 1 $TRIALS)
do
  for input1 in $INPUT1
  do
    # Check if input2 is given. Then we need a nested loop.
    if [ -z "$INPUT2" ]
    then
        # Create the command with one input.
        CMD="./waf --cwd=\"$CWD\" --run \"$BASE --$INPUT_NAME1=$input1 --input=\"$INPUT_NAME1=$input1\" --runID="$trial-$input1" --duration=$DURATION $@\""
        echo Trial: $trial Input: $input1
        echo "Running: $CMD"
        eval $CMD
    else
      for input2 in $INPUT2
      do
        # Create the command with two inputs.
        CMD="./waf --cwd=\"$CWD\" --run \"$BASE --$INPUT_NAME1=$input1 --$INPUT_NAME2=$input2 --input=\"$INPUT_NAME1=$input1,$INPUT_NAME2=$input2\" --runID="$trial-$input1-$input2" --duration=$DURATION $@\""
        echo Trial: $trial Input1: $input1 Input2: $input2
        echo "Running: $CMD"
        eval $CMD
      done
    fi 
    
  done
done

echo "Done!" 
echo "Data location: ./data.db"
echo "Run ./ee500_wifi.ipynb to analyze it."
