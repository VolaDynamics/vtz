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


        constexpr bool is_file_sep( char ch ) {
#if _WIN32
            return ch == '\\' || ch == '/';
#else
            return ch == '/';
#endif
        }

#ifdef _WIN32
        constexpr char FILE_SEP = '\\';
#else
        constexpr char FILE_SEP = '/';
#endif
    } // namespace

    std::string join_path( string_view prefix, string_view file ) {
        // If the prefix is empty, just return the file
        if( prefix.empty() ) return std::string( file );

        // If it already ends in a file separator, just add the file
        if( is_file_sep( prefix.back() ) )
        {
            std::string result;
            result.reserve( prefix.size() + file.size() );
            result.append( prefix );
            result.append( file );
            return result;
        }

        std::string result;
        result.reserve( prefix.size() + file.size() + 1 );
        result.append( prefix );
        result.push_back( FILE_SEP );
        result.append( file );
        return result;
    }

    auto file_error( int errc, string_view fp, string_view verb )
        -> std::runtime_error {
        return std::runtime_error(
            fmt::format( "Error when {} '{}'. What: {} (OS Error {})",
                verb,
                fp,
                std::make_error_code( std::errc( errc ) ).message(),
                errc ) );
    }


    std::string read_file( std::string const& fp ) {
        return read_file( fp.c_str() );
    }

    std::string read_file( char const* fp ) {
        if( fp == nullptr )
            throw std::runtime_error( "read_file(): given null filepath" );

        std::FILE* file = std::fopen( fp, "rb" );

        if( file == nullptr ) throw file_error( errno, fp, "opening" );

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
                throw file_error( errc, fp, "reading" );

            // Add the bytes to the buffer
            result.append( buffer, bytes );

            if( std::feof( file ) ) { return result; }
        }
    }


    file_bytes read_file_bytes( std::string const& fp ) {
        return read_file_bytes( fp.c_str() );
    }

    file_bytes read_file_bytes( char const* fp ) {
        if( fp == nullptr )
            throw std::runtime_error( "read_file(): given null filepath" );

        return read_file_bytes( std::fopen( fp, "rb" ), fp );
    }

    file_bytes read_file_bytes( std::FILE* file, char const* fp ) {
        if( file == nullptr ) throw file_error( errno, fp, "opening" );

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
                throw file_error( errc, fp, "reading" );

            // Add the bytes to the buffer
            result.insert( result.end(), buffer, buffer + bytes );

            if( std::feof( file ) ) { return result; }
        }
    }

} // namespace vtz
