/** @file extendable-square-matrix.hh
 *  @brief Definition of an extendable square matrix
 *
 *  Description:
 *
 *  An extendable square matrix is a n x n square matrix (where n <= capacity)
 *  which can be extended to the (n+1) x (n+1) matrix (if (n+1) <= capacity)
 *  whenever it is necessary.
 *  
 *  Such a matrix is able to represent a binary relation over a set, a matrix of
 *  counters etc. The data type of the matrix cells is templated.
 *
 *  This file contains an abstract class ExtendableSquareMatrix which does
 *  not contain the exact inner representation of the matrix.
 *
 *  The file also contains several concrete subclasses of the 
 *  ExtendableSquareMatrix class, namely:
 *  - CascadeSquareMatrix
 *  - DynamicSquareMatrix
 *  - HashedSquareMatrix
 *  which implement the inner representation of the matrix on its own. The
 *  data cells will be accessible exclusively through the methods which are
 *  virtual in context of the abstract class ExtendableSquareMatrix and which
 *  are implemented within the subclasses.
 *
 *  Using this class, it is possible to:
 *  - get a value of the matrix cell using two indices in O(1)
 *  - set a value of the matrix cell using two indices in O(1)
 *  - extend a n x n matrix to a (n+1) x (n+1) matrix in O(n)
 *  - implement and use custom inner representation of the matrix
 *  - choose a data type of the matrix cells
 *
 *  @author Tomáš Kocourek
 */

#ifndef EXTENDABLE_SQUARE_MATRIX_HH_
#define EXTENDABLE_SQUARE_MATRIX_HH_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>

namespace mata::utils {

/************************************************************************
*
*
*                        EXTENDABLE SQUARE MATRIX
*
*
*                        (RELATIONS AND COUNTERS)                   
*
*
*************************************************************************/

/**
 * ExtendableSquareMatrix
 * 
 * @brief interface for extendable square matrix implementations
 *
 * Square matrix "n x n" which can be extended to "(n+1) x (n+1)" matrix
 * if n is less than the maximal capacity. Such a class allows us to
 * represent binary relations over carrier set with n elements and adjust
 * it to n+1 elements whenever a new element of the carrier set is created
 * (for example when we represent relation over partition and a block of
 * partition is split in two) or matrices of counters etc.
 * 
 * This abstract class declares methods for accessing elements of the 
 * matrix, assigning to the cells of matrix and extending the matrix by one row
 * and one column (changing the size). 
 * It defines attributes for maximal capacity (which cannot be changed) and
 * current size.
 * It does not define the data structure for storing data. Each subclass
 * which inherits from this abstract class should:
 * - contain the storage for data of datatype T which represents n x n matrix 
 * - implement methods set, get and extend
 * - implement a method clone which creates a deep copy of a matrix
 * Then, the ExtendableSquareMatrix can be used independently of the inner
 * representation of the matrix. Therefore, one can dynamically choose from
 * various of implementations depending on the situation. If any new 
 * subclass is implemented, one should also modify the 'create' 
 * function and extend 'MatrixType' enumerator.
 *
 * Note that in context of an n x n matrix, this implementation uses the word
 * 'size' to refer to the number n (number of rows or columns). The word 
 * 'capacity' corresponds to the maximal allowed size (maximal number
 * of rows or columns). 
 *
**/

using MatrixType = enum MatrixType { None, Cascade, Dynamic, Hashed };

template <typename T>
class ExtendableSquareMatrix {
    protected:
        
        // number of rows (or columns) of the current square matrix
        size_t size_{0};
        
        // maximal allowed number of rows (or columns) of the square matrix        
        size_t capacity_{0};
        
        // type of the matrix which will be chosen as soon as the
        // child class is created
        MatrixType m_type{MatrixType::None};

    public:
        
        //
        //  GETTERS
        //
        
        /** Returns a size of the matrix. In this context,
        * the size of an n x n matrix corresponds to the value n.
        * @brief returns the size of the matrix
        * @return size of the matrix
        */
        size_t size() const { return size_; }
        
        /** Returns a capacity of the matrix. In this context,
        * the capacity of an n x n matrix corresponds to the value n_max such
        * that if the matrix is extended to the n_max x n_max matrix, it cannot
        * be extended anymore.
        * @brief returns the capacity of the matrix
        * @return capacity of the matrix
        */
        size_t capacity() const { return capacity_; }
        
