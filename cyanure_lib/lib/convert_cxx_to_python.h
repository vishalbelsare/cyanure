#ifndef CONVERT_CXX_TO_PYTHON_H
#define CONVERT_CXX_TO_PYTHON_H

#if defined(_MSC_VER) && defined(_DEBUG)
#undef _DEBUG
#include <Python.h>
#define _DEBUG 1
#else
#include <Python.h>
#endif
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include "data_structure/structures/vector.h"
#include "error_management/exception.h"

#define array_type(a) (int)(PyArray_TYPE(a))
#define PyArray_SimpleNewF(nd, dims, typenum) PyArray_New(&PyArray_Type, nd, dims, typenum, NULL, NULL, 0, NPY_ARRAY_FARRAY, NULL)
#define check_array(a, npy_type) (!is_array(a) || !require_contiguous(a) || !require_native(a) || array_type(a) != npy_type)
#define array_is_contiguous(a) (PyArray_ISCONTIGUOUS(a))
#define array_is_native(a) (PyArray_ISNOTSWAPPED(a))
#define array_numdims(a) (((PyArrayObject *)a)->nd)
#define is_array(a) ((a) && PyArray_Check((PyArrayObject *)a))

template <typename T>
inline std::string getTypeName();
template <>
inline std::string getTypeName<int>() { return "intc"; };
template <>
inline std::string getTypeName<unsigned char>() { return "uint8"; };
template <>
inline std::string getTypeName<float>() { return "float32"; };
template <>
inline std::string getTypeName<double>() { return "float64"; };

template <typename T>
inline int getTypeNumber();
template <>
inline int getTypeNumber<int>() { return NPY_INT; };
template <>
inline int getTypeNumber<long long int>() { return NPY_INT64; };
template <>
inline int getTypeNumber<unsigned char>() { return NPY_UINT8; };
template <>
inline int getTypeNumber<float>() { return NPY_FLOAT32; };
template <>
inline int getTypeNumber<double>() { return NPY_FLOAT64; };

/* Test whether a python object is contiguous.  If array is
 * contiguous, return 1.  Otherwise, set the python error string and
 * return 0.
 */
int require_contiguous(PyArrayObject *ary)
{
    int contiguous = 1;
    if (!array_is_contiguous(ary))
    {
        PyErr_SetString(PyExc_TypeError,
                        "Array must be contiguous.  A non-contiguous array was given");
        contiguous = 0;
    }
    return contiguous;
}

/* Require that a numpy array is not byte-swapped.  If the array is
 * not byte-swapped, return 1.  Otherwise, set the python error string
 * and return 0.
 */
int require_native(PyArrayObject *ary)
{
    int native = 1;
    if (!array_is_native(ary))
    {
        PyErr_SetString(PyExc_TypeError,
                        "Array must have native byteorder.  "
                        "A byte-swapped array was given");
        native = 0;
    }
    return native;
}

bool isSparseMatrix(PyObject *input)
{
    return PyObject_HasAttrString(input, "indptr");
}

void getTypeObject(PyObject *input, int &T, int &I)
{
    if (isSparseMatrix(input))
    {
        PyArrayObject *data = (PyArrayObject *)PyObject_GetAttrString(input, "data");
        PyArrayObject *indptr = (PyArrayObject *)PyObject_GetAttrString(input, "indptr");
        T = PyArray_TYPE((PyArrayObject *)data);
        I = PyArray_TYPE((PyArrayObject *)indptr);
    }
    else
    {
        T = PyArray_TYPE((PyArrayObject *)input);
        I = getTypeNumber<INTM>();
    }
}

template <typename T, typename I>
static void npyToSpMatrix(PyObject *array, SpMatrix<T, I> &matrix, std::string obj_name)
{
    if (array == NULL)
    {
        throw ConversionError("The array to convert is NULL!");
    }
    PyArrayObject *indptr = (PyArrayObject *)PyObject_GetAttrString(array, "indptr");
    PyArrayObject *indices = (PyArrayObject *)PyObject_GetAttrString(array, "indices");
    PyArrayObject *data = (PyArrayObject *)PyObject_GetAttrString(array, "data");
    PyObject *shape = PyObject_GetAttrString(array, "shape");
    if (check_array(indptr, getTypeNumber<I>()))
    {
        throw ConversionError("spmatrix arg1: indptr array should be 1d int's");
    }
    if (check_array(indices, getTypeNumber<I>()))
    {
        throw ConversionError("spmatrix arg1: indices array should be 1d int's");
    }
    if (check_array(data, getTypeNumber<T>()))
    {
        throw ConversionError("spmatrix arg1: data array should be 1d and match datatype");
    }
    if (!PyTuple_Check(shape))
    {
        throw ConversionError("shape should be a tuple");
    }
    I m = PyLong_AsLong(PyTuple_GetItem(shape, 0));
    I n = PyLong_AsLong(PyTuple_GetItem(shape, 1));
    I *pB = (I *)PyArray_DATA(indptr);
    I *pE = pB + 1;
    I nzmax = (I)PyArray_SIZE(data);
    Py_DECREF(indptr);
    Py_DECREF(indices);
    Py_DECREF(data);
    Py_DECREF(shape);
    matrix.setData((T *)PyArray_DATA(data), (I *)PyArray_DATA(indices), pB, pE, m, n, nzmax);
}

