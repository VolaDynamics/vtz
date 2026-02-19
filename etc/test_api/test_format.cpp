#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vtz/format.h>

using namespace vtz;

namespace {
    struct fmt_test_data_s {
        std::string_view fmt;
        int64_t          t;
        std::string_view out;
    };

    struct fmt_test_data_d {
        std::string_view fmt;
        int32_t          days;
        std::string_view out;
    };


    // Reference data obtained from etc/scripts/seconds_to_datetime.py:
    //
    //   0          = 1970-01-01 00:00:00 (dow=4, Thu)
    //   86399      = 1970-01-01 23:59:59 (dow=4, Thu)
    //   946684800  = 2000-01-01 00:00:00 (dow=6, Sat)
    //   1234567890 = 2009-02-13 23:31:30 (dow=5, Fri)
    //   1709294400 = 2024-03-01 12:00:00 (dow=5, Fri) [leap year]
    //   1735689599 = 2024-12-31 23:59:59 (dow=2, Tue) [leap year]
    //   1740000000 = 2025-02-19 21:20:00 (dow=3, Wed)
    //   1762442685 = 2025-11-06 15:24:45 (dow=4, Thu)

    fmt_test_data_s TEST_CASES_SECONDS[]{
        // Canonical datetime format
        { "%Y-%m-%d %H:%M:%S", 0, "1970-01-01 00:00:00" },
        { "%Y-%m-%d %H:%M:%S", 86399, "1970-01-01 23:59:59" },
        { "%Y-%m-%d %H:%M:%S", 946684800, "2000-01-01 00:00:00" },
        { "%Y-%m-%d %H:%M:%S", 1234567890, "2009-02-13 23:31:30" },
        { "%Y-%m-%d %H:%M:%S", 1709294400, "2024-03-01 12:00:00" },
        { "%Y-%m-%d %H:%M:%S", 1735689599, "2024-12-31 23:59:59" },
        { "%Y-%m-%d %H:%M:%S", 1740000000, "2025-02-19 21:20:00" },
        { "%Y-%m-%d %H:%M:%S", 1762442685, "2025-11-06 15:24:45" },

        // ISO 8601 equivalents (%F = %Y-%m-%d, %T = %H:%M:%S)
        { "%F %T", 0, "1970-01-01 00:00:00" },
        { "%F %T", 1234567890, "2009-02-13 23:31:30" },
        { "%F %T", 1740000000, "2025-02-19 21:20:00" },

        // Year
        { "%Y", 0, "1970" },
        { "%Y", 946684800, "2000" },
        { "%Y", 1740000000, "2025" },
        { "%y", 0, "70" },
        { "%y", 946684800, "00" },
        { "%y", 1740000000, "25" },

        // Month
        { "%m", 0, "01" },          // January
        { "%m", 1234567890, "02" }, // February
        { "%m", 1709294400, "03" }, // March
        { "%m", 1762442685, "11" }, // November
        { "%m", 1735689599, "12" }, // December

        // Day of month (%d zero-padded, %e space-padded)
        { "%d", 0, "01" },
        { "%d", 1234567890, "13" },
        { "%d", 1735689599, "31" },
        { "%e", 0, " 1" },
        { "%e", 1234567890, "13" },
        { "%e", 1735689599, "31" },

        // Hour (24-hour)
        { "%H", 0, "00" },
        { "%H", 1709294400, "12" },
        { "%H", 1762442685, "15" },
        { "%H", 86399, "23" },

        // Hour (12-hour)
        { "%I", 0, "12" },          // midnight = 12 AM
        { "%I", 1709294400, "12" }, // noon = 12 PM
        { "%I", 1762442685, "03" }, // 15:00 = 3 PM
        { "%I", 86399, "11" },      // 23:59 = 11 PM
        { "%I", 1740000000, "09" }, // 21:00 = 9 PM

        // Minute, second
        { "%M", 0, "00" },
        { "%M", 1740000000, "20" },
        { "%M", 1234567890, "31" },
        { "%M", 86399, "59" },
        { "%S", 0, "00" },
        { "%S", 1234567890, "30" },
        { "%S", 1762442685, "45" },
        { "%S", 86399, "59" },

        // %R = %H:%M
        { "%R", 0, "00:00" },
        { "%R", 1740000000, "21:20" },
        { "%R", 86399, "23:59" },

        // Day of year (%j)
        { "%j", 0, "001" },          // Jan 1
        { "%j", 1740000000, "050" }, // Feb 19 = day 50
        { "%j", 1762442685, "310" }, // Nov 6 = day 310
        { "%j", 1735689599, "366" }, // Dec 31 in leap year 2024
        { "%j", 1709294400, "061" }, // Mar 1 in leap year 2024
        { "%j", 1234567890, "044" }, // Feb 13 = day 44

        // Weekday (%w Sun=0, %u Mon=1 ISO)
        { "%w", 0, "4" },          // Thu
        { "%w", 946684800, "6" },  // Sat
        { "%w", 1234567890, "5" }, // Fri
        { "%w", 1740000000, "3" }, // Wed
        { "%w", 1735689599, "2" }, // Tue
        { "%u", 0, "4" },          // Thu
        { "%u", 946684800, "6" },  // Sat
        { "%u", 1234567890, "5" }, // Fri
        { "%u", 1740000000, "3" }, // Wed
        { "%u", 1735689599, "2" }, // Tue

        // Timezone specifiers (UTC context)
        { "%z", 0, "+00" },
        { "%z", 1740000000, "+00" },
        { "%Z", 0, "UTC" },
        { "%Z", 1740000000, "UTC" },
        { "%F %T %Z", 1740000000, "2025-02-19 21:20:00 UTC" },
        { "%F %T%z", 1740000000, "2025-02-19 21:20:00+00" },

        // Escape and literal specifiers
        { "%%", 0, "%" },
        { "%n", 0, "\n" },
        { "%t", 0, "\t" },
        { "hello", 0, "hello" },
        { "%%Y", 0, "%Y" },

        // Combined formats
        { "%Y%m%d", 1740000000, "20250219" },
        { "%Y%m%d %H%M%S", 1762442685, "20251106 152445" },
        { "%d/%m/%Y", 1234567890, "13/02/2009" },
        { "%H:%M on %F", 1740000000, "21:20 on 2025-02-19" },

        // Trailing literal after specifier
        { "%Yx", 1740000000, "2025x" },
        { "%Y-%m-%d end", 0, "1970-01-01 end" },
    };