        /** Returns a type of the matrix. The type specifies the
        * way the inner representation of the matrix is implemented.
        * @brief returns the type of the matrix
        * @return type of the matrix
        */
        size_t type() const { return m_type; }
        
        //
        // VIRTUAL METHODS 
        // 
        // These virtual methods will be implemented in the subclasses according
        // to allow to the concrete representation of the matrix. These methods
        // provide a way to access the contents of the matrix
        //
        
        /**
        * @brief Assigns a value to the cell of the matrix
        * @param[in] i row of the matrix 
        * @param[in] j column of the matrix
        * @param[in] value a value which will be assigned to the memory cell
        */        
        virtual void set(size_t i, size_t j, T value = T()) = 0;

        /**
        * @brief Finds a value of the matrix memory cell
        * @param[in] i row of the matrix 
        * @param[in] j column of the matrix
        * @return a found value in the matrix
        */
        virtual T get(size_t i, size_t j) const = 0;
        
        /**
        * @brief changes the n x n matrix to the (n+1) x (n+1) matrix 
        * @param[in] placeholder value which will be assigned to the
        * newly allocated memory cells 
        */
        virtual void extend(T placeholder = T()) = 0;

        /**
        * Changes the n x n matrix to the (n+1) x (n+1) matrix by duplicating
        * existing row and column. The row parameter is an index of the row
        * which should be duplicated and added as a (n+1)th row, while the
        * col parameter is an index of the column which should be duplicated
        * and added as an (n+1)th row. If the row parameter equals n, then
        * it will be initialized using default values of the type T. If the
        * col parameter equals n, the new column will be also initialized
        * with the default values of the type T. Using this approach, one is
        * able to copy only a row or column and initialize the other one with
        * default values. Calling extend_and_copy(n, n) has the same effect as
        * calling extend(). 
        * The element at the position [n, n] will be always initialized using
        * the default value of the type T. 
        * @brief changes the n x n matrix to the (n+1) x (n+1) matrix with
        * copying of the existing data 
        * @param[in] placeholde value which will be assigned to the
        * newly allocated memory cells 
        */        
        virtual void extend_and_copy(size_t row, size_t col) = 0;
        
        // 
        //  CLONING
        //
        
        /**
        * @brief creates a deep copy of the matrix 
        * @return deep copy of the matrix
        */
        virtual std::unique_ptr<ExtendableSquareMatrix<T>> clone() const = 0;
        
        virtual ~ExtendableSquareMatrix() = default;
        
        //
        //  MATRIX PROPERTIES
        //
        
        /** This function checks whether the matrix is reflexive. In this
        * context, the matrix is reflexive iff none of the elements on the main
        * diagonal is the zero element of the type T
        * @brief checks whether the Extendable square matrix is reflexive
        * @return true iff the matrix is reflexive
        */
        bool is_reflexive() const {
            size_t size = this->size();
            for(size_t i = 0; i < size; ++i) {
                if(!get(i, i)) { return false; }
            }
            return true;
        }

        /** This function checks whether the matrix is antisymmetric. In this
        * context, the matrix is antisymmetric iff there are no indices i, j
        * where i != j and both matrix[i][j], matrix[j][i] contain nonzero 
        * elements of the type T
        * @brief checks whether the Extendable square matrix is antisymmetric
        * @return true iff the matrix is antisymmetric
        */
        bool is_antisymmetric() const {
            size_t size = this->size();
            for(size_t i = 0; i < size; ++i) {
                for(size_t j = 0; j < size; ++j) {
                    if(i == j) [[unlikely]] { continue; }
                    if(get(i, j) && get(j, i)) { return false; }
                }
            }
            return true;
        }

