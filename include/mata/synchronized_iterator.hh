//
// Created by Lukáš Holík on 29.10.2022.
//

#ifndef LIBMATA_SYNCHRONIZED_ITERATOR_HH
#define LIBMATA_SYNCHRONIZED_ITERATOR_HH

#include <mata/ord_vector.hh>

namespace Mata {
    namespace Util {

        // Classes that provide "synchronized" iterators through a vector of ordered vectors,
        // needed in computation of post
        // in subset construction, product, and non-determinization.
        // Key is the type stored in OrdVectors, it must be comparable with <,>,==,!=,<=,>=,
        // and it must be a total (linear) ordering.
        // The intended usage in, for instance, determinisation is for Key to be Move.
        // Move is ordered by the symbol.
        //
        // SyncrhonisedIterator is the parent virtual class.
        // It stores a vector of end-iterators for the OrdVectors and a vector of current positions.
        // They are filled in using the function push_back(v), that adds v.begin() to positions and v.end() to ends.
        // Method advance advances all positions forward so that they are synchronized on the next smallest equiv class
        // (next smallest symbol in the case of Move).
        //
        // There are two versions of the class.
        // i) In product, ALL positions must point to currently the smallest equiv. class (Moves with the same symbol).
        // Method get_current then returns the vector of all position iterators, synchronized.
        // In determinization, it is enough that there EXISTS a position that points to the smallest class.
        // Method get_current then returns the vector of only those positions that point to the smallest equiv. class.
        //
        // Usage: 0) construct, 1) fill in using push_back, iterate using advance and get_current, 2) reset, goto 1)

        template<typename Key> class SynchronizedIterator {
        public:
            using Iterator = typename OrdVector<Key>::const_iterator;

            std::vector<Iterator> positions;
            std::vector<Iterator> ends;

            explicit SynchronizedIterator(const int size = 0)
            {
                positions.reserve(size);
                ends.reserve(size);
            };

            // This is supposed to be called only before an iteration,
            // after constructor of reset.
            // Calling after advance breaks the iterator.
            virtual void push_back (const OrdVector<Key> &vector) {
                // hope that the parameter is not deep copied ... is it?
                this->positions.push_back(vector.begin());
                this->ends.push_back(vector.end());
            };

            // Empties positions and ends.
            // Though they should keep the allocated space.
            void reset(const int size = 0) {
                positions.clear();
                ends.clear();
                if (size > 0) {
                    positions.reserve(size);
                    ends.reserve(size);
                }
            };

            virtual bool advance() = 0;
            virtual const std::vector<Iterator> get_current() = 0;
        };



        template<typename Key>
        class SynchronizedUniverzalIterator: public SynchronizedIterator<Key> {
        public:

            using Iterator = typename OrdVector<Key>::const_iterator;

            // "minimum" would be the smallest class bounded from below by all positions that appears in all OrdVectos.
            // Are all positions at this class?
            // Invariant: it can be true only if all positions are indeed synchronized.
            bool synchronized_at_current_minimum = false;

            // advance() advances all positions to the NEXT minimum and returns true,
            // or returns false if positions cannot be synchronized.
            // If positions are synchronized to start with,
            // then synchronized_at_current_minimum decides whether to stay or advance further.
            // The general of the algorithm is to synchronize everybody with position[0].
            bool advance()
            {
                //Nothing to synchronize.
                if (this->positions.empty())
                    return false; //?? or not?

                // If already synchronized, start moving forward by advancing position[0] (and so break synchronization).
                if (this->synchronized_at_current_minimum) {
                    ++this->positions[0];
                    synchronized_at_current_minimum = false;
                }

                // If positions[0] has nowhere to go, then sync is not possible.
                if (this->positions[0] == this->ends[0])
                    return false;

                // Synchronise all positions with position[0].
                for (size_t i = 1, positions_size = this->positions.size(); i < positions_size; ++i)
                {
                    // If some positions has nowhere to go, then sync is not possible.
                    if (this->positions[i] == this->ends[i])
                        return false;

                    //  Advance position[i] and position[0] to the closest equal values.
                    while (*this->positions[i] != *this->positions[0]) {

                        // Advance position[i] to or beyond position[0].
                        while (*this->positions[i] < *this->positions[0]) {
                            ++this->positions[i];
                            if (this->positions[i] == this->ends[i])
                                return false;
                        }

                        // Advance position[0] to or beyond position[i].
                        while (*this->positions[i] > *this->positions[0]) {
                            ++this->positions[0];
                            if (this->positions[0] == this->ends[0])
                                return false;
                        }

                        // If position[0] changed, start from position 1 again.
                        // (note that i gets incremented at the for-loop body end,
                        // and that, since we are inside the for, there are at least two positions
                        // as the for starts with i=1.)
                        if (this->positions[0] > this->positions[1])
                            i=0;
                    }
                }
                this->synchronized_at_current_minimum = true;
                return true;
            }

