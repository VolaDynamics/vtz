

#include <ctime>
#include <fmt/format.h>
#include <iomanip>
#include <sstream>
#include <vtz/civil.h>

namespace vtz {
    static sysdays_t dateFromTM( std::tm const& t ) noexcept {
        int year = 1900 + t.tm_year;
        if( t.tm_mday || t.tm_mon )
        {
            int mday = t.tm_mday;
            // If the mday wasn't specified, assume the 1st
            if( mday == 0 ) mday = 1;
            return resolveCivil( year, t.tm_mon + 1, mday );
        }
        else
        {
            // Use the day of the year. if the day of the year is 0, this will
            // just give us the first of the year.
            return resolveCivilOrdinal( t.tm_year, t.tm_yday + 1 );
        }
    }
    /// Convert from `tm` struct to seconds since the epoch
    static sec_t timeFromTM( std::tm const& t ) noexcept {
        return daysToSeconds( dateFromTM( t ), t.tm_hour, t.tm_min, t.tm_sec );
    }


    sysdays_t parse_date( string_view t, char const* fmt ) {
        auto    ss = std::istringstream( std::string( t ) );
        std::tm tm{};
        ss >> std::get_time( &tm, fmt );

        if( ss.fail() )
        {
            throw std::runtime_error( fmt::format( "Unable to parse {} as {}",
                escapeString( t ),
                escapeString( fmt ) ) );
        }

        return dateFromTM( tm );
    }


    sysseconds_t parse_time( string_view t, char const* fmt ) {
        auto    ss = std::istringstream( std::string( t ) );
        std::tm tm{};
        ss >> std::get_time( &tm, fmt );

        if( ss.fail() )
        {
            throw std::runtime_error( fmt::format( "Unable to parse {} as {}",
                escapeString( t ),
                escapeString( fmt ) ) );
        }

        return timeFromTM( tm );
    }
} // namespace vtz
