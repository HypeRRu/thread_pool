#include <iostream>
#include <vector>

#include "simple_pool.hpp"
#include "retval_pool.hpp"
#include "local_queue_pool.hpp"
#include "work_stealing_pool.hpp"


int main()
{
    std::vector< std::future< int > > f;

    work_stealing_pool::thread_pool pool;
    for ( size_t i = 0; i < 100; ++i )
    {
        f.emplace_back( pool.add_task( [ i ]() -> int { std::cout << "hello " << i << "\n"; return i; } ) );
    }

    for ( size_t i = 0; i < 100; ++i )
    {
        std::cerr << "wait res " << f[ i ].get() << std::endl;
    }
    
    return 0;
}
