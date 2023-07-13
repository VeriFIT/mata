from libc.stdint cimport uint8_t
from libcpp cimport bool
from libcpp.vector cimport vector

cdef extern from "mata/sparse-set.hh" namespace "Mata::Util":
    cdef cppclass CSparseSet "Mata::Util::SparseSet" [T]:
        vector[T] dense
        vector[T] sparse
        size_t size
        size_t domain_size

        CSparseSet()
        CSparseSet(T)
        CSparseSet(vector[T])

        size_t domain_size()
        bool empty()
        void clear()
        void reserve(size_t)
        bool contains(T)
        void insert(T)
        void erase(T)
        bool consistent()
        bool operator[](T)

        cppclass const_iterator:
            const T operator *()
            const_iterator operator++()
            bint operator ==(const_iterator)
            bint operator !=(const_iterator)
        const_iterator cbegin()
        const_iterator cend()

        cppclass iterator:
            T operator *()
            iterator operator++()
            bint operator ==(iterator)
            bint operator !=(iterator)
        iterator begin()
        iterator end()


cdef extern from "mata/ord-vector.hh" namespace "Mata::Util":
    cdef cppclass COrdVector "Mata::Util::OrdVector" [T]:
        COrdVector() except+
        COrdVector(vector[T]) except+
        vector[T] ToVector()

        cppclass const_iterator:
            const T operator *()
            const_iterator operator++()
            bint operator ==(const_iterator)
            bint operator !=(const_iterator)
        const_iterator cbegin()
        const_iterator cend()

        cppclass iterator:
            T operator *()
            iterator operator++()
            bint operator ==(iterator)
            bint operator !=(iterator)
        iterator begin()
        iterator end()


cdef extern from "mata/util.hh" namespace "Mata":
    cdef cppclass CBoolVector "Mata::BoolVector":
        CBoolVector()
        CBoolVector(size_t, bool)
        CBoolVector(vector[uint8_t])

        size_t count()

