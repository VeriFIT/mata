from typing import Self

class BinaryRelation:
    """Wrapper for binary relation."""
    def __init__(self, size: int = 0, defVal: bool = False, rowSize: int = 16) -> None:
        ...
    def size(self) -> int:
        """Returns the size of the relation

        :return: size of the relation
        """
        ...
    def resize(self, size: int, defValue: bool = False) -> None:
        """Resizes the binary relation to size

        :param size_t size: new size of the binary relation
        :param bool defValue: default value that is set after resize
        """
        ...
    def get(self, row: int, col: int) -> bool:
        """Gets the value of the relation at [row, col]

        :param size_t row: row of the relation
        :param size_t col: col of the relation
        :return: value of the binary relation at [row, col]
        """
        ...
    def set(self, row: int, col: int, value: bool) -> None:
        """Sets the value of the relation at [row, col]

        :param size_t row: row of the relation
        :param size_t col: col of the relation
        :param bool value: value that is set
        """
        ...
    def to_matrix(self) -> list[list[bool]]:
        """Converts the relation to list of lists of booleans

        :return: relation of list of lists to booleans
        """
        ...
    def reset(self, defValue: bool = False) -> None:
       """Resets the relation to defValue

       :param bool defValue: value to which the relation will be reset
       """
       ...
    def split(self, at: int, reflexive: bool = True) -> None:
        """Creates new row corresponding to the row/col at given index (i think)

        :param size_t at: where the splitting will commence
        :param bool reflexive: whether the relation should stay reflexive
        """
        ...
    def alloc(self) -> int:
        """Increases the size of the relation by one

        :return: previsous size of the relation
        """
        ...
    def is_symmetric_at(self, row: int, col: int) -> bool:
        """Checks if the relation is symmetric at [row, col] and [col, row]

        :param size_t row: checked row
        :param size_t col: checked col
        :return: true if [row, col] and [col, row] are symmetric
        """
        ...
    def restrict_to_symmetric(self) -> None:
        """Restricts the relation to its symmetric fragment"""
        ...
    def build_equivalence_classes(self) -> tuple[list[int], list[int]]:
        """Builds equivalence classes w.r.t relation

        :return: mapping of state to its equivalence class,
            first states corresponding to a equivalence class?
        """
        ...
    def build_index(self) -> list[list[int]]:
        """Builds index mapping states to their images

        :return: index mapping states to their images, i.e. x -> {y | xRy}
        """
        ...
    def build_inverse_index(self) -> list[list[int]]:
        """Builds index mapping states to their co-images

        :return: index mapping states to their co-images, i.e. x -> {y | yRx}
        """
        ...
    def build_indexes(self) -> tuple[list[list[int]], list[list[int]]]:
        """Builds index mapping states to their images/co-images

        :return: index mapping states to their images/co-images, i.e. x -> {y | yRx}
        """
        ...
    def transpose(self) -> Self:
        """Transpose the relation

        :return: transposed relation
        """
        ...
    def get_quotient_projection(self) -> list[int]:
        """Gets quotient projection of the relation

        :return: quotient projection
        """
        ...
    def __str__(self) -> str:
        ...
