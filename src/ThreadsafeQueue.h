/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013, 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

   An implementation for a threadsafe queue using boost thread library

   When creating a ThreadsafeQueue, one can specify the minimal number
   of elements it must contain before it is possible to take one
   element out.
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H

#include <boost/thread.hpp>
#include <queue>

/* This queue is meant to be used by two threads. One producer
 * that pushes elements into the queue, and one consumer that
 * retrieves the elements.
 *
 * The queue can make the consumer block until an element
 * is available.
 */

template<typename T>
class ThreadsafeQueue
{
public:
    /* Push one element into the queue, and notify another thread that
     * might be waiting.
     *
     * returns the new queue size.
     */
    size_t push(T const& val)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        the_queue.push(val);
        size_t queue_size = the_queue.size();
        lock.unlock();

        notify();

        return queue_size;
    }

    void notify()
    {
        the_condition_variable.notify_one();
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(the_mutex);
        return the_queue.empty();
    }

    size_t size() const
    {
        boost::mutex::scoped_lock lock(the_mutex);
        return the_queue.size();
    }

    bool try_pop(T& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        if (the_queue.empty()) {
            return false;
        }

        popped_value = the_queue.front();
        the_queue.pop();
        return true;
    }

    void wait_and_pop(T& popped_value, size_t prebuffering = 1)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        while (the_queue.size() < prebuffering) {
            the_condition_variable.wait(lock);
        }

        popped_value = the_queue.front();
        the_queue.pop();
    }

private:
    std::queue<T> the_queue;
    mutable boost::mutex the_mutex;
    boost::condition_variable the_condition_variable;
};

#endif