        /** This function checks whether the matrix is transitive. In this
        * context, the matrix is transitive iff it holds that the input matrix
        * casted to the matrix of booleans (false for zero values of type T, 
        * otherwise true) remains the same if it is multiplied by itself.
        * @brief checks whether the Extendable square matrix is transitive
        * @return true iff the matrix is transitive
        */
        bool is_transitive() const {
            size_t size = this->size();
            for(size_t i = 0; i < size; ++i) {
                for(size_t j = 0; j < size; ++j) {
                    bool found = false;
                    for(size_t k = 0; k < size; ++k) {
                        if(get(i, k) && get(k, j)) { 
                            found = true;
                            break; 
                        }
                    }
                    if(!found == static_cast<bool>(get(i, j))) { return false; }
                }
            }
            return true;
        }
        
                
}; // ExtendableSquareMatrix



/*************************************
*
*        CASCADE SQUARE MATRIX
*
**************************************/

/**
 * CascadeSquareMatrix
 * 
 * @brief Linearized square matrix implemented using single vector of
 * elements which stores data in some kind of "cascading" way
 *
 * This implementation tries to avoid
 * - moving the whole matrix when it is extended
 * - allocation of unnecessary data cells
 * - violation of data locality
 *
 * The matrix is represented as a single vector of a type T. Initially,
 * the maximal possible capacity is given to the constructor. It reserves
 * 'capacity * capacity' data cells for the vector (in the constant time O(1))
 * without need to allocate anything.
 * When the matrix is extended, additional (size * 2) + 1 elements of the
 * vector are allocated. The matrix is traversed in some kind of "cascading" way
 * as follows:
 *
 * Each number in the matrix corresponds to the order of accessing that element
 * using this "cascading traversal".
 *
 *   MATRIX:
 *   -----------------
 *   | 0 | 3 | 8 | 15|
 *   -----------------
 *   | 1 | 2 | 7 | 14|
 *   -----------------
 *   | 4 | 5 | 6 | 13|
 *   -----------------
 *   | 9 | 10| 11| 12|
 *   -----------------
 *
 *   VECTOR:
 *   -----------------------------------------------------------------------
 *   | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
 *   -----------------------------------------------------------------------
 *
 * The data cell matrix[i][j] could be accessed using the formula
 * vector[i >= j ? i * i + j : j * j + 2 * j - i].
 *   
 * Using this approach, there is no need to allocate unnecessary data cells
 * when extending n x n matrix to the (n+1) x (n+1) matrix (in contrast with
 * using row or column traversal).
 * Since 'capacity * capacity' data cells were reserved, the vector won't ever
 * be moved in the memory due to the extending. However, it requieres to
 * reserve a lot of memory space when the capacity is a huge number.
 * The data locality won't be violated since all of the elements will be stored
 * as a contiguous vector.
 *
**/
template <typename T>
class CascadeSquareMatrix : public ExtendableSquareMatrix<T> {
    private:
 
        // data are stored in a single vector
        std::vector<T> data_{};
    
    public:
    
        //
        //  CONSTRUCTORS
        //
        
        /**
        * @brief creates a Cascade square matrix
        * @param[in] max_rows capacity of the square matrix
        * @param[in] init_rows initial size of the square matrix
        */
        CascadeSquareMatrix(size_t max_rows, size_t init_rows) {
            assert(init_rows <= max_rows && 
                   "Initial size of the matrix cannot be"
                   "bigger than the capacity");
            
            this->m_type = MatrixType::Cascade;
            this->capacity_ = max_rows;
            data_.reserve(this->capacity_ * this->capacity_);
            
            // creating the initial size and filling the data cells with
            // default values
            for(size_t i = 0; i < init_rows; ++i) {extend();}
        }
        
        /** This method provides a way to create a copy of a given
        * CascadeSquareMatrix and preserves the reserved capacity of the vector
        * 'data_'. This goal is achieved using the custom assignment operator.
        * @brief copy constructor of a CascadeSquareMatrix
        * @param other matrix which should be copied
        */
        CascadeSquareMatrix(const CascadeSquareMatrix<T>& other) {
            *this = other;
        }
        
        // 
        //  IMPLEMENTED VIRTUAL METHODS
        //
        
        /**
        * @brief Assigns a value to the Cascade square matrix.
        * @param[in] i row of the square matrix
        * @param[in] j column of the square matrix
        * @param[in] value value to be assigned to the square matrix data cell
        */
        void set(size_t i, size_t j, T value) {
            assert(i < this->size_ && "Nonexistent row cannot be accessed");
            assert(j < this->size_ && "Nonexistent column cannot be accessed");
            
            // accessing the matrix in the cascading way
            data_[i >= j ? i * i + j : j * j + 2 * j - i] = value;
        }

