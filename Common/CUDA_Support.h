#if !defined CUDA_SUPPORT_H
#define CUDA_SUPPORT_H

#include </var/scratch/jsteinbe/tensor-core-correlator/build/_deps/cudawrappers-src/include/cudawrappers/cu.hpp>

#include <boost/multi_array.hpp>


template <typename T, std::size_t DIM> class MultiArrayHostBuffer : public cu::HostMemory, public boost::multi_array_ref<T, DIM>
{
  public:
    template <typename ExtentList>
    MultiArrayHostBuffer(const ExtentList &extents, int flags = 0)
    :
      cu::HostMemory(boost::multi_array_ref<T, DIM>(0, extents).num_elements() * sizeof(T), flags),
      boost::multi_array_ref<T, DIM>((T *) *this, extents)
    {
    }

    size_t bytesize() const
    {
      return this->num_elements() * sizeof(T);
    }
};

#endif
