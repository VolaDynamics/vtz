#pragma once

#include <cstddef>

/// A buffer of N bytes placed immediately before a guard page.
///
/// On Linux and MacOS, this allocates two pages of virtual memory. The first
/// page is readable and writable; the second page is protected (PROT_NONE),
/// so any read or write to it will trigger a segfault. The N-byte buffer is
/// positioned at the end of the first page, so that buf[N] falls on the
/// first byte of the guard page.
///
/// This means that if a function writes even a single byte past the end of
/// the buffer, the process will immediately fault rather than silently
/// corrupting memory. This is used in tests to verify that formatting
/// functions respect the output buffer size.
///
/// On Windows, where mmap/mprotect are not available, this falls back to a
/// plain heap allocation (no guard page).
///
/// A size of 0 is permitted. In that case, data() points to the guard page
/// boundary itself, and any write will fault.

class guard_page_buffer {
  public:

    explicit guard_page_buffer( size_t size );
    ~guard_page_buffer();

    guard_page_buffer( guard_page_buffer const& )            = delete;
    guard_page_buffer& operator=( guard_page_buffer const& ) = delete;

    char*  data() noexcept { return data_; }
    size_t size() const noexcept { return size_; }

  private:

    char*  data_;
    size_t size_;

#ifndef _WIN32
    void*  base_;     // start of the mmap'd region (for munmap)
    size_t map_size_; // total size of the mmap'd region
#endif
};