        /**
        * @brief returns a value of the Cascade square matrix
        * @param[in] i row of the square matrix
        * @param[in] j column of the square matrix
        * @return value found in the square matrix data cell
        */
        T get(size_t i, size_t j) const {
            assert(i < this->size_ && "Nonexistent row cannot be accessed");
            assert(j < this->size_ && "Nonexistent column cannot be accessed");

            // accessing the matrix in the cascading way
            return data_[i >= j ? i * i + j : j * j + 2 * j - i];
        }

        /**
        * @brief extends the Cascade square matrix by a new row and column
        * @param[in] placeholder a value which will be assigned to all the new 
        * data cells (optional)
        */
        void extend(T placeholder = T()) {
            assert(this->size_ < this->capacity_ 
                   && "The matrix cannot be extended anymore");

            // allocation of 2 * size + 1 new data cells
            data_.insert(data_.end(), 2 * this->size_ + 1, placeholder);
            
            // the size increases
            ++this->size_;
        }

        /**
        * Extends the n x n cascade matrix to the (n+1) x (n+1) matrix by
        * duplicating an existing row and column. The row parameter is an index
        * of the row which should be duplicated and added as a (n+1)th row,
        * while the col parameter is an index of the column which should be
        * duplicated and added as an (n+1)th row. If the row parameter equals n,
        * then it will be initialized using default values of the type T. If the
        * col parameter equals n, the new column will be also initialized
        * with the defaults values of the type T. Using this approach, one is
        * able to copy only a row or column and initialize the other one with
        * default values. Calling extend_and_copy(n, n) has the same effect as
        * calling extend().
        * The element at the position [n, n] will be always initialized using
        * the default value of the type T. 
        * @brief changes the n x n matrix to the (n+1) x (n+1) matrix with
        * copying of the existing data 
        * @param[in] row index of the row which should be copied
        * @param[in] col index of the column which should be copied 
        */ 
       void extend_and_copy(size_t row, size_t col) {
            assert(this->size_ < this->capacity_ 
                   && "The matrix cannot be extended anymore");
                   std::cout << this->size_ << " " << row << std::endl;
            assert(row <= this->size_
                   && "Index of the copied row cannot be bigger than the size");
            assert(col <= this->size_
                   && "Index of the copied col cannot be bigger than the size");
                   
            // if the row index corresponds to the index of the newly created
            // row, it will be initialized using default values of the type T
            if(row == this->size_) {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_.push_back(T());
                }
            // otherwise, the row with the index 'row' will be duplicated
            // to create a new row
            } else {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_.push_back(get(row, i));
                }
            }
            
            // element at the position [n, n] will always be initialized using
            // the default value of the type T
            data_.push_back(T());
            
            // if the column index corresponds to the index of the newly created
            // column, it will be initialized using default values of the type T
            if(col == this->size_) {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_.push_back(T());
                }            
            // otherwise, the column with the index 'col' will be duplicated
            // to create a new column
            } else {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_.push_back(get(this->size_ - 1 - i, col));
                }
            }
            
            // the size of the matrix increases after extending
            ++this->size_;
        }
        
        //
        //  CLONING
        //
        
        std::unique_ptr<ExtendableSquareMatrix<T>> clone() const override {
            return std::make_unique<CascadeSquareMatrix<T>>(*this); 
        }
            
        //
        //  OPERATORS
        //
        
        /** This method provides a way to assign a CascadeSquareMatrix 
        * to the variable. The method ensures us to keep the reserved 
        * capacity of the vector 'data_' since the default vector
        * assignment does not preserve it.
        * @brief assignment operator for the CascadeSquareMatrix class
        * @param[in] other matrix which should be assigned
        */
        CascadeSquareMatrix<T>& operator=(const CascadeSquareMatrix<T>& other) {
            // initialization of the matrix
            this->capacity_ = other.capacity();
            this->size_ = 0;
            this->data_ = std::vector<T>();
            this->data_.reserve(this->capacity_ * this->capacity_);
            size_t other_size = other.size();
            for(size_t i = 0; i < other_size; ++i) {this->extend();}
            
            // copying memory cells
            for(size_t i = 0; i < this->size_; ++i) {
                for(size_t j = 0; j < this->size_; ++j) {
                    this->set(i, j, other.get(i, j));
                }
            }
            return *this;
        }
        
}; // CascadeSquareMatrix

