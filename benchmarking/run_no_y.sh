#!/bin/bash

# Change the current directory to the directory of the script
cd "$(dirname "$0")"

output=/dfs/scratch0/noetzli/output
datasets="g_plus higgs socLivejournal orkut cid-patents"
orderings="random degree"
STABLE_BIN_DIR=/afs/cs.stanford.edu/u/noetzli/tmp/EmptyHeaded/stable_binaries

curtime="$(date +'%H-%M-%S')"
date="$(date +'%d-%m-%Y')"
numruns="7"

for system in "no_y"; do
  odir="${output}/${system}_${date}_${curtime}"
  mkdir $odir
  echo $odir
  for run in `seq $numruns`; do
   for dataset in $datasets; do
     #${STABLE_BIN_DIR}/undirected_triangle_counting_noalgo --graph=/dfs/scratch0/caberger/datasets/${dataset}/bin/u_degree.bin --input_type=binary --t=1 --layout=uint | tee $odir/${dataset}_${run}_pruned.log
     #${STABLE_BIN_DIR}/undirected_triangle_counting_noalgo_unpruned --graph=/dfs/scratch0/caberger/datasets/${dataset}/bin/u_degree.bin --input_type=binary --t=1 --layout=uint | tee $odir/${dataset}_${run}_unpruned.log
     echo ${STABLE_BIN_DIR}/undirected_lollipop_counting_slow --graph=/dfs/scratch0/caberger/datasets/${dataset}/bin/u_degree.bin --input_type=binary --t=1 --layout=hybrid | tee $odir/${dataset}_${run}_pruned.log
     ${STABLE_BIN_DIR}/undirected_lollipop_counting_slow --graph=/dfs/scratch0/caberger/datasets/${dataset}/bin/u_degree.bin --input_type=binary --t=1 --layout=hybrid | tee $odir/${dataset}_${run}_pruned.log
   done
  done
done