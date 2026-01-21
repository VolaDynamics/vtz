

def to_civil(days: int) -> tuple[int, int, int]:
    days += 719468  # Shift the epoch from 1970-01-01 to 0000-03-01
    era = days // 146097
    doe = days % 146097
    yoe = (doe - doe // 1460 + doe // 36524 - int(doe >= 146096)) // 365
    y = yoe + era * 400
    doy = doe - (365 * yoe + yoe // 4 - yoe // 100)
    mp = (5 * doy + 2) // 153
    d = doy - (153 * mp + 2) // 5 + 1
    m = mp + 3 if mp < 10 else mp - 9
    return (y + int(m <= 2), m, d)


def date_to_string(T: int) -> str:
    days = T // 86400
    tod = T % 86400
    hr = tod // 3600
    mi = (tod % 3600) // 60
    se = tod % 60
    y, m, d = to_civil(days)

    dow = (days + 4) % 7

    MONTHS = [
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    ]

    WEEKDAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]

    return f'{WEEKDAYS[dow]} {MONTHS[m-1]} {d:0>2} {hr:0>2}:{mi:0>2}:{se:0>2} {y:0>4}'

import datetime
print("Today:              ", date_to_string(int(datetime.datetime.timestamp(datetime.datetime.now()))))
print("max time_t 32-bit:  ", date_to_string(2**31-1))
print("max time_t 64-bit:  ", date_to_string(2**63-1))
print("max time_t 128-bit: ", date_to_string(2**127-1))
print("max time_t 256-bit: ", date_to_string(2**255-1))