/*************************************
*
*        DYNAMIC SQUARE MATRIX
*
**************************************/

/**
 * DynamicSquareMatrix
 * 
 * @brief Dynamic square matrix implemented using vector of vectors
 * of the type T
 *
 * This implementation tries to avoid
 * - allocation or reservation of data cells which won't ever be used
 *
 * The matrix is represented as a vector of vectors of the type T. It is
 * extended dynamically without any need to allocate or reserve any unnecessary
 * memory space before it is used.
 * However, the data locality is not ensured. Moreover, if the matrix is
 * extended, it could possibly be moved in the memory.
**/
template <typename T>
class DynamicSquareMatrix : public ExtendableSquareMatrix<T> {
    private:
 
        // data are stored in a vector of vectors
        std::vector<std::vector<T>> data_{};
    
    public:
    
        //
        //  CONSTRUCTORS
        //
        
        /**
        * @brief creates a Dynamic square matrix
        * @param[in] max_rows capacity of the square matrix
        * @param[in] init_rows initial size of the square matrix
        */
        DynamicSquareMatrix(size_t max_rows, size_t init_rows) {
            assert(init_rows <= max_rows && 
                   "Initial size of the matrix cannot"
                   "be bigger than the capacity");
            
            this->m_type = MatrixType::Dynamic;
            this->capacity_ = max_rows;
            
            // creating the initial size and filling the data cells with
            // default values
            for(size_t i = 0; i < init_rows; ++i) {extend();}
        }
        
        //
        //  IMPLEMENTED VIRTUAL METHODS
        //

        /**
        * @brief Assigns a value to the Dynamic square matrix.
        * @param[in] i row of the square matrix
        * @param[in] j column of the square matrix
        * @param[in] value value to be assigned to the square matrix data cell
        */
        T get(size_t i, size_t j) const {
            assert(i < this->size_ && "Nonexistent row cannot be accessed");
            assert(j < this->size_ && "Nonexistent column cannot be accessed");
            
            return data_[i][j];
        }

        /**
        * @brief returns a value of the Dynamic square matrix
        * @param[in] i row of the square matrix
        * @param[in] j column of the square matrix
        * @return value found in the square matrix data cell
        */
        void set(size_t i, size_t j, T value) {
            assert(i < this->size_ && "Nonexistent row cannot be accessed");
            assert(j < this->size_ && "Nonexistent column cannot be accessed");
            
            data_[i][j] = value;
        }

        /**
        * @brief extends the Dynamic square matrix by a new row and column
        * @param[in] placeholder a value which will be assigned to all the 
        * new data cells
        */
        void extend(T placeholder = T()) {
            assert(this->size_ < this->capacity_ 
                   && "The matrix cannot be extended anymore");
            
            // creating a new column      
            for(size_t i = 0; i < this->size_; ++i) {
                data_[i].push_back(placeholder);
            }
            
            // creating a new row
            data_.emplace_back();
            ++this->size_;
            data_.back().insert(data_.back().end(), this->size_, placeholder);
        }
        
