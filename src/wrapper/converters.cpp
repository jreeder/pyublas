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
  template <class TargetType>
  struct array_converter_base
  {
    typedef typename TargetType::value_type value_type;
    typedef TargetType target_type;

    static void construct(
        PyObject* obj, 
        py::converter::rvalue_from_python_stage1_data* data)
    {
      void* storage = ((py::converter::rvalue_from_python_storage<target_type>*)data)->storage.bytes;

      new (storage) target_type(py::handle<>(py::borrowed(obj)));

      // record successful construction
      data->convertible = storage;
    }

    template <class RealTgtType>
    static void construct_indirect(
        PyObject* obj, 
        py::converter::rvalue_from_python_stage1_data* data)
    {
      void* storage = ((py::converter::rvalue_from_python_storage<target_type>*)data)->storage.bytes;

      new (storage) RealTgtType(target_type(py::handle<>(py::borrowed(obj))));

      // record successful construction
      data->convertible = storage;
    }

    struct to_python
    {
      static PyObject* convert(target_type const &v)
      {
        return v.to_python().release();
      }
    };

    template <class OriginalType>
    struct indirect_to_python
    {
      static PyObject* convert(OriginalType const &v)
      {
        target_type copied_v(v);
        return copied_v.to_python().release();
      }
    };
  };




  template <class VectorType>
  struct vector_converter : public array_converter_base<VectorType>
  {
    private:
      typedef array_converter_base<VectorType> super;

    public:
      static void *check(PyObject* obj)
      {
        if (!PyArray_Check(obj))
          return 0;
        if (PyArray_TYPE(obj) != get_typenum(typename super::value_type()))
          return 0;
        if (!PyArray_CHKFLAGS(obj, NPY_ALIGNED))
          return 0;
        if (PyArray_CHKFLAGS(obj, NPY_NOTSWAPPED))
          return 0;

        return obj;
      }
  };




  template <class MatrixType>
  struct matrix_converter : public array_converter_base<MatrixType>
  {
    private:
      typedef array_converter_base<MatrixType> super;

    public:
      static void *check(PyObject* obj)
      {
        if (!PyArray_Check(obj))
          return 0;
        if (PyArray_TYPE(obj) != get_typenum(typename super::value_type()))
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

  };




  const PyTypeObject *get_PyArray_Type()
  { return &PyArray_Type; }

  template <class Converter>
  void register_array_converter()
  {
    py::converter::registry::push_back(
        &Converter::check
        , &Converter::construct
        , py::type_id<typename Converter::target_type>()
#ifndef BOOST_PYTHON_NO_PY_SIGNATURES
        , &get_PyArray_Type
#endif
        );

    py::to_python_converter<
      typename Converter::target_type, typename Converter::to_python>();
  }




  template <class Converter, class RealTgtType>
  void register_indirect_array_converter()
  {
    py::converter::registry::push_back(
        &Converter::check
        , &Converter::template construct_indirect<RealTgtType>
        , py::type_id<RealTgtType>()
#ifndef BOOST_PYTHON_NO_PY_SIGNATURES
        , &get_PyArray_Type
#endif
        );

    py::to_python_converter<
      RealTgtType, typename Converter::template indirect_to_python<RealTgtType> >();
  }



  // array scalars ------------------------------------------------------------
  template <class T>
  const PyTypeObject *get_array_scalar_typeobj()
  { 
    return (PyTypeObject *) PyArray_TypeObjectFromType(get_typenum(T())); 
  }

  template <class T>
  void *check_array_scalar(PyObject *obj)
  { 
    if (obj->ob_type == get_array_scalar_typeobj<T>())
      return obj; 
    else
      return 0;
  }

  template <class T>
  static void convert_array_scalar(
      PyObject* obj, 
      py::converter::rvalue_from_python_stage1_data* data)
  {
    void* storage = ((py::converter::rvalue_from_python_storage<T>*)data)->storage.bytes;

    // no constructor needed, only dealing with POD types
    PyArray_ScalarAsCtype(obj, reinterpret_cast<T *>(storage));

    // record successful construction
    data->convertible = storage;
  }




  // main exposer -------------------------------------------------------------
  template <class T>
  void expose_converters()
  {
    // conversion of array scalars
    py::converter::registry::push_back(
        check_array_scalar<T>
        , convert_array_scalar<T>
        , py::type_id<T>()
#ifndef BOOST_PYTHON_NO_PY_SIGNATURES
        , get_array_scalar_typeobj<T>
#endif
        );

    // conversion of arrays
    typedef numpy_vector<T> vec;
    typedef numpy_matrix<T, ublas::row_major> rm_mat;
    typedef numpy_matrix<T, ublas::column_major> cm_mat;

    typedef vector_converter<vec> vec_converter;
    typedef matrix_converter<cm_mat> cm_mat_converter;
    typedef matrix_converter<rm_mat> rm_mat_converter;

    register_array_converter<vec_converter>();
    register_array_converter<cm_mat_converter>();
    register_array_converter<rm_mat_converter>();

    register_indirect_array_converter
      <vec_converter, ublas::vector<T> >();
    register_indirect_array_converter
      <vec_converter, ublas::bounded_vector<T, 2> >();
    register_indirect_array_converter
      <vec_converter, ublas::bounded_vector<T, 3> >();
    register_indirect_array_converter
      <rm_mat_converter, ublas::matrix<T, ublas::row_major> >();
    register_indirect_array_converter
      <cm_mat_converter, ublas::matrix<T, ublas::column_major> >();
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