    // Reference data obtained from etc/scripts/seconds_to_datetime.py:
    //
    //   0     = 1970-01-01 (dow=4, Thu)
    //   -1    = 1969-12-31 (dow=3, Wed)
    //   -365  = 1969-01-01 (dow=3, Wed)
    //   365   = 1971-01-01 (dow=5, Fri)
    //   10000 = 1997-05-19 (dow=1, Mon)
    //   18262 = 2020-01-01 (dow=3, Wed)
    //   19723 = 2024-01-01 (dow=1, Mon)
    //   20454 = 2026-01-01 (dow=4, Thu)

    fmt_test_data_d TEST_CASES_DATES[]{
        // Canonical date format
        { "%Y-%m-%d", 0, "1970-01-01" },
        { "%Y-%m-%d", -1, "1969-12-31" },
        { "%Y-%m-%d", -365, "1969-01-01" },
        { "%Y-%m-%d", 365, "1971-01-01" },
        { "%Y-%m-%d", 10000, "1997-05-19" },
        { "%Y-%m-%d", 18262, "2020-01-01" },
        { "%Y-%m-%d", 19723, "2024-01-01" },
        { "%Y-%m-%d", 20454, "2026-01-01" },

        // %F (equivalent to %Y-%m-%d)
        { "%F", 0, "1970-01-01" },
        { "%F", 10000, "1997-05-19" },
        { "%F", 20454, "2026-01-01" },

        // Year
        { "%Y", 0, "1970" },
        { "%Y", -365, "1969" },
        { "%Y", 10000, "1997" },
        { "%Y", 20454, "2026" },
        { "%y", 0, "70" },
        { "%y", -365, "69" },
        { "%y", 10000, "97" },
        { "%y", 20454, "26" },

        // Month
        { "%m", 0, "01" },
        { "%m", -1, "12" },
        { "%m", 10000, "05" },

        // Day (%d and %e)
        { "%d", 0, "01" },
        { "%d", -1, "31" },
        { "%d", 10000, "19" },
        { "%e", 0, " 1" },
        { "%e", -1, "31" },
        { "%e", 10000, "19" },

        // Day of year (%j)
        { "%j", 0, "001" },
        { "%j", 365, "001" },
        { "%j", -365, "001" },
        { "%j", 20454, "001" },
        { "%j", -1, "365" },    // Dec 31 of non-leap year 1969
        { "%j", 10000, "139" }, // 1997-05-19: 31+28+31+30+19 = 139

        // Weekday (%w Sun=0, %u Mon=1 ISO)
        { "%w", 0, "4" },     // Thu
        { "%w", -1, "3" },    // Wed
        { "%w", 10000, "1" }, // Mon
        { "%w", 19723, "1" }, // Mon
        { "%w", 365, "5" },   // Fri
        { "%u", 0, "4" },     // Thu
        { "%u", -1, "3" },    // Wed
        { "%u", 10000, "1" }, // Mon
        { "%u", 19723, "1" }, // Mon
        { "%u", 365, "5" },   // Fri

        // Escape specifiers
        { "%%", 0, "%" },
        { "%%F", 0, "%F" },

        // Combined formats
        { "%Y%m%d", 10000, "19970519" },
        { "%d/%m/%Y", 10000, "19/05/1997" },
    };
} // namespace


