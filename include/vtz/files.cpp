#include <vtz/files.h>
#include <vtz/strings.h>

#include <cerrno>
#include <cstdio>
#include <fmt/format.h>
#include <stdexcept>
#include <system_error>

namespace vtz {
    using std::string_view;

    namespace {
        template<class T>
        struct _guard {
            T func;
            ~_guard() { func(); }
        };
        template<class T>
        _guard( T ) -> _guard<T>;


    } // namespace

    auto fileError( int errc, string_view fp, string_view verb )
        -> std::runtime_error {
        return std::runtime_error(
            fmt::format( "Error when {} '{}'. What: {} (OS Error {})",
                verb,
                fp,
                std::make_error_code( std::errc( errc ) ).message(),
                errc ) );
    }


    std::string readFile( std::string const& fp ) {
        return readFile( fp.c_str() );
    }

    std::string readFile( char const* fp ) {
        if( fp == nullptr )
            throw std::runtime_error( "read_file(): given null filepath" );

        std::FILE* file = std::fopen( fp, "rb" );

        if( file == nullptr ) throw fileError( errno, fp, "opening" );

        // Close the file when we leave the scope
        auto _close = _guard{ [file] {
            if( file != nullptr ) std::fclose( file );
        } };

        constexpr size_t BUFF_SZ = 1ull << 18;

        std::string result;

        char buffer[BUFF_SZ];
        for( ;; )
        {
            // Read some bytes
            size_t bytes = std::fread( buffer, 1, BUFF_SZ, file );

            // Check the error code
            if( int errc = std::ferror( file ) )
                throw fileError( errc, fp, "reading" );

            // Add the bytes to the buffer
            result.append( buffer, bytes );

            if( std::feof( file ) ) { return result; }
        }
    }


    file_bytes readFileBytes( std::string const& fp ) {
        return readFileBytes( fp.c_str() );
    }

    file_bytes readFileBytes( char const* fp ) {
        if( fp == nullptr )
            throw std::runtime_error( "read_file(): given null filepath" );

        return readFileBytes( std::fopen( fp, "rb" ), fp );
    }

    file_bytes readFileBytes( std::FILE* file, char const* fp ) {
        if( file == nullptr ) throw fileError( errno, fp, "opening" );

        // Close the file when we leave the scope
        auto _close = _guard{ [file] { std::fclose( file ); } };

        constexpr size_t BUFF_SZ = 1ull << 18;

        file_bytes result;

        char buffer[BUFF_SZ];
        for( ;; )
        {
            // Read some bytes
            size_t bytes = std::fread( buffer, 1, BUFF_SZ, file );

            // Check the error code
            if( int errc = std::ferror( file ) )
                throw fileError( errc, fp, "reading" );

            // Add the bytes to the buffer
            result.insert( result.end(), buffer, buffer + bytes );

            if( std::feof( file ) ) { return result; }
        }
    }

} // namespace vtz