        /**
        * Extends the n x n dynamic matrix to the (n+1) x (n+1) matrix by
        * duplicating an existing row and column. The row parameter is an index
        * of the row which should be duplicated and added as a (n+1)th row,
        * while the col parameter is an index of the column which should be
        * duplicated and added as an (n+1)th row. If the row parameter equals n,
        * then it will be initialized using default values of the type T. If the
        * col parameter equals n, the new column will be also initialized
        * with the defaults values of the type T. Using this approach, one is
        * able to copy only a row or column and initialize the other one with
        * default values. Calling extend_and_copy(n, n) has the same effect as
        * calling extend().
        * The element at the position [n, n] will be always initialized using
        * the default value of the type T. 
        * @brief changes the n x n matrix to the (n+1) x (n+1) matrix with
        * copying of the existing data 
        * @param[in] row index of the row which should be copied
        * @param[in] col index of the column which should be copied
        */         
        void extend_and_copy(size_t row, size_t col) {
            assert(this->size_ < this->capacity_
                   && "The matrix cannot be extended anymore");
            assert(row <= this->size_
                   && "Index of the copied row cannot be bigger than the size");
            assert(col <= this->size_
                   && "Index of the copied col cannot be bigger than the size");

            // if the row index corresponds to the index of the newly created
            // row, it will be initialized using default values of the type T            
            if(row == this->size_) {
                data_.emplace_back();
                for(size_t i = 0; i < this->size_; ++i) {
                    data_[this->size_].emplace_back(T());
                }
            // otherwise, the row at the index 'row' will be duplicated
            } else {
                data_.push_back(data_[row]);
            }
            
            // if the column index corresponds to the index of the newly created
            // column, it will be initialized using default values of the type T
            if(col == this->size_) {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_[i].emplace_back(T());
                }
            // otherwise, the column at the index 'col' will be duplicated
            } else {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_[i].push_back(data_[i][col]);
                }
            }
            
            // element at the position [n, n] will always be initialized using
            // the default value of the type T
            data_[this->size_].emplace_back(T());
            
            // the size of the matrix increases after extending
            ++this->size_;
        }
        
        //
        //  CLONING
        //
        
        std::unique_ptr<ExtendableSquareMatrix<T>> clone() const override {
            return std::make_unique<DynamicSquareMatrix<T>>(*this); 
        }

}; // DynamicSquareMatrix



/*************************************
*
*        HASHED SQUARE MATRIX
*
**************************************/

/**
 * HashedSquareMatrix
 * 
 * @brief Hashed square matrix implemented using unordered hash map
 *
 * The matrix is represented as a unordered hash map of the type T indexed
 * by the size_t type. It is referred as in context of row-traversal of the
 * matrix. To access matrix[i][j], we use map[i * capacity + j].
**/
template <typename T>
class HashedSquareMatrix : public ExtendableSquareMatrix<T> {
    private:

        // data are stored in a hashmap
        mutable std::unordered_map<size_t, T> data_{};
    
    public:
    
        //
        //  CONSTRUCTORS
        //
        
        /**
        * @brief creates a Hashed square matrix
        * @param[in] max_rows capacity of the square matrix
        * @param[in] init_rows initial size of the square matrix
        */
        HashedSquareMatrix(size_t max_rows, size_t init_rows) {
            assert(init_rows <= max_rows && 
                   "Initial size of the matrix cannot be"
                   "bigger than the capacity");
            
            this->m_type = MatrixType::Hashed;
            this->capacity_ = max_rows;
            
            // creating the initial size and filling the data cells with
            // default values
            for(size_t i = 0; i < init_rows; ++i) {extend();}
        }
        
        // 
        //  IMPLEMENTED VIRTUAL METHODS
        //       

        /**
        * @brief Assigns a value to the Hashed square matrix.
        * @param[in] i row of the square matrix
        * @param[in] j column of the square matrix
        * @param[in] value value to be assigned to the square matrix data cell
        */
        void set(size_t i, size_t j, T value) {
            assert(i < this->size_ && "Nonexistent row cannot be accessed");
            assert(j < this->size_ && "Nonexistent column cannot be accessed");
            
            // accessing the hashmap using row matrix traversal
            data_[i * this->capacity_ + j] = value;
        }

        /**
        * @brief returns a value of the Hashed square matrix
        * @param[in] i row of the square matrix
        * @param[in] j column of the square matrix
        * @return value found in the square matrix data cell
        */
        T get(size_t i, size_t j) const {
            assert(i < this->size_ && "Nonexistent row cannot be accessed");
            assert(j < this->size_ && "Nonexistent column cannot be accessed");
            
            // accessing the hashmap using row matrix traversal
            return data_[i * this->capacity_ + j];
        }

        /**
        * @brief extends the Hashed square matrix by a new row and column
        * @param[in] placeholder a value which will be assigned to all the 
        * new data cells
        */
        void extend(T placeholder = T()) {
            assert(this->size_ < this->capacity_ 
                   && "Matrix cannot be extended anymore");

            // creating a new row and column
            for(size_t i = 0; i < this->size_; ++i) {
                data_[this->size_ * this->capacity_ + i] = placeholder;
                data_[i * this->capacity_ + this->size_] = placeholder;
            }
            data_[this->size_ * this->capacity_ + this->size_] = placeholder;
            
            // increasing size
            ++this->size_;
        }