// Alias for std::string_view, to make test code more compact
using sv = std::string_view;


TEST( vtz_format, format_time ) {
    for( auto const& tc : TEST_CASES_SECONDS )
    {
        auto t_sys = sys_seconds( seconds( tc.t ) );

        // format_time_s
        ASSERT_EQ( format_time_s( tc.fmt, tc.t ), tc.out );

        // format_time (chrono wrapper)
        ASSERT_EQ( format_time( tc.fmt, t_sys ), tc.out );

        // format_time_to_s (buffer + truncation)
        {
            char   buf[128];
            size_t n = format_time_to_s( tc.fmt, tc.t, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), tc.out );

            for( size_t sz = 0; sz < tc.out.size(); sz++ )
            {
                size_t written = format_time_to_s( tc.fmt, tc.t, buf, sz );
                ASSERT_EQ( written, sz );
                ASSERT_EQ( sv( buf, written ), tc.out.substr( 0, sz ) );
            }
        }

        // format_time_to (chrono buffer wrapper + truncation)
        {
            char   buf[128];
            size_t n = format_time_to( tc.fmt, t_sys, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), tc.out );

            for( size_t sz = 0; sz < tc.out.size(); sz++ )
            {
                size_t written = format_time_to( tc.fmt, t_sys, buf, sz );
                ASSERT_EQ( written, sz );
                ASSERT_EQ( sv( buf, written ), tc.out.substr( 0, sz ) );
            }
        }
    }
}


TEST( vtz_format, format_date ) {
    for( auto const& tc : TEST_CASES_DATES )
    {
        auto d_sys  = sys_days( days( tc.days ) );

        // format_date_d
        ASSERT_EQ( format_date_d( tc.fmt, tc.days ), tc.out );

        // format_date (chrono wrapper)
        ASSERT_EQ( format_date( tc.fmt, d_sys ), tc.out );

        // format_date_to_d (buffer + truncation)
        {
            char   buf[128];
            size_t n = format_date_to_d( tc.fmt, tc.days, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), tc.out );

            for( size_t sz = 0; sz < tc.out.size(); sz++ )
            {
                size_t written = format_date_to_d( tc.fmt, tc.days, buf, sz );
                ASSERT_EQ( written, sz );
                ASSERT_EQ( sv( buf, written ), tc.out.substr( 0, sz ) );
            }
        }

        // format_date_to (chrono buffer wrapper + truncation)
        {
            char   buf[128];
            size_t n = format_date_to( tc.fmt, d_sys, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), tc.out );

            for( size_t sz = 0; sz < tc.out.size(); sz++ )
            {
                size_t written = format_date_to( tc.fmt, d_sys, buf, sz );
                ASSERT_EQ( written, sz );
                ASSERT_EQ( sv( buf, written ), tc.out.substr( 0, sz ) );
            }
        }
    }
}
