// class templates
#include "SparseMatrix.hpp"
#include "MutableGraph.hpp"
#include "pcm_helper.hpp"

using namespace pcm_helper;

template<class T,class R>
class thread_data{
  public:
    size_t thread_id;
    uint8_t *buffer;
    uint32_t *decoded_src;
    uint32_t *decoded_dst;
    long result;
    SparseMatrix<T,R> *graph;

    thread_data(SparseMatrix<T,R>* graph_in, size_t buffer_lengths, const size_t thread_id_in){
      graph = graph_in;
      thread_id = thread_id_in;
      decoded_src = new uint32_t[buffer_lengths]; //space for A and B
      decoded_dst = new uint32_t[buffer_lengths]; //space for A and B
      buffer = new uint8_t[buffer_lengths*sizeof(int)];
      result = 0;
    }
};

template<class T, class R>
class application{
  public:
    SparseMatrix<T,R>* graph;
    long num_triangles;
    thread_data<T,R> **t_data_pointers;
    MutableGraph *inputGraph;
    size_t num_numa_nodes;
    size_t num_threads;
    string layout;

    application(size_t num_numa_nodes_in, MutableGraph *inputGraph_in, size_t num_threads_in, string input_layout){
      num_triangles = 0;
      num_numa_nodes = num_numa_nodes_in;
      inputGraph = inputGraph_in; 
      num_threads = num_threads_in;
      layout = input_layout;
      t_data_pointers = new thread_data<T,R>*[num_threads];
    }
    #ifdef ATTRIBUTES
    inline bool myNodeSelection(uint32_t node, uint32_t attribute){
      (void)node; (void) attribute;
      return attribute > 500;
    }
    inline bool myEdgeSelection(uint32_t src, uint32_t dst, uint32_t attribute){
      (void) attribute;
      return attribute == 2012 && src < dst;
    }
    #else
    inline bool myNodeSelection(uint32_t node, uint32_t attribute){
      (void)node; (void) attribute;
      return true;
    }
    inline bool myEdgeSelection(uint32_t node, uint32_t nbr, uint32_t attribute){
      (void) attribute;
      return nbr < node;
    }
    #endif
    inline void allocBuffers(){
      for(size_t k= 0; k < num_threads; k++){
        t_data_pointers[k] = new thread_data<T,R>(graph, graph->max_nbrhood_size,k);
      }
    }
    inline void produceSubgraph(){
      auto node_selection = std::bind(&application::myNodeSelection, this, _1, _2);
      auto edge_selection = std::bind(&application::myEdgeSelection, this, _1, _2, _3);
#ifdef ATTRIBUTES
      cout << "running attribute code" << endl;
      graph = SparseMatrix<T,R>::from_symmetric_attribute_graph(inputGraph,node_selection,edge_selection,num_threads);
#else
      graph = SparseMatrix<T,R>::from_symmetric_graph(inputGraph,node_selection,edge_selection,num_threads);
#endif
    }

    inline void queryOver(){
      system_counter_state_t before_sstate = pcm_get_counter_state();
      server_uncore_power_state_t* before_uncstate = pcm_get_uncore_power_state();

      size_t matrix_size = graph->matrix_size;

      common::par_for_range(num_threads, 0, matrix_size, 100,
        [this](size_t tid, size_t i) {
           long t_num_triangles = 0;
           uint32_t *src_buffer = t_data_pointers[tid]->decoded_src;
           uint32_t *dst_buffer = t_data_pointers[tid]->decoded_dst;

           Set<R> A = this->graph->get_decoded_row(i,src_buffer);
           Set<R> C(this->t_data_pointers[tid]->buffer);

           A.foreach([this, &A, &C, &dst_buffer, &t_num_triangles] (uint32_t j){
             Set<R> B = this->graph->get_decoded_row(j,dst_buffer);
             t_num_triangles += ops::set_intersect(C,A,B).cardinality;
           });

           this->t_data_pointers[tid]->result += t_num_triangles;
        }
      );

      num_triangles = 0;
      for(size_t k = 0; k < num_threads; k++)
         num_triangles += t_data_pointers[k]->result;

    server_uncore_power_state_t* after_uncstate = pcm_get_uncore_power_state();
    pcm_print_uncore_power_state(before_uncstate, after_uncstate);
    system_counter_state_t after_sstate = pcm_get_counter_state();
    pcm_print_counter_stats(before_sstate, after_sstate);
  }

  inline void run(){
    double start_time = common::startClock();
    produceSubgraph();
    common::stopClock("Selections",start_time);


    //common::startClock();
    allocBuffers();
    //common::stopClock("Allocating Buffers");

    graph->print_data("graph.txt");

    if(pcm_init() < 0)
       return;

    start_time = common::startClock();
    queryOver();
    common::stopClock("UNDIRECTED TRIANGLE COUNTING",start_time);

    cout << "Count: " << num_triangles << endl << endl;
    pcm_cleanup();
  }
};

//Ideally the user shouldn't have to concern themselves with what happens down here.
int main (int argc, char* argv[]) { 
  if(argc < 4){
    cout << "Please see usage below: " << endl;
    cout << "\t./main <adjacency list file/folder> <# of threads> <layout type=bs,pshort,uint,hybrid,v,bp> <optional attribute file>" << endl;
    exit(0);
  }

  size_t num_threads = atoi(argv[2]);
  cout << endl << "Number of threads: " << num_threads << endl;
  omp_set_num_threads(num_threads);
  common::init_threads(num_threads);

  std::string input_layout = argv[3];

  size_t num_nodes = 1;
  double file_reading = common::startClock();
#ifdef TEXT_INPUT
  MutableGraph *inputGraph = MutableGraph::undirectedFromEdgeList(argv[1]); //filename, # of files
#endif
#ifdef ATTRIBUTES
  MutableGraph *inputGraph = MutableGraph::undirectedFromAttributeList(argv[1],argv[4]); //filename, # of files
#endif
#ifdef BINARY
  MutableGraph *inputGraph = MutableGraph::undirectedFromBinary(argv[1]); //filename, # of files
#endif
  common::stopClock("Reading File",file_reading);

  if(input_layout.compare("uint") == 0){
    application<uinteger,uinteger> myapp(num_nodes,inputGraph,num_threads,input_layout);
    myapp.run();
  } else if(input_layout.compare("bs") == 0){
    application<bitset,bitset> myapp(num_nodes,inputGraph,num_threads,input_layout);
    myapp.run();  
  } else if(input_layout.compare("pshort") == 0){
    application<pshort,pshort> myapp(num_nodes,inputGraph,num_threads,input_layout);
    myapp.run();  
  } else if(input_layout.compare("hybrid") == 0){
    application<hybrid,hybrid> myapp(num_nodes,inputGraph,num_threads,input_layout);
    myapp.run();  
  } 
  #if COMPRESSION == 1
  else if(input_layout.compare("v") == 0){
    application<variant,uinteger> myapp(num_nodes,inputGraph,num_threads,input_layout);
    myapp.run();  
  } else if(input_layout.compare("bp") == 0){
    application<bitpacked,uinteger> myapp(num_nodes,inputGraph,num_threads,input_layout);
    myapp.run();
  } 
  #endif
  else{
    cout << "No valid layout entered." << endl;
    exit(0);
  }

  return 0;
}
