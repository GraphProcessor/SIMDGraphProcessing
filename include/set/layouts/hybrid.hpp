#ifndef _HYBRID_H_
#define _HYBRID_H_
/*

THIS CLASS IMPLEMENTS THE FUNCTIONS ASSOCIATED WITH AN HYBRID SET LAYOUT.
HERE WE DYNAMICALLY DETECT THE UNDERLYING LAYOUT OF THE SET AND
RUN THE CORRESPONDING SET MEHTODS.

*/


#include "uinteger.hpp"
#include "pshort.hpp"
#include "bitset.hpp"
#include "bitpacked.hpp" //this file includes variant.hpp

class hybrid{
  public:
    static common::type get_type();
    static common::type get_type(const uint32_t *data, const size_t length);
    static common::type compute_type(const double density);
    static size_t build(uint8_t *r_in, const uint32_t *data, const size_t length);
    static size_t build_flattened(uint8_t *r_in, const uint32_t *data, const size_t length);
    static tuple<size_t,size_t,common::type> get_flattened_data(const uint8_t *set_data, const size_t cardinality);
    static void foreach(const std::function <void (uint32_t)>& f,const uint8_t *data_in, const size_t cardinality, const size_t number_of_bytes, const common::type t);
};
inline common::type hybrid::get_type(){
  return common::PSHORT;
}
inline common::type hybrid::compute_type(const double density){
  if( density > (double) 1/32 ){
    return common::BITSET;
  } else if(((1/density)*(1/65536)) > 12){
    return common::PSHORT;
  } else {
    return common::UINTEGER;
  } 
}
inline common::type hybrid::get_type(const uint32_t *data, const size_t length){
  if(length > 0){
    uint32_t max_value = data[length-1];
    double sparsity = (double) length/max_value;
    if( sparsity > (double) 1/32 ){
      return common::BITSET;
    } else if((length/((max_value >> 16) - (data[0] >> 16) + 1)) > 8){
      return common::PSHORT;
    } else {
      return common::UINTEGER;
    } 
  } else{
    return common::UINTEGER;
  }
}

//Copies data from input array of ints to our set data r_in
inline size_t hybrid::build(uint8_t *r_in, const uint32_t *data, const size_t length){
  common::type t = hybrid::get_type(data,length);
  switch(t){
    case common::UINTEGER :
      return uinteger::build(r_in,data,length);
    break;
    case common::PSHORT :
      return pshort::build(r_in,data,length);
    break;
    case common::BITSET :
      return bitset::build(r_in,data,length);
    break;
    case common::VARIANT :
      return variant::build(r_in,data,length);
    break;
    case common::BITPACKED :
      return bitpacked::build(r_in,data,length);
    break;
    default:
      return 0;
  }
}
//Nothing is different about build flattened here. The number of bytes
//can be infered from the type. This gives us back a true CSR representation.
inline size_t hybrid::build_flattened(uint8_t *r_in, const uint32_t *data, const size_t length){
  common::type t = hybrid::get_type(data,length);
  r_in[0] = (uint8_t) t;
  switch(t){
    case common::UINTEGER :
      return 1+uinteger::build_flattened(&r_in[1],data,length);
    break;
    case common::PSHORT :
      return 1+pshort::build_flattened(&r_in[1],data,length);
    break;
    case common::BITSET :
      return 1+bitset::build_flattened(&r_in[1],data,length);
    break;
    case common::VARIANT :
      return 1+variant::build_flattened(&r_in[1],data,length);
    break;
    case common::BITPACKED :
      return 1+bitpacked::build_flattened(&r_in[1],data,length);
    break;
    default:
      return 0;
  }
}

inline tuple<size_t,size_t,common::type> hybrid::get_flattened_data(const uint8_t *set_data, const size_t cardinality){
  common::type t = (common::type) set_data[0];
  tuple<size_t,size_t,common::type> mytup;
  switch(t){
    case common::UINTEGER :
      mytup = uinteger::get_flattened_data(&set_data[1],cardinality);
      get<0>(mytup)++;
      return mytup;
    break;
    case common::PSHORT :
      mytup = pshort::get_flattened_data(&set_data[1],cardinality);
      get<0>(mytup)++;
      return mytup;
    break;
    case common::BITSET :
      mytup = bitset::get_flattened_data(&set_data[1],cardinality);
      get<0>(mytup)++;
      return mytup;
    break;
    case common::VARIANT :
      mytup = variant::get_flattened_data(&set_data[1],cardinality);
      get<0>(mytup)++;
      return mytup;
    break;
    case common::BITPACKED :
      mytup = bitpacked::get_flattened_data(&set_data[1],cardinality);
      get<0>(mytup)++;
      return mytup;
    break;
    default:
      return make_tuple(0,0,common::UINTEGER);
  }
}

//Iterates over set applying a lambda.
inline void hybrid::foreach(const std::function <void (uint32_t)>& f, const uint8_t *data_in, 
  const size_t cardinality, const size_t number_of_bytes, const common::type t){

  switch(t){
    case common::UINTEGER :
      uinteger::foreach(f,data_in,cardinality,number_of_bytes,common::UINTEGER);
    break;
    case common::PSHORT :
      pshort::foreach(f,data_in,cardinality,number_of_bytes,common::PSHORT);
    break;
    case common::BITSET :
      bitset::foreach(f,data_in,cardinality,number_of_bytes,common::BITSET);
    break;
    case common::VARIANT :
      variant::foreach(f,data_in,cardinality,number_of_bytes,common::BITSET);
    break;
    case common::BITPACKED :
      bitpacked::foreach(f,data_in,cardinality,number_of_bytes,common::BITSET);
    break;
    default:
    break;
  }
}

#endif