        /**
        * Extends the n x n hashed matrix to the (n+1) x (n+1) matrix by
        * duplicating an existing row and column. The row parameter is an index
        * of the row which should be duplicated and added as a (n+1)th row,
        * while the col parameter is an index of the column which should be
        * duplicated and added as an (n+1)th row. If the row parameter equals n,
        * then it will be initialized using default values of the type T. If the
        * col parameter equals n, the new column will be also initialized
        * with the defaults values of the type T. Using this approach, one is
        * able to copy only a row or column and initialize the other one with
        * default values. Calling extend_and_copy(n, n) has the same effect as
        * calling extend().
        * The element at the position [n, n] will be always initialized using
        * the default value of the type T. 
        * @brief changes the n x n matrix to the (n+1) x (n+1) matrix with
        * copying of the existing data 
        * @param[in] row index of the row which should be copied
        * @param[in] col index of the column which should be copied
        */
        void extend_and_copy(size_t row, size_t col) {
            assert(this->size_ < this->capacity_
                   && "The matrix cannot be extended anymore");
            assert(row <= this->size_
                   && "Index of the copied row cannot be bigger than the size");
            assert(col <= this->size_
                   && "Index of the copied col cannot be bigger than the size");

            // if the row index corresponds to the index of the newly created
            // row, it will be initialized using default values of the type T
            if(row == this->size_) {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_[this->size_ * this->capacity_ + i] = T();
                }
            // otherwise, the row at the index 'row' will be duplicated
            } else {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_[this->size_ * this->capacity_ + i] = 
                        data_[row * this->capacity_ + i];
                }
            }

            // if the col index corresponds to the index of the newly created
            // column, it will be initialized using default values of the type T
            if(col == this->size_) {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_[i * this->capacity_ + this->size_] = T();
                }
            // otherwise, the column at the index 'col' will be duplicated
            } else {
                for(size_t i = 0; i < this->size_; ++i) {
                    data_[i * this->capacity_ + this->size_] =
                        data_[i * this->capacity_ + col];
                }
            }

            // element at the position [n, n] will always be initialized using
            // the default value of the type T
            data_[this->size_ * this->capacity_ + this->size_] = T();
            
            // the size of the matrix increases after extending
            ++this->size_;
        }

        // 
        //  CLONING
        //
        
        std::unique_ptr<ExtendableSquareMatrix<T>> clone(void) const override { 
            return std::make_unique<HashedSquareMatrix<T>>(*this); 
        }
        
}; // HashedSquareMatrix

/*************************************
*
*        ADDITIONAL FUNCTIONS
*
**************************************/

/**
* @brief factory function which creates an ExtendableSquareMatrix of given type
* @param[in] type type of the new matrix
* @param[in] capacity maximal matrix capacity
* @param[in] size initial matrix size
* @return pointer to the newly created matrix
*/
template <typename T>
inline std::unique_ptr<ExtendableSquareMatrix<T>> create(MatrixType type, 
    size_t capacity, size_t size = 0) {
    
    switch(type) {
        case MatrixType::Cascade:
            return std::make_unique<CascadeSquareMatrix<T>>(capacity, size);
        case MatrixType::Dynamic:
            return std::make_unique<DynamicSquareMatrix<T>>(capacity, size);    
        case MatrixType::Hashed:
            return std::make_unique<HashedSquareMatrix<T>>(capacity, size);    
        default:
            return nullptr;
    }
}

/**
* @brief debugging function which allows us to print text representation of
* the Extendable square matrix
* @param[out] output stream
* @param[in] matrix which will be printed
* @return output stream
*/
template <typename T>
inline std::ostream& operator<<(std::ostream& os, 
    const ExtendableSquareMatrix<T>& matrix) {
    
    size_t size = matrix.size();
    size_t capacity = matrix.capacity();    
    std::string result = "\nSIZE: " + std::to_string(size) + "\n";
    result += "CAPACITY: " + std::to_string(capacity) + "\n";
    result += "MATRIX:\n";
    for(size_t i = 0; i < size; ++i) {
        for(size_t j = 0; j < size; ++j) {
            result += std::to_string(
                static_cast<unsigned long>(matrix.get(i, j))) + " ";
        }
        result += "\n";
    }
    return os << result;
}

} // namespace mata::utils

#endif // EXTENDABLE_SQUARE_MATRIX_HH_
