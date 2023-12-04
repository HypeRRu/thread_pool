#pragma once

#ifndef LOCAL_QUEUE_POOL_HPP
#define LOCAL_QUEUE_POOL_HPP

#include <thread>
#include <deque>
#include <vector>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <future>
#include <atomic>

namespace local_queue_pool
{

using lock_t = std::unique_lock< std::mutex >;
using task   = std::packaged_task< void() >;

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
    std::vector< notification_queue >   queues_;
    std::atomic< size_t >               index_;

    void run( const size_t thread_idx )
    {
        while ( true )
        {
            task f;
            if ( !queues_[ thread_idx ].pop( f ) )
            {
                return;
            }
            f();
        }
    }

public:
    thread_pool()
        : count_{ std::thread::hardware_concurrency() }
        , queues_{ count_ }
        , index_{ 0 }
    {
        for ( size_t idx = 0; idx < count_; ++idx )
        {
            threads_.emplace_back( [ this, idx ]{ run( idx ); } );
        }
    }

    ~thread_pool()
    {
        for ( auto& q: queues_ )
        {
            q.done();
        }
        for ( auto& t: threads_ )
        {
            t.join();
        }
    }

    template< typename Func, typename Ret = std::result_of_t< Func() > >
    std::future< Ret > add_task( Func&& f )
    {
        using ptask = std::packaged_task< Ret() >;
        ptask task_added{ std::move( f ) };
        std::future< Ret > result{ task_added.get_future() };

        const size_t thread_idx = index_++;
        queues_[ thread_idx % count_ ].push( std::forward< ptask >( task_added ) );
        return result;
    }
};

} // local_queue_pool

#endif // LOCAL_QUEUE_POOL_HPP