template <typename T>
static void npyToMatrix(PyArrayObject *array, Matrix<T> &matrix, std::string obj_name)
{
    if (array == NULL)
    {
        throw ConversionError("The array to convert is NULL!");
    }
    if (!(PyArray_NDIM(array) == 2 &&
          PyArray_TYPE(array) == getTypeNumber<T>() &&
          (PyArray_FLAGS(array) & NPY_ARRAY_F_CONTIGUOUS)))
    {
        throw ConversionError((obj_name + " matrices should be f-contiguous 2D " + getTypeName<T>() + " array").c_str());
    }
    T *rawX = reinterpret_cast<T *>(PyArray_DATA(array));
    const npy_intp *shape = PyArray_DIMS(array);
    npy_intp m = shape[0];
    npy_intp n = shape[1];
    matrix.setData(rawX, m, n);
}

template <typename T>
static void npyToOptimInfo(PyArrayObject *array, OptimInfo<T> &matrix, std::string obj_name)
{
    if (array == NULL)
    {
        throw ConversionError("The array to convert is NULL!");
    }
    if (!(PyArray_NDIM(array) == 3 &&
          PyArray_TYPE(array) == getTypeNumber<T>() &&
          (PyArray_FLAGS(array) & NPY_ARRAY_F_CONTIGUOUS)))
    {
        throw ConversionError((obj_name + " matrices should be f-contiguous 3D " + getTypeName<T>() + " array").c_str());
    }
    T *rawX = reinterpret_cast<T *>(PyArray_DATA(array));
    const npy_intp *shape = PyArray_DIMS(array);
    npy_intp nclass = shape[0];
    npy_intp m = shape[1];
    npy_intp n = shape[2];
    matrix.setData(rawX, nclass, m, n);
}

template <typename T>
static void npyToVector(PyArrayObject *array, Vector<T> &vector, std::string obj_name)
{
    if (array == NULL)
    {
        throw ConversionError("The array to convert is NULL!");
    }
    T *rawX = reinterpret_cast<T *>(PyArray_DATA(array));
    const npy_intp *shape = PyArray_DIMS(array);
    npy_intp n = shape[0];

    if (!(PyArray_NDIM(array) == 1 &&
          PyArray_TYPE(array) == getTypeNumber<T>() &&
          (PyArray_FLAGS(array) & NPY_ARRAY_ALIGNED)))
    {
        throw ConversionError((obj_name + " should be aligned 1D " + getTypeName<T>() + " array").c_str());
    }
    vector.setData(rawX, n);
}

template <typename T>
inline PyArrayObject *create_np_vector(const int n)
{
    const int nd = 1;
    npy_intp dims[nd] = {n};
    return (PyArrayObject *)PyArray_SimpleNew(nd, dims, getTypeNumber<T>());
}

template <typename T>
inline PyArrayObject *create_np_matrix(const int m, const int n)
{
    const int nd = 2;
    npy_intp dims[nd] = {m, n};
    return (PyArrayObject *)PyArray_SimpleNewF(nd, dims, getTypeNumber<T>());
}

template <typename T>
inline PyArrayObject *create_np_optim_info(const int nclass, const int m, const int n)
{
    const int nd = 3;
    npy_intp dims[nd] = {nclass, m, n};
    return (PyArrayObject *)PyArray_SimpleNewF(nd, dims, getTypeNumber<T>());
}

template <typename T>
inline PyArrayObject *create_np_map(const Vector<int> &size)
{
    int nd = 3;
    int m = size[0];
    int n = size[1];
    int V = size.n() == 2 ? 1 : size[2];
    npy_intp dims[3] = {m, n, V};
    return (PyArrayObject *)PyArray_SimpleNew(nd, dims, getTypeNumber<T>()); // maps are not fortran-arrays
}

#endif