            const std::vector<Iterator> get_current()
            {
                // How to return so that things don't get copied?
                // Or maybe we should return a copy of positions anyway, to prevent a side effect of advance?
                // Instead of returning a vector, we could also provide iterators through the result.
                return this->positions;
            };

            explicit SynchronizedUniverzalIterator(const int size=0) : SynchronizedIterator<Key>(size) {};

            void reset(const int size = 0) {
                SynchronizedIterator < Key > ::reset(size);
                this->synchronized_at_current_minimum = false;
            };
        };




        template<typename Key>
        class SynchronizedExistentialIterator : public SynchronizedIterator<Key> {
        public:

            using Iterator = typename OrdVector<Key>::const_iterator;

            std::vector<Iterator> currently_synchronized; // Positions that are currently synchronized.
            Iterator next_minimum;

            // Advances all positions just above current_minimum,
            // that is, to or above next_minimum.
            // Those at next_minimum are added to currently_synchronized.
            // Since next_minimum becomes the current minimum,
            // new next_minimum must be updated too.
            bool advance() {
                // The next_minimum becomes the current current_minimum.
                auto current_minimum = this->next_minimum;

                // Here we collect the result.
                currently_synchronized.clear();

                for (size_t i = 0, positions_size = this->positions.size();i < positions_size;)
                {
                    if (this->positions[i] == this->ends[i])
                        // If there is nothing left at the position, it is removed, that is,
                        // swapped with a position form the end of the vector,
                        // and shorten the vector.
                        // The same is done with ends.
                        while (this->positions[i] == this->ends[i] && i < positions_size)
                        {
                            this->positions[i] = this->positions[positions_size - 1];
                            this->ends[i] =      this->ends     [positions_size - 1];
                            this->positions.pop_back();
                            this->ends     .pop_back();
                            positions_size--;
                        }
                    // If the i-th position, i.e., where we are now, was erased,
                    // then we reached the end of active positions.
                    if (i == positions_size)
                        return !currently_synchronized.empty();

                    // If we are at the current_minimum, then save it and advance the position.
                    if (*this->positions[i] == *current_minimum)
                    {
                        this->currently_synchronized.push_back(this->positions[i]);
                        ++this->positions[i];
                        continue;
                    }

                    // If the position (now larger than current_minimum) is smaller than next_minimum,
                    // or next_minimum has not yet been updated, then update the next_minimum.
                    if (*this->next_minimum > *this->positions[i] || *this->next_minimum == *current_minimum)
                        this->next_minimum = this->positions[i];

                    ++i;
                }
                return !currently_synchronized.empty();
            };

            const std::vector<Iterator> get_current()
            {
                return this->currently_synchronized;
            };

            // Supposed to be called only before the iteration starts.
            void push_back (const OrdVector<Key> &vector) {

                // Empty vector would not have any effect (unlike in the case of the universal iterator).
                if (vector.empty()) return;

                // Initialise next_minimum as the first position at the first vector.
                if (this->positions.empty())
                    this->next_minimum = vector.begin();

                    // If the first position is of the new vector is smaller then minimum,
                    // update minimum.
                else if (*this->next_minimum > *vector.begin())
                    this->next_minimum = vector.begin();

                // Let position point to the beginning the vector,
                // save the end of the vector.
                this->positions.push_back(vector.begin());
                this->ends.push_back(vector.end());
            };

            explicit SynchronizedExistentialIterator(const int size=0) : SynchronizedIterator<Key>(size)
            {
                this->currently_synchronized.reserve(size);
            };

            void reset(const int size = 0) {
                SynchronizedIterator < Key > ::reset(size);
                if (size > 0) {
                    this->currently_synchronized.reserve(size);
                }
            };
        };
    }
}

#endif //LIBMATA_SYNCHRONIZED_ITERATOR_HH
