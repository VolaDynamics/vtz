import argparse
import re
import sys


def civil_from_days(days: int) -> tuple[int, int, int]:
    days += 719468
    era = days // 146097
    doe = days - era * 146097
    yoe = (doe - doe // 1460 + doe // 36524 - int(doe >= 146096)) // 365
    y = yoe + era * 400
    doy = doe - (365 * yoe + yoe // 4 - yoe // 100)
    mp = (5 * doy + 2) // 153
    d = doy - (153 * mp + 2) // 5 + 1
    m = mp + 3 if mp < 10 else mp - 9
    return (y + int(m <= 2), m, d)


def days_from_civil(y: int, m: int, d: int) -> int:
    y -= int(m <= 2)
    era = y // 400
    yoe = y - era * 400
    doy = (153 * (m - 3 if m > 2 else m + 9) + 2) // 5 + d - 1
    doe = yoe * 365 + yoe // 4 - yoe // 100 + doy
    return era * 146097 + doe - 719468


_DATETIME_RE = re.compile(
    r'^(-?\d+)-(\d{1,2})-(\d{1,2})\s+(\d{1,2}):(\d{2}):(\d{2})$'
)

_DATE_RE = re.compile(
    r'^(-?\d+)-(\d{1,2})-(\d{1,2})$'
)


def seconds_to_string(t: int) -> str:
    days = t // 86400
    tod = t % 86400
    hr = tod // 3600
    mi = (tod % 3600) // 60
    se = tod % 60
    y, m, d = civil_from_days(days)
    dow = (days + 4) % 7
    return f'{y:0>4}-{m:0>2}-{d:0>2} {hr:0>2}:{mi:0>2}:{se:0>2} (dow={dow})'


def string_to_seconds(s: str) -> int:
    match = _DATETIME_RE.match(s.strip())
    if not match:
        print(f"error: cannot parse '{s}', expected format: YYYY-MM-DD HH:MM:SS",
              file=sys.stderr)
        sys.exit(1)
    y, mo, d, hr, mi, se = (int(g) for g in match.groups())
    days = days_from_civil(y, mo, d)
    return days * 86400 + hr * 3600 + mi * 60 + se


def days_to_string(days: int) -> str:
    y, m, d = civil_from_days(days)
    dow = (days + 4) % 7
    return f'{y:0>4}-{m:0>2}-{d:0>2} (dow={dow})'


def string_to_days(s: str) -> int:
    match = _DATE_RE.match(s.strip())
    if not match:
        print(f"error: cannot parse '{s}', expected format: YYYY-MM-DD",
              file=sys.stderr)
        sys.exit(1)
    y, mo, d = (int(g) for g in match.groups())
    return days_from_civil(y, mo, d)


def cmd_from_days(args):
    for days in args.days:
        print(f"days={days} date={days_to_string(days)}")


def cmd_to_days(args):
    for s in args.dates:
        days = string_to_days(s)
        print(f"date='{s}' days={days}")


def cmd_from_seconds(args):
    for t in args.times:
        print(f"time={t} string={seconds_to_string(t)}")


def cmd_to_seconds(args):
    for s in args.datetimes:
        t = string_to_seconds(s)
        print(f"string='{s}' time={t}")


def main():
    cli = argparse.ArgumentParser(description='Convert between seconds and datetime strings')
    sub = cli.add_subparsers(dest='command', required=True)

    p_from = sub.add_parser('from_seconds',
                            help='Convert seconds since epoch to datetime strings')
    p_from.add_argument('times', type=int, nargs='+',
                        help='One or more times in seconds')
    p_from.set_defaults(func=cmd_from_seconds)

    p_from_d = sub.add_parser('from_days',
                                help='Convert days since epoch to date strings')
    p_from_d.add_argument('days', type=int, nargs='+',
                          help='One or more day counts')
    p_from_d.set_defaults(func=cmd_from_days)

    p_to_d = sub.add_parser('to_days',
                            help='Convert date strings to days since epoch')
    p_to_d.add_argument('dates', nargs='+',
                        help="One or more date strings in 'YYYY-MM-DD' format")
    p_to_d.set_defaults(func=cmd_to_days)

    p_to = sub.add_parser('to_seconds',
                          help='Convert datetime strings to seconds since epoch')
    p_to.add_argument('datetimes', nargs='+',
                       help="One or more datetime strings in 'YYYY-MM-DD HH:MM:SS' format")
    p_to.set_defaults(func=cmd_to_seconds)

    args = cli.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()
