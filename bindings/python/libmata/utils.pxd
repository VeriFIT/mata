from libc.stdint cimport uint8_t
from libcpp cimport bool
from libcpp.vector cimport vector

cdef extern from "mata/utils/sparse-set.hh" namespace "mata::utils":
    cdef cppclass CSparseSet "mata::utils::SparseSet" [T]:
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


cdef extern from "mata/utils/ord-vector.hh" namespace "mata::utils":
    cdef cppclass COrdVector "mata::utils::OrdVector" [T]:
        COrdVector() except+
        COrdVector(vector[T]) except+
        vector[T] ToVector()
        size_t size()

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


cdef extern from "mata/utils/utils.hh" namespace "mata":
    cdef cppclass CBoolVector "mata::BoolVector":
        CBoolVector()
        CBoolVector(size_t, bool)
        CBoolVector(vector[uint8_t])

        size_t count()


cdef extern from "mata/simlib/util/binary_relation.hh" namespace "Simlib::Util":
    ctypedef vector[size_t] ivector
    ctypedef vector[bool] bvector
    ctypedef vector[ivector] IndexType

    cdef cppclass CBinaryRelation "Simlib::Util::BinaryRelation":
        CBinaryRelation()
        CBinaryRelation(size_t, bool, size_t)
        CBinaryRelation(vector[bvector])

        void reset(bool)
        void resize(size_t, bool)
        size_t alloc()
        size_t split(size_t, bool)
        bool get(size_t, size_t)
        bool set(size_t, size_t, bool)
        size_t size()
        bool sym(size_t, size_t)
        void build_equivalence_classes(ivector&)
        void build_equivalence_classes(ivector&, ivector&)
        CBinaryRelation transposed(CBinaryRelation&)
        void build_index(IndexType&)
        void build_inv_index(IndexType&)
        void build_index(IndexType&, IndexType&)
        void restrict_to_symmetric()
        void get_quotient_projection(ivector&)

# Forward declarations
cdef class BinaryRelation:
    """Wrapper for binary relation."""
    cdef CBinaryRelation *thisptr
