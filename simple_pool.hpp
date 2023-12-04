#pragma once

#ifndef SIMPLE_POOL_HPP
#define SIMPLE_POOL_HPP

#include <thread>
#include <deque>
#include <vector>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <atomic>

namespace simple_pool
{

using lock_t = std::unique_lock< std::mutex >;
using task   = std::function< void() >;

class notification_queue
{
private:
    std::deque< task >      q_;
    bool                    done_{ false };
    std::mutex              mutex_;
    std::condition_variable ready_;

public:
    void done()
    {
        {
            lock_t lock{ mutex_ };
            done_ = true;
        }
        ready_.notify_all();
    }

    bool pop( task& x )
    {
        lock_t lock{ mutex_ };
        ready_.wait( lock, [ this ]{
            return !q_.empty() || done_;
        } );
        if ( done_ )
        {
            return false;
        }
        x = std::move( q_.front() );
        q_.pop_front();
        return true;
    }

    template< typename Func >
    void push( Func&& f )
    {
        {
            lock_t lock{ mutex_ };
            q_.emplace_back( std::forward< Func >( f ) );
        }
        ready_.notify_one();
    }
};


class thread_pool
{
private:
    const size_t                        count_;
    std::vector< std::thread >          threads_;
    notification_queue                  queue_;

    void run( const size_t thread_idx )
    {
        while ( true )
        {
            task f;
            if ( !queue_.pop( f ) )
            {
                return;
            }
            f();
        }
    }

public:
    thread_pool() : count_{ std::thread::hardware_concurrency() }
    {
        for ( size_t idx = 0; idx < count_; ++idx )
        {
            threads_.emplace_back( [ this, idx ]{ run( idx ); } );
        }
    }

    ~thread_pool()
    {
        queue_.done();
        for ( auto& t: threads_ )
        {
            t.join();
        }
    }

    template< typename Func >
    void add_task( Func&& f )
    {
        queue_.push( std::forward< Func >( f ) );
    }
};

} // simple_pool

#endif // SIMPLE_POOL_HPP
