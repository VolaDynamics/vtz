#include "guard_page_buffer.h"

#ifndef _WIN32

    #include <sys/mman.h>
    #include <unistd.h>

    #include <cerrno>
    #include <cstdlib>
    #include <cstring>
    #include <stdexcept>
    #include <string>

guard_page_buffer::guard_page_buffer( size_t size )
: size_( size ) {
    long page_size = sysconf( _SC_PAGESIZE );
    if( page_size <= 0 )
        throw std::runtime_error( "sysconf(_SC_PAGESIZE) failed" );

    auto ps = size_t( page_size );

    // We need enough writable pages to hold `size` bytes, plus one guard
    // page. Round up the data region to a page boundary.
    size_t data_pages = ( size + ps - 1 ) / ps;
    if( data_pages == 0 ) data_pages = 1;
    map_size_ = ( data_pages + 1 ) * ps;

    base_ = mmap( nullptr,
        map_size_,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0 );
    if( base_ == MAP_FAILED )
        throw std::runtime_error(
            std::string( "mmap failed: " ) + std::strerror( errno ) );

    // The guard page is the last page in the mapping.
    void* guard = static_cast<char*>( base_ ) + map_size_ - ps;
    if( mprotect( guard, ps, PROT_NONE ) != 0 )
    {
        int err = errno;
        munmap( base_, map_size_ );
        throw std::runtime_error(
            std::string( "mprotect failed: " ) + std::strerror( err ) );
    }

    // Position the buffer so that data_[size_] is the first byte of the
    // guard page.
    data_ = static_cast<char*>( guard ) - size;
}

guard_page_buffer::~guard_page_buffer() { munmap( base_, map_size_ ); }

#else // _WIN32

guard_page_buffer::guard_page_buffer( size_t size )
: data_( new char[size ? size : 1] )
, size_( size ) {}

guard_page_buffer::~guard_page_buffer() { delete[] data_; }

#endif
