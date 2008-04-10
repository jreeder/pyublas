//
// Copyright (c) 2008 Andreas Kloeckner
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies and
// that both that copyright notice and this permission notice appear
// in supporting documentation.  The authors make no representations
// about the suitability of this software for any purpose.
// It is provided "as is" without express or implied warranty.
//




#include <pyublas/numpy.hpp>
#include <boost/python/converter/implicit.hpp>
#include <boost/python/converter/registry.hpp>

#include <iostream>
#include <boost/numeric/ublas/io.hpp>




using namespace pyublas;
namespace py = boost::python;
namespace ublas = boost::numeric::ublas;




namespace
{
  template <class VectorType>
  struct vector_converter
  {
    typedef typename VectorType::value_type value_type;
    typedef VectorType tgt_type;

    static void *check(PyObject* obj)
    {
      if (!PyArray_Check(obj))
        return 0;
      if (PyArray_TYPE(obj) != get_typenum(value_type()))
        return 0;
      if (!PyArray_CHKFLAGS(obj, NPY_ALIGNED))
        return 0;
      if (PyArray_CHKFLAGS(obj, NPY_NOTSWAPPED))
        return 0;

      return obj;
    }

    static void construct(
        PyObject* obj, 
        py::converter::rvalue_from_python_stage1_data* data)
    {
      void* storage = ((py::converter::rvalue_from_python_storage<tgt_type>*)data)->storage.bytes;

      new (storage) tgt_type(py::handle<>(py::borrowed(obj)));

      // record successful construction
      data->convertible = storage;
    }

    struct to_python
    {
      static PyObject* convert(tgt_type const &v)
      {
        return py::handle<>(v.data().handle()).release();
      }
    };

    template <class OriginalType>
    struct indirect_to_python
    {
      static PyObject* convert(OriginalType const &v)
      {
        tgt_type copied_v(v);
        return copied_v.to_python().release();
      }
    };
  };




  template <class MatrixType>
  struct matrix_converter
  {
    typedef typename MatrixType::value_type value_type;
    typedef MatrixType tgt_type;

    static void *check(PyObject* obj)
    {
      if (!PyArray_Check(obj))
        return 0;
      if (PyArray_TYPE(obj) != get_typenum(value_type()))
        return 0;
      if (!PyArray_CHKFLAGS(obj, NPY_ALIGNED))
        return 0;
      if (PyArray_NDIM(obj) != 2)
        return 0;
      if (PyArray_STRIDE(obj, 1) == PyArray_ITEMSIZE(obj))
      {
        if (!is_row_major(typename MatrixType::orientation_category()))
          return 0;
      }
      else if (PyArray_STRIDE(obj, 0) == PyArray_ITEMSIZE(obj))
      {
        if (is_row_major(typename MatrixType::orientation_category()))
          return 0;
      }
      else
      {
        // no dim has stride == 1
        return 0;
      }

      return obj;
    }

    static void construct(
        PyObject* obj, 
        py::converter::rvalue_from_python_stage1_data* data)
    {
      void* storage = ((py::converter::rvalue_from_python_storage<tgt_type>*)data)->storage.bytes;

      new (storage) tgt_type(py::handle<>(py::borrowed(obj)));

      // record successful construction
      data->convertible = storage;
    }

    struct to_python
    {
      static PyObject* convert(tgt_type const &v)
      {
        return v.to_python().release();
      }
    };

    template <class OriginalType>
    struct indirect_to_python
    {
      static PyObject* convert(OriginalType const &v)
      {
        tgt_type copied_v(v);
        return copied_v.to_python().release();
      }
    };
  };




  const PyTypeObject *get_PyArray_Type()
  { return &PyArray_Type; }

  template <class Converter>
  void register_array_converter()
  {
    py::converter::registry::push_back(
        &Converter::check
        , &Converter::construct
        , py::type_id<typename Converter::tgt_type>()
#ifndef BOOST_PYTHON_NO_PY_SIGNATURES
        , &get_PyArray_Type
#endif
        );

    py::to_python_converter<
      typename Converter::tgt_type, typename Converter::to_python>();
  }




  template<class T>
  T dodbl(T const &x)
  {
    std::cout << x << std::endl;
    return (typename T::value_type)(2)*x;
  }

  template <class T>
  void expose_converters()
  {
    typedef numpy_vector<T> vec;
    typedef numpy_matrix<T, ublas::row_major> rm_mat;
    typedef numpy_matrix<T, ublas::column_major> cm_mat;

    typedef vector_converter<vec> vec_converter;
    typedef matrix_converter<cm_mat> cm_mat_converter;
    typedef matrix_converter<rm_mat> rm_mat_converter;
    register_array_converter<vec_converter>();
    register_array_converter<cm_mat_converter>();
    register_array_converter<rm_mat_converter>();

    py::def("dblmat", dodbl<rm_mat>);
    py::def("dblmat", dodbl<cm_mat>);
    py::def("dblvec", dodbl<vec>);

#define EXPOSE_TO_PYTHON_INDIRECT(CONVERTER, TP) \
    py::to_python_converter< \
      TP, typename CONVERTER::template indirect_to_python<TP> >()

    {
      typedef ublas::vector<T> cl;
      py::to_python_converter<cl, 
        typename vec_converter::template indirect_to_python<cl> >();
    }
    {
      typedef ublas::bounded_vector<T, 3> cl;
      py::to_python_converter<cl, 
        typename vec_converter::template indirect_to_python<cl> >();
    }
    {
      typedef ublas::matrix<T, ublas::row_major> cl;
      py::to_python_converter<cl, 
        typename rm_mat_converter::template indirect_to_python<cl> >();
    }
    {
      typedef ublas::matrix<T, ublas::column_major> cl;
      py::to_python_converter<cl, 
        typename cm_mat_converter::template indirect_to_python<cl> >();
    }

    py::implicitly_convertible<vec, ublas::vector<T> >();
    py::implicitly_convertible<vec, ublas::bounded_vector<T, 3> >();
    py::implicitly_convertible<rm_mat, ublas::matrix<T, ublas::row_major> >();
    py::implicitly_convertible<cm_mat, ublas::matrix<T, ublas::column_major> >();
  }
}




void pyublas_expose_converters()
{
  import_array();

  expose_converters<npy_byte>();
  expose_converters<npy_short>();
  expose_converters<npy_ushort>();
  expose_converters<npy_int>();
  expose_converters<npy_uint>();
  expose_converters<npy_long>();
  expose_converters<npy_ulong>();
  expose_converters<npy_longlong>();
  expose_converters<npy_ulonglong>();
  expose_converters<npy_float>();
  expose_converters<npy_double>();
  expose_converters<std::complex<float> >();
  expose_converters<std::complex<double> >();
}
