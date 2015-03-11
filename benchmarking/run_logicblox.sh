#!/bin/bash

# Change the current directory to the directory of the script
cd "$(dirname "$0")"

source env.sh

curtime="$(date +'%H-%M-%S')"
date="$(date +'%d-%m-%Y')"
numruns="7"
datasets="twitter2010" #"higgs" #orkut g_plus" # higgs socLivejournal orkut g_plus"
system="logicblox"
num_threads="1"

odir="${output}/${system}_${date}_${curtime}"
mkdir $odir
echo $odir
for i in `seq 1 $numruns`; do
  for dataset in $datasets; do
    for threads in $num_threads; do
       cd ${system}
       echo "Benchmarking ${system} on ${dataset} with ${threads} threads"
       ./run.sh /dfs/scratch0/caberger/datasets/${dataset} |& tee $odir/${system}.${dataset}.${threads}.${i}.log
       cd ..
    done
  done
done