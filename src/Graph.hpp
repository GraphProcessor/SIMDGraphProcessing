#ifndef GRAPH_H
#define GRAPH_H

#include <omp.h>
#include <unordered_map>
#include <xmmintrin.h>
#include <cstring>
#include <immintrin.h>
#include <unordered_map>
#include "Partition.hpp"

using namespace std;

struct CompressedGraph {
  const size_t num_nodes;
  const size_t num_edges;
  const size_t edge_array_length;
  const unsigned int *nbr_lengths;
  const size_t *nodes;
  const unsigned short *edges;
  const unordered_map<size_t,size_t> *external_ids;
  
  CompressedGraph(  
    const size_t num_nodes_in, 
    const size_t num_edges_in,
    const size_t edge_array_length_in,
    const unsigned int *nbrs_lengths_in, 
    const size_t *nodes_in,
    const unsigned short *edges_in,
    const unordered_map<size_t,size_t> *external_ids_in): 
      num_nodes(num_nodes_in), 
      num_edges(num_edges_in),
      edge_array_length(edge_array_length_in),
      nbr_lengths(nbrs_lengths_in),
      nodes(nodes_in), 
      edges(edges_in),
      external_ids(external_ids_in){}
    
    inline size_t getEndOfNeighborhood(const size_t node){
      size_t end = 0;
      if(node+1 < num_nodes) end = nodes[node+1];
      else end = edge_array_length;
      return end;
    }
    inline size_t neighborhoodStart(const size_t node){
      return nodes[node];
    }
    inline size_t neighborhoodEnd(const size_t node){
      size_t end = 0;
      if(node+1 < num_nodes) end = nodes[node+1];
      else end = edge_array_length;
      return end;
    }
    inline void setupInnerPartition(size_t &i, size_t &prefix, size_t &end,bool &isBitSet){
      const size_t header_length = 2;
      const size_t start = i;
      prefix = edges[i++];
      const size_t len = edges[i++];
      isBitSet = len > WORDS_IN_BS;
      end = start+len+header_length;
    }
    inline long countTriangles(){
      long count = 0;
      cout << "Num threads: " << omp_get_num_threads() << endl;
      #pragma omp parallel for default(none) schedule(static,150) reduction(+:count)   
      for(size_t i = 0; i < num_nodes; ++i){
        count += foreachNbr(i,&CompressedGraph::intersect_neighborhoods);
      }
      return count;
    }
    inline long intersect_neighborhoods(const size_t n, const size_t nbr) {
      const size_t start1 = neighborhoodStart(n);
      const size_t end1 = neighborhoodEnd(n);

      const size_t start2 = neighborhoodStart(nbr);
      const size_t end2 = neighborhoodEnd(nbr);

      return intersect_partitioned(n,edges+start1,edges+start2,end1-start1,end2-start2);
    }
    inline long foreachNbr(size_t node,long (CompressedGraph::*func)(const size_t,const size_t)){
      long count = 0;
      const size_t start1 = neighborhoodStart(node);
      const size_t end1 = neighborhoodEnd(node);
      for(size_t j = start1; j < end1; ++j){
        bool isBitSet;
        size_t prefix, inner_end;
        setupInnerPartition(j,prefix,inner_end,isBitSet);
       
        bool notFinished = (node >> 16) >= prefix; //this is t counting specific

        if(!isBitSet){
          while(j < inner_end && notFinished){
            const size_t cur = (prefix << 16) | edges[j];
            
            notFinished = node > cur; //has to be reverse cause cutoff could
            //be in the middle of a partition.
            
            if(notFinished){
              long ncount = (this->*func)(cur,node);
              count += ncount;
            } else break;
            ++j;
          }
        }else{
          size_t ii = 0;
          inner_end = j + WORDS_IN_BS;
          while((ii < WORDS_IN_BS) && notFinished){
            size_t jj = 0;
            while((jj < 16) && notFinished){
              if((edges[ii+j] >> jj) % 2){
                const size_t cur = (prefix << 16) | (jj + (ii << 4));
                
                notFinished = node > cur; //has to be reverse cause cutoff could
                
                if(notFinished){
                  long ncount = (this->*func)(cur,node);
                  count += ncount;
                } else break;

              }
              ++jj;
            }
            ++ii;
          }
        }//end else */
        j = inner_end-1;   
      }
      return count;
    }
    /*
    inline void printGraph(){
      for(size_t i = 0; i < num_nodes; ++i){
        size_t start1 = nodes[i];
        
        size_t end1 = 0;
        if(i+1 < num_nodes) end1 = nodes[i+1];
        else end1 = edge_array_length;

        size_t j = start1;
        cout << "Node: " << i <<endl;

        while(j < end1){
          unsigned int prefix = (edges[j] << 16);
          size_t len = edges[j+1];
          j += 2;
          size_t inner_end;
          if(len > WORDS_IN_BS){
            inner_end = j+4096;
            printBitSet(prefix,WORDS_IN_BS,&edges[j]);
            j = inner_end;
          }
          else{
            inner_end = j+len;
            while(j < inner_end){
              unsigned int tmp = prefix | edges[j];
              cout << "Nbr: " << tmp << endl;
              ++j;
            }
            j--;
          }
          j = inner_end;
        }
        cout << endl;
      }
    }
    inline double pagerank(){
      float *pr = new float[num_nodes];
      float *oldpr = new float[num_nodes];
      const double damp = 0.85;
      const int maxIter = 100;
      const double threshold = 0.0001;

      #pragma omp parallel for default(none) schedule(static,150) shared(pr,oldpr)
      for(size_t i=0; i < num_nodes; ++i){
        oldpr[i] = 1.0/num_nodes;
      }
      
      int iter = 0;
      double delta = 1000000000000.0;
      float totalpr = 0.0;
      while(delta > threshold && iter < maxIter){
        totalpr = 0.0;
        delta = 0.0;
        #pragma omp parallel for default(none) shared(pr,oldpr) schedule(static,150) reduction(+:delta) reduction(+:totalpr)
        for(size_t i = 0; i < num_nodes; ++i){
          const size_t start1 = nodes[i];
          const size_t end1 = getEndOfNeighborhood(i);
          
          //float *tmp_degree_holder = new float[len];
          //size_t k = 0;

          size_t j = start1;
          float sum = 0.0;
          while(j < end1){
            size_t prefix, inner_end;
            traverseInnerPartition(j,prefix,inner_end);
            while(j < inner_end){
              if(j+7 < inner_end){
                float tmp_pr_holder[8];
                float tmp_deg_holder[8];
                for(size_t k = 0; k < 8; k++){
                  size_t cur = (prefix << 16) | edges[j+k];
                  tmp_pr_holder[k] = oldpr[cur];
                  tmp_deg_holder[k] = nbr_lengths[cur];
                }
                __m256 pr = _mm256_load_ps(tmp_pr_holder);
                __m256 deg = _mm256_load_ps(tmp_deg_holder);
                __m256 d = _mm256_div_ps(pr, deg);
                _mm256_storeu_ps(tmp_pr_holder, d);
                for(size_t o = 0; o < 8; o++){
                  sum += tmp_pr_holder[o]; //should be auto vectorized.
                }
                j += 8;
                //const size_t len2 = nbr_lengths[cur];
                //sum += oldpr[cur]/len2;
              }
              else{
                const size_t cur = (prefix << 16) | edges[j];
                const size_t len2 = nbr_lengths[cur];
                sum += oldpr[cur]/len2;
                ++j;
              }
            }
          }
          pr[i] = ((1.0-damp)/num_nodes) + damp * sum;
          delta += abs(pr[i]-oldpr[i]);
          totalpr += pr[i];
        }

        float *tmp = oldpr;
        oldpr = pr;
        pr = tmp;
        ++iter;
      }
      pr = oldpr;
    
      cout << "Iter: " << iter << endl;
      
      for(size_t i=0; i < num_nodes; ++i){
        cout << "Node: " << i << " PR: " << pr[i] << endl;
      }

      return totalpr;
    }
    */
};

#endif
