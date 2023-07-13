import tabulate

from cython.operator import dereference

from libmata.utils cimport CBinaryRelation

cdef class BinaryRelation:
    """Wrapper for binary relation."""

    def __cinit__(self, size_t size=0, bool defVal=False, size_t rowSize=16):
        self.thisptr = new CBinaryRelation(size, defVal, rowSize)

    def __dealloc__(self):
        if self.thisptr != NULL:
            del self.thisptr

    def size(self):
        """Returns the size of the relation

        :return: size of the relation
        """
        return self.thisptr.size()

    def resize(self, size_t size, bool defValue=False):
        """Resizes the binary relation to size

        :param size_t size: new size of the binary relation
        :param bool defValue: default value that is set after resize
        """
        self.thisptr.resize(size, defValue)

    def get(self, size_t row, size_t col):
        """Gets the value of the relation at [row, col]

        :param size_t row: row of the relation
        :param size_t col: col of the relation
        :return: value of the binary relation at [row, col]
        """
        return self.thisptr.get(row, col)

    def set(self, size_t row, size_t col, bool value):
        """Sets the value of the relation at [row, col]

        :param size_t row: row of the relation
        :param size_t col: col of the relation
        :param bool value: value that is set
        """
        self.thisptr.set(row, col, value)

    def to_matrix(self):
        """Converts the relation to list of lists of booleans

        :return: relation of list of lists to booleans
        """
        size = self.size()
        result = []
        for i in range(0, size):
            sub_result = []
            for j in range(0, size):
                sub_result.append(self.get(i, j))
            result.append(sub_result)
        return result

    def reset(self, bool defValue = False):
        """Resets the relation to defValue

        :param bool defValue: value to which the relation will be reset
        """
        self.thisptr.reset(defValue)

    def split(self, size_t at, bool reflexive=True):
        """Creates new row corresponding to the row/col at given index (i think)

        :param size_t at: where the splitting will commence
        :param bool reflexive: whether the relation should stay reflexive
        """
        self.thisptr.split(at, reflexive)

    def alloc(self):
        """Increases the size of the relation by one

        :return: previsous size of the relation
        """
        return self.thisptr.alloc()

    def is_symmetric_at(self, size_t row, size_t col):
        """Checks if the relation is symmetric at [row, col] and [col, row]

        :param size_t row: checked row
        :param size_t col: checked col
        :return: true if [row, col] and [col, row] are symmetric
        """
        return self.thisptr.sym(row, col)

    def restrict_to_symmetric(self):
        """Restricts the relation to its symmetric fragment"""
        self.thisptr.restrict_to_symmetric()

    def build_equivalence_classes(self):
        """Builds equivalence classes w.r.t relation

        :return: mapping of state to its equivalence class,
            first states corresponding to a equivalence class?
        """
        cdef vector[size_t] index
        cdef vector[size_t] head
        self.thisptr.build_equivalence_classes(index, head)
        return index, head

    def build_index(self):
        """Builds index mapping states to their images

        :return: index mapping states to their images, i.e. x -> {y | xRy}
        """
        cdef vector[vector[size_t]] index
        self.thisptr.build_index(index)
        return [[v for v in i] for i in index]

    def build_inverse_index(self):
        """Builds index mapping states to their co-images

        :return: index mapping states to their co-images, i.e. x -> {y | yRx}
        """
        cdef vector[vector[size_t]] index
        self.thisptr.build_inv_index(index)
        return [[v for v in i] for i in index]

    def build_indexes(self):
        """Builds index mapping states to their images/co-images

        :return: index mapping states to their images/co-images, i.e. x -> {y | yRx}
        """
        cdef vector[vector[size_t]] index
        cdef vector[vector[size_t]] inv_index
        self.thisptr.build_index(index, inv_index)
        return [[v for v in i] for i in index], [[v for v in i] for i in inv_index]

    def transpose(self):
        """Transpose the relation

        :return: transposed relation
        """
        result = BinaryRelation()
        self.thisptr.transposed(dereference(result.thisptr))
        return result

    def get_quotient_projection(self):
        """Gets quotient projection of the relation

        :return: quotient projection
        """
        cdef vector[size_t] projection
        self.thisptr.get_quotient_projection(projection)
        return projection

    def __str__(self):
        return str(tabulate.tabulate(self.to_matrix()))
