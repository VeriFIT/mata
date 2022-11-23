//
// Created by Lukáš Holík on 29.10.2022.
//

#ifndef LIBMATA_SYNCHRONIZED_ITERATOR_HH
#define LIBMATA_SYNCHRONIZED_ITERATOR_HH

#include <mata/ord-vector.hh>

namespace Mata {
    namespace Util {
       /* Two classes that provide "synchronized" iterators through a vector of ordered vectors,
        *  (or of some ordered OrdContainer that have a similar iterator),
        *  needed in computation of post
        *  in subset construction, product, and non-determinization.
        *  The Type stored in OrdContainers must be comparable with <,>,==,!=,<=,>=,
        *  and it must be a total (linear) ordering.
        *  The intended usage in, for instance, determinisation is for Type to be TransSymbolStates.
        *  TransSymbolStates is ordered by the symbol.
        *
        *  SyncrhonisedIterator is the parent virtual class.
        *  It stores a vector of end-iterators for the OrdContainer v and a vector of current positions.
        *  They are filled in using the function push_back(begin,end), that adds begin and end iterators of v to positions and ends, respectively
        *  Method advance advances all positions forward so that they are synchronized on the next smallest equiv class
        *  (next smallest symbol in the case of TransSymbolStates).
        *
        *  There are two versions of the class.
        *  i) In product, ALL positions must point to currently the smallest equiv. class (Moves with the same symbol).
        *  Method get_current then returns the vector of all position iterators, synchronized.
        *  ii) In determinization, it is enough that there EXISTS a position that points to the smallest class.
        *  Method get_current then returns the vector of only those positions that point to the smallest equiv. class.
        *
        *  Usage: 0) construct, 1) fill in using push_back, iterate using advance and get_current, 2) reset, goto 1)
        *
        *  The memory allocated internally for positions and ends is kept after reset, so it is advisable to use the same iterator for many iterations, as
        *  opposed to creating a new one for each iteration.
        */
        template<typename Iterator> class SynchronizedIterator {
        public:

            std::vector<Iterator> positions;
            std::vector<Iterator> ends;

            //@param size Number of elements to reserve up-front for positions and ends.
            explicit SynchronizedIterator(const int size = 0)
            {
                positions.reserve(size);
                ends.reserve(size);
            };

           /* This is supposed to be called only before an iteration,
            * after constructor of reset.
            * Calling after advance breaks the iterator.
            * Specifies begin and end of one vector, to initialise before the iteration starts.
            */
            virtual void push_back (const Iterator &begin, const Iterator &end) {
                // Btw, I don't know what I am doing with the const & parameter passing,
                // begin is actually incremented in advance ...? But tests do pass ...
                this->positions.emplace_back(begin);
                this->ends.emplace_back(end);
            };


           /* Empties positions and ends.
            * Though they should keep the allocated space.
            * @param size Number of elements to reserve up-front for positions and ends.
            */
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



        template<typename Iterator>
        class SynchronizedUniversalIterator: public SynchronizedIterator<Iterator> {
        public:

           /* "minimum" would be the smallest class bounded from below by all positions that appears in all OrdContainers.
            * Are we sure that all positions at this class?
            * Invariant: it can be true only if all positions are indeed synchronized.
            */
            bool synchronized_at_current_minimum = false;

           /* advance() advances all positions to the NEXT minimum and returns true.
            * (though the next minimum might be the current state if synchronized_at_current_minimum is false).
            * or returns false if positions cannot be synchronized.
            * If positions are synchronized to start with,
            * then synchronized_at_current_minimum decides whether to stay or advance further.
            * The general of the algorithm is to synchronize everybody with position[0].
            */
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
                        // (note that
                        // i gets incremented at the end of the for-loop body,
                        // and that,
                        // since we are inside the for, there are at least two positions
                        // as the for starts with i=1.)
                        if (this->positions[0] > this->positions[1])
                            i=0;
                    }
                }
                this->synchronized_at_current_minimum = true;
                return true;
            }

            // Returns the vector of current positions.
            const std::vector<Iterator> get_current()
            {
                // How to return so that things don't get copied?
                // Or maybe we should return a copy of positions anyway, to prevent a side effect of advance?
                // Instead of returning a vector, we could also provide iterators through the result.
                return this->positions;
            };

            explicit SynchronizedUniversalIterator(const int size=0) : SynchronizedIterator<Iterator>(size) {};

            void reset(const int size = 0) {
                SynchronizedIterator < Iterator > ::reset(size);
                this->synchronized_at_current_minimum = false;
            };
        };

        template<typename Iterator>
        class SynchronizedExistentialIterator : public SynchronizedIterator<Iterator> {
        public:

            std::vector<Iterator> currently_synchronized; // Positions that are currently synchronized.

            Iterator next_minimum; // the value we should synchronise on after the first next call of advance().

            bool is_synchronized() {
                return !currently_synchronized.empty();
            }

            const Iterator get_current_minimum()
            {
                if (currently_synchronized.empty())
                    throw std::runtime_error("Trying to get minimum from sync. ex. iterator which has no minimum. Don't do that ever again or your nose falls off!");
                return currently_synchronized[0];
            }


            /* Advances all positions just above current_minimum,
             * that is, to or above next_minimum.
             * Those at next_minimum are added to currently_synchronized.
             * Since next_minimum becomes the current minimum,
             * new next_minimum must be updated too.
             */
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
                        this->currently_synchronized.emplace_back(this->positions[i]);
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

           /* Returns the vector of current still active positions.
            * Beware, thy will be ordered differently from how there were input into the iterator.
            * This is due to swapping of the emptied positions with positions at the end.
            */
            const std::vector<Iterator> get_current()
            {
                return this->currently_synchronized;
            };

            void push_back (const Iterator &begin, const Iterator &end) {

                // Empty vector would not have any effect (unlike in the case of the universal iterator).
                if (begin == end) return;

                // Initialise next_minimum as the first position at the first vector.
                if (this->positions.empty())
                    this->next_minimum = begin;


                // If the first position is of the new vector is smaller than minimum,
                // update minimum.
                else if (*this->next_minimum > *begin)
                    this->next_minimum = begin;

                // Let position point to the beginning the vector,
                // save the end of the vector.
                this->positions.emplace_back(begin);
                this->ends.emplace_back(end);
            };

            explicit SynchronizedExistentialIterator(const int size=0) : SynchronizedIterator<Iterator>(size)
            {
                this->currently_synchronized.reserve(size);
            };

            void reset(const int size = 0) {
                SynchronizedIterator < Iterator > ::reset(size);
                if (size > 0) {
                    this->currently_synchronized.reserve(size);
                }

                this->currently_synchronized.clear();

            };
        };

       /* In order to make initialisation of the sync. iterator nicer than inputting v.begin() and v.end()
        * as the two parameters of the method push_back,
        * this function wraps the method push_back,
        * takes the iterator and v and extracts the v.begin() and v.end() from v.
        */
        template<class Container>
        void push_back (SynchronizedIterator<typename Container::const_iterator> &i,const Container &container) {
            i.push_back(container.begin(),container.end());
        };
    }
}

#endif //LIBMATA_SYNCHRONIZED_ITERATOR_HH
