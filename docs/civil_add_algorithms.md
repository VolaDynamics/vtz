# Civil month and year arithmetic

How `civil_add_months`, `civil_add_years`, and their clamped variants work.

This is a companion to Howard Hinnant's [_chrono-Compatible Low-Level Date
Algorithms_][hinnant], and it assumes you have read that page. What it adds is
the part that differs: the four `civil_add_*` functions do not decode to a
`(y, m, d)` triple and re-encode. They compute a day _delta_ and add it, over a
day count cut on the **century** rather than on the 400-year era — a
decomposition due to Neri and Schneider, credited in
[§1](#1-what-is-borrowed-and-what-is-new).

[hinnant]: https://howardhinnant.github.io/date_algorithms.html

The code is in [`include/impl/vtz/civil.h`](../include/impl/vtz/civil.h), in
namespace `vtz::_civ`. It is dense, which is why this document exists.

## Contents

- [1. What is borrowed, and what is new](#1-what-is-borrowed-and-what-is-new)
- [2. Why not decode, shift, and re-encode?](#2-why-not-decode-shift-and-re-encode)
- [3. The century as the working window](#3-the-century-as-the-working-window)
- [4. `split_century`](#4-split_century)
- [5. The month magic: 535, 331, 14](#5-the-month-magic-535-331-14)
- [6. The collapsed leap test](#6-the-collapsed-leap-test)
- [7. `add_years_impl`](#7-add_years_impl)
- [8. `add_months_impl`](#8-add_months_impl)
- [9. Clamping, and the packed month-length table](#9-clamping-and-the-packed-month-length-table)
- [10. The domain](#10-the-domain)
- [11. The proof](#11-the-proof)
- [12. Yes, but how do you know this all really works?](#12-yes-but-how-do-you-know-this-all-really-works)
- [13. What it costs](#13-what-it-costs)
- [Where the code lives](#where-the-code-lives)
- [References](#references)

---

## 1. What is borrowed, and what is new

Borrowed from Hinnant's [`civil_from_days`][cfd] and [`days_from_civil`][dfc]
without change:

- **The epoch shift.** `719468` moves day 0 from 1970-01-01 to 0000-03-01.
- **The March-based year.** Months are renumbered so that March is month 0 and
  February is month 11. Hinnant's reason for it is what makes everything below
  easy: it "puts the leap day, Feb. 29 as the last day of the year", so a leap
  day is _appended to a year_ and never _inserted into one_.
- **The month table.** `month_start( mp ) == ( 153 * mp + 2 ) / 5` is the number
  of days from March 1st to the first of March-based month `mp`, derived in
  [_Computing day-of-year from month and day-of-month_][doyfm]. vtz gives it a
  name; `to_civil` and `resolve_civil` spell the same expression out inline.
- **The names.** `doy` and `mp` mean what they mean on Hinnant's page, as do
  `doe` and `yoe` in the era-split functions that are left unchanged.

[cfd]: https://howardhinnant.github.io/date_algorithms.html#civil_from_days
[dfc]: https://howardhinnant.github.io/date_algorithms.html#days_from_civil
[doyfm]: https://howardhinnant.github.io/date_algorithms.html#Computing%20day-of-year%20from%20month%20and%20day-of-month
[mfdoy]: https://howardhinnant.github.io/date_algorithms.html#Computing%20month%20from%20day-of-year

Borrowed from Neri and Schneider's [_Euclidean affine functions_][ns], which is
where the entire decode of [§3](#3-the-century-as-the-working-window)–[§5](#5-the-month-magic-535-331-14)
comes from. The [References](#references) have the full accounting:

- **The window is a century, not an era.** Cutting the day count on the century
  is the first step of their `to_date`. `yoe`/`doe` give way to `z` (year of
  century, `[0, 99]`) and `doc` (day of century, `[0, 36524]`).
- **The `4x + 3` scaling, three levels deep.** Century from the day count, year
  from the day of the century, month from the day of the year: the same affine
  form each time, each level reusing its own remainder to feed the next.
- **Month and day-of-month from a single product.** High bits give the month, low
  bits the day within it.

New here, and the subject of the rest of this document:

- **Nothing is re-encoded.** The four public functions return `days + delta`.
  Neri and Schneider give the two conversions and not calendar arithmetic, so the
  delta formulation of [§7](#7-add_years_impl) and [§8](#8-add_months_impl) does
  not come from there.
- **The absolute year never appears.** Only differences are needed, and the
  century index enters every formula through its low two bits alone.
- **The whole `i32` domain.** Their century step holds `4N + 3` in a `u32`, which
  bounds the input to `N < 2³⁰` — about a quarter of the `i32` day range. Forming
  it in 64 bits costs one instruction and buys the rest.
  [§3.1](#31-an-unsigned-day-count), [§10](#10-the-domain).

The second of those is what makes the century window legal _here_. A century is
_not_ a self-similar unit of the Gregorian calendar the way an era is — 36524 days
is the length of a century only three times in four. It works because these
functions never need to know which century they are in, only how many century
boundaries a shift crosses. Their `to_date` does need the absolute century, and
recovers the year as `100·C + Z`; nothing below ever does.

## 2. Why not decode, shift, and re-encode?

The obvious implementation of "add `k` months" is the specification, and it is
what the tests use as a reference:

```cpp
// etc/test/test_impl/test_civil.cpp, namespace ref
constexpr i64 add_months( i64 days, int months, bool clamp ) noexcept {
    ymd t  = to_civil( days );
    i64 m0 = i64( t.month ) - 1 + months; // 0-based
    i64 y2 = t.year + fdiv( m0, 12 );
    int m2 = int( fmod( m0, 12 ) ) + 1;
    int d  = t.day;
    if( clamp )
    {
        int last = days_in_month( y2, m2 );
        if( d > last ) d = last;
    }
    return resolve_civil( y2, m2, d );
}
```

That costs a full `civil_from_days` and a full `days_from_civil`. The expensive
part of `civil_from_days` is recovering the year of era:

```cpp
const u32 yoe = ( doe - doe / 1460 + doe / 36524 - doe / 146096 ) / 365;
```

Four divisions, and the three correction terms exist purely to undo the leap
exceptions that a 400-year era straddles: `/1460` for the divisible-by-4 rule,
`/36524` for the divisible-by-100 exception, `/146096` for the divisible-by-400
exception to the exception. `days_from_civil` then pays the mirror image of the
same thing on the way back out (`yoe/4 - yoe/100`, plus the floor-division of
the year by 400).

But a month or year shift does not need an absolute year. It needs the number of
days between where you are and where you are going — and a difference can be
taken in a window that hides the leap exceptions, where an absolute year cannot.

The century is that window.

## 3. The century as the working window

The Gregorian leap rule ([`is_leap`][isleap]) has three clauses:

```cpp
constexpr bool is_leap( i32 year ) noexcept {
    bool div4   = year % 4 == 0;
    bool div100 = year % 100 == 0;
    bool div400 = year % 400 == 0;
    return div4 && ( !div100 || div400 );
}
```

[isleap]: https://howardhinnant.github.io/date_algorithms.html#is_leap

Number the March-based years inside century `c` as `z ∈ [0, 99]`, so that year
`(c, z)` runs from March 1st of calendar year `100c + z` to the end of the
February that follows. Whether that year has 366 days depends on the leap status
of `100c + z + 1`, and that number is a multiple of 100 for exactly one `z`:
`z == 99`. So for 99 of the 100 years in the window, `div100` and `div400` are
both false and the rule degenerates to `div4` — a mask.

The hundredth year sits at the window boundary. That is why the window was chosen
this way: the one hard case then falls to be handled once per boundary crossing
rather than once per query, which is what `century_start`'s `m >> 2` does.
[§6](#6-the-collapsed-leap-test) is all that remains of the three-clause test
afterwards.

Concretely: inside century `c` the year lengths run 365, 365, 365, 366 for
`z ∈ [0, 95]`, then 365, 365, 365 for `z ∈ [96, 98]`, and year 99 is 366 exactly
when `c % 4 == 3`. A century is therefore 36524 days long three times in four,
and 36525 once.

### 3.1 An unsigned day count

Every division below is a truncating machine division. For unsigned operands
truncation _is_ floor division, so all of Hinnant's `(z >= 0 ? z : z - 146096)`
style fixups disappear if the day count can be made non-negative first. So:

```cpp
constexpr u32 CENTURY_ORIGIN = 2147614883u;

u64 n = u64( i64( days ) + i64( CENTURY_ORIGIN ) );
```

`CENTURY_ORIGIN` has to satisfy exactly two properties, both asserted at the
declaration:

1. **It is `719468` plus a whole number of eras.** `2147614883 - 719468 ==
146097 * 14695`. Whole eras cancel out of every difference the code computes,
   and they do not disturb any `% 4` or `% 400` test, so adding 14695 of them is
   free. Century index 0 therefore begins on March 1st of calendar year
   `-400 * 14695 == -5878000`, a multiple of 400 — so the century index is
   aligned with the leap rule, and `c % 4` alone says which of an era's four
   positions century `c` occupies.
2. **It is at least 2³¹**, so that `days + CENTURY_ORIGIN` is non-negative even
   at `days == INT32_MIN`, and the decode can be unsigned throughout.

Those two pin the constant _exactly_: the largest era-aligned value below 2³¹ is
`2147468786`, which fails property 2, so `2147614883` is the smallest usable
origin. And the pinning is what forces the sum into 64 bits, since

$$\texttt{MAX\_SHIFTED\_DAYS} = \texttt{INT32\_MAX} + \texttt{CENTURY\_ORIGIN} = 4295098530 > 2^{32} - 1$$

There is no era-aligned origin that keeps the shifted count inside a `u32`, and
the shape of the obstruction is why: the overflow is exactly
`CENTURY_ORIGIN - 2³¹ == 131235`, because the sum tops out at
`2³¹ - 1 + CENTURY_ORIGIN` while a `u32` tops out at `2³² - 1`. Lowering the
origin to shrink the overflow lowers it below 2³¹ by the same amount. A third
`static_assert` records the impossibility:

```cpp
static_assert( CENTURY_ORIGIN - 146097u < 2147483648u,
    "CENTURY_ORIGIN must be the smallest era-aligned value >= 2^31" );
```

Earlier revisions did compute `n` in a `u32` and paid for it: the top 131,235
day counts wrapped, and about half of them decoded to a self-consistent but
_wrong_ date, off by one to four days. See [§10](#10-the-domain).

### 3.2 Where a century starts

```cpp
constexpr u32 DAYS_PER_CENTURY = 36524u;

constexpr u64 century_start( u64 m ) noexcept {
    return u64( DAYS_PER_CENTURY ) * m + ( m >> 2 );
}
```

36524 days per century, plus one extra day for each divisible-by-400 February
29th already passed — one per four centuries, contributed by the century that
ends with it, `c % 4 == 3`. Writing the closed form as a single floor is what
makes the inverse mechanical:

$$\texttt{century\_start}(m) = 36524m + \left\lfloor \frac{m}{4} \right\rfloor = \left\lfloor \frac{146097m}{4} \right\rfloor$$

(Check: `146097 == 4 * 36524 + 1`, so the two agree; `century_start(4) == 146097`,
one era, as it must be.)

### 3.3 Inverting it

The century containing day `n` is the largest `m` whose start is not past `n`:

$$
c = \max\left\{m : \left\lfloor \tfrac{146097m}{4} \right\rfloor \le n\right\}
  = \max\{m : 146097m < 4(n+1)\}
  = \left\lfloor \frac{4(n+1) - 1}{146097} \right\rfloor
  = \left\lfloor \frac{4n+3}{146097} \right\rfloor
$$

This is Neri and Schneider's, from [_Euclidean affine functions_][ns]: the
century step of their `to_date` is `C = (4N + 3) / 146097`, with the day of the
century falling out of the same remainder. The two levels below it, in
[§4](#4-split_century) and [§5](#5-the-month-magic-535-331-14), are theirs too.

The `-1` in `4(n+1) - 1` is load-bearing: without it, the formula reads one
century too high on every day that is the last day of an era.

[ns]: https://arxiv.org/abs/2102.06959

Computing that quotient directly would cost a 64-bit division by a constant, and
the shifted count needs 33 bits, so there is no narrow form to fall back on.
Instead:

```cpp
constexpr u64 CENTURY_MAGIC = 3853261555ull;
constexpr u32 CENTURY_SHIFT = 47u;

constexpr u32 century_of( u64 n ) noexcept {
    return u32( ( ( n + 1 ) * CENTURY_MAGIC ) >> CENTURY_SHIFT );
}
```

One multiply and one shift; the result is at most a few hundred thousand, so only
the argument has to be wide. The multiplier is

$$\texttt{CENTURY\_MAGIC} = \left\lfloor \frac{2^{47} \cdot 4}{146097} \right\rfloor = \lfloor 3853261555.14 \rfloor = 3853261555$$

so `((n+1) * M) >> 47` is a scaled, rounded-down stand-in for
`4(n+1) / 146097`. Rounded _down_ is the essential part, and it is what supplies
the `-1`: at `n + 1 == 146097k`, the exact quotient `4(n+1)/146097` is the
integer `4k`, but the truncated multiplier undershoots it and the shift yields
`4k - 1`, which is the century index we actually want. Using the ceiling
(3853261556) instead gives the wrong answer at the very first era boundary,
`n == 146096`.

**Exactness.** The `+ 1` inside `century_of` is the `B` of the affine form, not
an off-by-one patch, and the identity

$$\left\lfloor \frac{(n+1) \cdot 3853261555}{2^{47}} \right\rfloor = \left\lfloor \frac{4n+3}{146097} \right\rfloor$$

holds for every `n` that `split_century` can produce. Three separate bounds are
worth keeping straight:

| Bound                | Value          | What it is                                                                                           |
| -------------------- | -------------- | ---------------------------------------------------------------------------------------------------- |
| Reachable            | century 117595 | The century `MAX_SHIFTED_DAYS` lands in. Derived, not written out: `century_of( MAX_SHIFTED_DAYS )`. |
| `u64`-safe           | century 131071 | Past `n == 4787306495`, `(n + 1) * M` leaves `u64`.                                                  |
| Mathematically exact | century 188178 | The last century the identity holds for; 188179 is the first to fail.                                |

The binding limit is the `u64` product, and the margin to it is 1.11× — not
large, but checked: it and the reachable bound are both asserted, so widening the
day count trips the build rather than quietly returning wrong centuries.
Perturbing the multiplier by ±1 fails far sooner — `M + 1` already misreads the
last day of century 3, `M - 1` century 23135 — and both are caught by the
compile-time guard in [§12.1](#121-compile-time-guards-on-the-constants).

## 4. `split_century`

Everything above assembles into six statements:

```cpp
VTZ_INLINE constexpr century_parts split_century( sys_days_t days ) noexcept {
    u64 n   = u64( i64( days ) + i64( CENTURY_ORIGIN ) );
    u32 c   = century_of( n );
    u32 doc = u32( n - century_start( c ) ); // narrows: doc <= 36524
    u32 t   = 4 * doc + 3;
    u32 z   = t / 1461;              // [0, 99]
    u32 doy = ( t - 1461 * z ) >> 2; // [0, ( 1461 - 1 ) >> 2] == [0, 365]
    return { c, z, doc, doy };
}
```

The first three lines are [§3](#3-the-century-as-the-working-window). The last
three extract the year within the century, and they are the _same derivation one
level down_ — which is how Neri and Schneider's `to_date` proceeds as well, using
a `2939745 / 2³²` multiply-shift where a division by 1461 serves here.

Inside a century, March-based year `j` begins `365j + ⌊j/4⌋` days after the
century start — 365 days a year plus the divisible-by-4 leap days, with no
exceptions to correct for, which is the payoff for cutting on the century. And
just as before,

$$365j + \left\lfloor \frac{j}{4} \right\rfloor = \left\lfloor \frac{1461j}{4} \right\rfloor$$

so inverting it is the same maximisation with 1461 in place of 146097:

$$z = \left\lfloor \frac{4 \cdot \texttt{doc} + 3}{1461} \right\rfloor$$

which is `t = 4 * doc + 3; z = t / 1461`. No multiplier is needed this time:
`doc ≤ 36524`, so `4 * doc + 3` cannot overflow, and the division is by a
compile-time constant. (`4 * doc` has two zero low bits, so the `+ 3` compiles to
an `orr`, not an add.)

**The remainder is the day of the year, times four.** Write `s = z % 4`. Since
`1461 ≡ 1 (mod 4)`:

$$4 \cdot \texttt{doy} = 4\,\texttt{doc} - 1461z + s \quad\Longrightarrow\quad t - 1461z = 4\,\texttt{doy} + (3 - s)$$

and `3 - s ∈ [0, 3]`, so `>> 2` recovers `doy` exactly and discards the junk.
This is why the `+ 3` is the right offset and not, say, `+ 1`: it is
simultaneously the `-1` that makes the quotient land on year boundaries and the
padding that keeps the remainder's low two bits from borrowing.

`doy` reaches 365 only on a February 29th, since February is the last month of a
March-based year. That is what reduces the year-addition clamp to a single
subtraction in [§7.2](#72-clamping).

Note what is _absent_: there is no correction term. The
`- doe/1460 + doe/36524 - doe/146096` apparatus of the era split exists to repair
leap exceptions that are not present inside a century.

## 5. The month magic: 535, 331, 14

Given `doy`, Hinnant's `civil_from_days` recovers the March-based month and the
day within it with two divisions:

```cpp
const u32 mp = ( 5 * doy + 2 ) / 153;      // [0, 11]
const u32 d  = doy - ( 153 * mp + 2 ) / 5; // [0, 30]
```

The `+ 2` in the first is not algebra; [_Computing month from day-of-year_][mfdoy]
shows the naive inverse `(5·doy − 2)/153` failing on real inputs, and arrives at
the shipped form by fixing the slope and then tuning the intercept until every
month boundary lands right. What follows is the same exercise done once more, with
two extra requirements: the denominator must be a power of two, so the divide
becomes a shift, and the _remainder_ must stay usable, so that one product yields
both halves of the answer.

Both requirements, and reading the whole answer out of one product, are Neri and
Schneider's — their `to_date` uses `2141 · doy + 197913` with a 16-bit shift. The
constants differ here only because vtz wants a 0-based March-relative month where
they number months 3 through 14.

`civil_add_months` therefore does it with one multiply and one division:

```cpp
constexpr u32 MONTH_MAGIC_MUL   = 535u;
constexpr u32 MONTH_MAGIC_ADD   = 331u;
constexpr u32 MONTH_MAGIC_SHIFT = 14u;
constexpr u32 MONTH_MAGIC_MASK  = ( 1u << MONTH_MAGIC_SHIFT ) - 1u;

u32 t   = MONTH_MAGIC_MUL * doy + MONTH_MAGIC_ADD;
u32 mp  = t >> MONTH_MAGIC_SHIFT;                    // [0, 11], 0 is March
u32 dom = ( t & MONTH_MAGIC_MASK ) / MONTH_MAGIC_MUL;
```

The high bits of a single product give the month; the low bits of the same product
give the 0-based day within it. The claim is not that `535/16384` approximates
`5/153`. It does, but approximation is not the condition — the condition is on the
twelve month boundaries.

Define, for each March-based month `mp`,

$$r_{mp} = 535 \cdot \texttt{month\_start}(mp) + 331 - 16384 \cdot mp$$

which is the value of the low 14 bits on the _first_ day of month `mp`. For a
`doy` inside month `mp`, with `dom = doy - month_start(mp)`, the product is

$$t = 16384 \cdot mp + 535 \cdot \texttt{dom} + r_{mp}$$

so both identities hold for every day of month `mp` exactly when:

- `0 ≤ r_mp < 535` — so that the low bits divided by 535 give `dom` and not
  `dom ± 1`, and so that `t >> 14` has not stepped early; **and**
- `535 · (len_mp − 1) + r_mp < 16384` — so that the low bits do not carry into
  bit 14 before the month is over, i.e. `t >> 14` does not step late.

Both are satisfied, with room to spare in most months:

| `mp` | month     | `month_start` | `r_mp` | slack to the next carry |
| ---: | --------- | ------------: | -----: | ----------------------: |
|    0 | March     |             0 |    331 |                   **3** |
|    1 | April     |            31 |    532 |                     337 |
|    2 | May       |            61 |    198 |                     136 |
|    3 | June      |            92 |    399 |                     470 |
|    4 | July      |           122 |     65 |                     269 |
|    5 | August    |           153 |    266 |                      68 |
|    6 | September |           184 |    467 |                     402 |
|    7 | October   |           214 |    133 |                     201 |
|    8 | November  |           245 |    334 |                     535 |
|    9 | December  |           275 |      0 |                     334 |
|   10 | January   |           306 |    201 |                     133 |
|   11 | February  |           337 |    402 |                    1002 |

March is the tight one: on March 31st the product is `535·30 + 331 == 16381`,
three short of the carry into bit 14. Three units of slack in the tightest month
is why the guard in the test file checks every `doy` rather than sampling.

**Where it breaks.** The identity is exact for `doy ∈ [0, 365]`, which is
precisely the range `split_century` produces, and the first `doy` at which it
diverges is **428**. That is 63 days of margin, and the constant
`MONTH_MAGIC_FIRST_BAD_DOY = 428` is asserted to be where the divergence really
begins, so that anything widening the `doy` range fails the build with a message
naming the real bound rather than quietly producing wrong dates inside the
margin.

## 6. The collapsed leap test

```cpp
VTZ_INLINE constexpr bool march_year_is_leap( u32 c, u32 z ) noexcept {
    u32 w = z == 99 ? c : z;
    return ( w % 4 ) == 3;
}
```

"Does March-based year `z` of century `c` end with a February 29th?" That
February belongs to the _following_ calendar year, so the question is about
`Y + 1`. Writing the calendar year as `Y = 100c + z` (up to the multiple of 400
that `CENTURY_ORIGIN` contributes, which no `% 4` or `% 400` test can see):

- **`z < 99`.** The question is whether `Y + 1 = 100c + (z+1)` is a leap year.
  `z + 1 ∈ [1, 99]`, so `Y + 1` is not a multiple of 100 and both exception
  clauses are dead. `100c` is a multiple of 4, so `Y + 1` is a leap year iff
  `(z + 1) % 4 == 0`, i.e. iff `z % 4 == 3`.
- **`z == 99`.** Then `Y + 1 = 100(c + 1)`, which _is_ a multiple of 100, so it
  is a leap year iff it is a multiple of 400 — iff `(c + 1) % 4 == 0`, i.e. iff
  `c % 4 == 3`.

Both branches end in the same test, which is why the `+ 1` never appears in the
code: `(w + 1) % 4 == 0` and `w % 4 == 3` are the same predicate, and the latter
is one instruction cheaper.

So `z == 99` is the only case in which `c` is consulted at all, and even then
only its low two bits. The three-clause Gregorian test never appears, and neither
does the absolute year. Because both sides depend on `c` only through `c % 4`, a
four-century sweep is exhaustive for this function, and `march_year_is_leap_ok`
in the test file is exactly that sweep, against the real `is_leap`.

## 7. `add_years_impl`

Take the March-based framing seriously and year addition is almost trivial.
Every date sits at a fixed offset `doy` from its own March 1st; a leap day is
appended to the _end_ of a March-based year, never inserted into the middle. So
adding N years moves the date by `365 · N` plus the number of leap days crossed,
and the only work is counting those.

```cpp
template<bool Clamp>
VTZ_INLINE constexpr sys_days_t add_years_impl(
    sys_days_t days, i32 years ) noexcept {
    auto parts = split_century( days );
    u32  c     = parts.c;
    u32  z     = parts.z; // Year of century - [0, 99]

    // Move to the target year, carrying whole centuries out
    i32 total = i32( z + u32( years ) );
    i32 cq    = math::div_floor<100>( total );
    u32 z2    = u32( total - 100 * cq ); // [0, 99]

    // Same leap-day count as add_months_impl
    i32 leap_delta
        = 24 * cq + ( ( i32( c & 3 ) + cq ) >> 2 ) + i32( z2 >> 2 );

    // 365 days per year, plus the leap days crossed, minus where we
    // started within the century
    u32 delta = 365u * u32( years ) + u32( leap_delta ) - ( z >> 2 );

    if constexpr( Clamp )
    {
        delta -= u32( parts.doy == 365
                      && !march_year_is_leap( c + u32( cq ), z2 ) );
    }

    return sys_days_t( u32( days ) + delta );
}
```

Two of those lines are unsigned where you might expect `i32`, deliberately:
`z + u32( years )` and the `delta` chain are the only places the arithmetic can
leave `i32`, and unsigned overflow is defined wrapping where signed overflow is
UB. That is what makes the domain exactly "the answer is representable" rather
than something narrower — see [§11](#11-the-proof).

### 7.1 Deriving `delta`

Let the source be March-based year `z` of century `c`, and the target be year
`z2` of century `c + cq`. Both dates have the same `doy`. Using
`century_start` and the within-century year offset from
[§4](#4-split_century):

$$
\begin{aligned}
\texttt{days} &= \texttt{century\_start}(c) + 365z + \lfloor z/4 \rfloor + \texttt{doy} - \texttt{ORIGIN} \\
\texttt{target} &= \texttt{century\_start}(c + cq) + 365z_2 + \lfloor z_2/4 \rfloor + \texttt{doy} - \texttt{ORIGIN}
\end{aligned}
$$

`doy` and `ORIGIN` cancel, leaving

$$\delta = 36524\,cq + \left(\left\lfloor \tfrac{c+cq}{4} \right\rfloor - \left\lfloor \tfrac{c}{4} \right\rfloor\right) + 365(z_2 - z) + \left\lfloor \tfrac{z_2}{4} \right\rfloor - \left\lfloor \tfrac{z}{4} \right\rfloor$$

Two rewrites finish it.

**The 36500 absorbs into the year count.** `z2 - z == years - 100·cq`, so
`365(z2 - z) == 365·years - 36500·cq`, and `36524·cq - 36500·cq == 24·cq`. That
is the `24 * cq` term: 24 leap days per century from the divisible-by-4 rule
alone.

**The century's own leap days depend only on `c % 4`.** Write `c = 4q + r` with
`r = c & 3`. Then `⌊(c+cq)/4⌋ - ⌊c/4⌋ == ⌊(r + cq)/4⌋`, which is
`( i32( c & 3 ) + cq ) >> 2` — an arithmetic right shift, which is floor division
by 4 for negative values too, so backwards shifts need no special case. This is
the step where the absolute century, and with it the era, drops out of the
computation entirely.

Substituting both gives exactly the code:

$$\delta = 365 \cdot \texttt{years} + \underbrace{24\,cq + \left\lfloor \tfrac{(c \,\&\, 3) + cq}{4} \right\rfloor + \left\lfloor \tfrac{z_2}{4} \right\rfloor}_{\texttt{leap\_delta}} - \left\lfloor \tfrac{z}{4} \right\rfloor$$

### 7.2 Clamping

`doy == 365` happens on February 29th and nowhere else, because February is the
last month of a March-based year. If the target March-based year has no February
29th, it is only 365 days long — days 0 through 364 — and the unclamped result
lands on day 365 of it, which is March 1st of the year after. Subtracting one
gives February 28th. That is the whole `Clamp` branch:

```cpp
delta -= u32( parts.doy == 365
              && !march_year_is_leap( c + u32( cq ), z2 ) );
```

The unclamped variant is documented to roll over to March 1st, so it simply
leaves the term off.

## 8. `add_months_impl`

Month addition needs one more thing than year addition: the source month has to
be decoded (so the target month is known), and the day-of-month has to be
preserved rather than the day-of-year. The month magic from
[§5](#5-the-month-magic-535-331-14) supplies both from a single product.

```cpp
template<bool Clamp>
VTZ_INLINE constexpr sys_days_t add_months_impl(
    sys_days_t days, i32 months ) noexcept {
    auto parts = split_century( days );
    u32  c     = parts.c;
    u32  z     = parts.z;   // Year of century - [0, 99]
    u32  doy   = parts.doy; // Day of the March-based year - [0, 365]

    u32 t   = MONTH_MAGIC_MUL * doy + MONTH_MAGIC_ADD;
    u32 mp  = t >> MONTH_MAGIC_SHIFT; // [0, 11], 0 is March
    u32 dom = ( t & MONTH_MAGIC_MASK ) / MONTH_MAGIC_MUL;

    i32 total = i32( ( 12 * z + mp ) + u32( months ) );
    i32 yq    = math::div_floor<12>( total );
    i32 cq    = math::div_floor<1200>( total );
    u32 mp2   = u32( total - 12 * yq ); // [0, 11]
    u32 z2    = u32( yq - 100 * cq );   // [0, 99]

    i32 leap_delta
        = 24 * cq + ( ( i32( c & 3 ) + cq ) >> 2 ) + i32( z2 >> 2 );

    u32 delta = 365u * u32( yq ) + u32( leap_delta ) + month_start( mp2 )
                - parts.doc + dom;
    /* ... Clamp ... */
    return sys_days_t( u32( days ) + delta );
}
```

### 8.1 One value carries the whole target

`total` is the count of March-based months from the start of century `c` to the
target. `12 * z + mp ≤ 12·99 + 11 == 1199`, so it is small, and adding `months`
gives a single quantity from which everything else is a division:

- `yq = ⌊total / 12⌋` — the target year, as an offset from the century start. It
  may be negative, or above 99; that is fine and expected.
- `cq = ⌊total / 1200⌋` — the number of centuries crossed. By the nested-floor
  identity `⌊⌊x/12⌋/100⌋ == ⌊x/1200⌋`, this is exactly `⌊yq / 100⌋`, so the two
  divisions are consistent by construction rather than by fixup.
- `mp2 = total - 12·yq ∈ [0, 11]` and `z2 = yq - 100·cq ∈ [0, 99]`.

Floor division, via `math::div_floor`, because `total` goes negative for large
negative offsets and truncating division would round the wrong way there.

### 8.2 Deriving `delta`

Same shape as [§7.1](#71-deriving-delta), with `month_start` and `dom` in place
of `doy`, and `doc` used directly on the source side since the source day-of-year
no longer cancels:

$$
\begin{aligned}
\texttt{days} &= \texttt{century\_start}(c) + \texttt{doc} - \texttt{ORIGIN} \\
\texttt{target} &= \texttt{century\_start}(c+cq) + 365z_2 + \lfloor z_2/4 \rfloor + \texttt{month\_start}(mp_2) + \texttt{dom} - \texttt{ORIGIN}
\end{aligned}
$$

Subtract, then apply the same two rewrites — except that this time the "365 per
year" term absorbs into `yq` rather than `years`, because `yq = 100·cq + z2`
already counts the years from the century start:

$$365 \cdot yq = 36500\,cq + 365 z_2$$

which is what lets a single `365 * yq` stand in for both the whole centuries and
the leftover years. The result is the code:

$$\delta = 365\,yq + \texttt{leap\_delta} + \texttt{month\_start}(mp_2) - \texttt{doc} + \texttt{dom}$$

`leap_delta` is character-for-character the same expression as in
`add_years_impl`, which is why the two functions share a comment rather than a
helper.

### 8.3 Rollover comes out for free

`civil_add_months` is documented to roll over: January 31st plus one month is
March 3rd, not February 28th. Nothing in the formula implements that. It happens
because `delta` is built from `month_start(mp2) + dom` and never normalises the
result back into a month, so when `dom` exceeds the target month's length the sum
simply runs past the month's end.

That is exactly the behaviour of `resolve_civil( y, m, d )` when `d` is out of
range, which is what the reference implementation in
[§2](#2-why-not-decode-shift-and-re-encode) does, so the two agree without either
one special-casing it. Worked through: for 2025-01-31, `doy == 336`
(`month_start(10) + 30`); the target is February, `mp2 == 11`, and
`month_start(11) + dom == 337 + 30 == 367`. The March-based year 2024 runs
Mar 1 2024 → Feb 28 2025 and so has 365 days, numbered 0–364. Day 367 is two
days past the end: March 3rd, 2025. In a leap year the same arithmetic gives
March 2nd, because the year is 366 days long.

## 9. Clamping, and the packed month-length table

The clamped variant needs the last day of the target month:

```cpp
u32 bits = MARCH_MONTH_LAST_BITS
           - ( u32( march_year_is_leap( c + u32( cq ), z2 ) ) << ( 2 * 11 ) );
u32 last = 30 - ( ( bits >> ( 2 * mp2 ) ) & 3 );
delta -= dom > last ? dom - last : 0u;
```

`MARCH_MONTH_LAST_BITS == 12652612` is a 24-bit table, two bits per month, `mp =
0` (March) in the low bits. Each field holds `30 - last`, where `last` is the
0-based last day of the month, so the field is 0 for a 31-day month, 1 for a
30-day month, and 3 for a 28-day February. Two bits suffice because month lengths
span only 28–31 days.

The March-based ordering pays off a third time here. February is the only month
whose length depends on the leap rule, and in a March-based year it is the _last_
month — so its field is the _top_ one, and the leap adjustment is a single
subtraction of `1 << 22` applied to the whole word. The other eleven months
cannot be affected by it, so they cost nothing:

| `mp`   | 0 (Mar) | 1 (Apr) | 2   | 3   | 4   | 5   | 6   | 7   | 8   | 9   | 10 (Jan) | 11 (Feb)        |
| ------ | ------- | ------- | --- | --- | --- | --- | --- | --- | --- | --- | -------- | --------------- |
| field  | 0       | 1       | 0   | 1   | 0   | 0   | 1   | 0   | 1   | 0   | 0        | 3 → 2 if leap   |
| `last` | 30      | 29      | 30  | 29  | 30  | 30  | 29  | 30  | 29  | 30  | 30       | 27 → 28 if leap |

Had February been left in its calendar position, the leap bump would have landed
in the middle of the word and every month after it would have needed a shifted
correction.

## 10. The domain

The four `civil_add_*` functions are correct on **every** `(days, k)` pair in
`i32 × i32` whose true answer is representable in `sys_days_t`. There is no
further condition — not on `days`, not on `k`, not on the size of the shift:

$$\texttt{impl}(d, k) = \texttt{ideal}(d, k) \iff \texttt{ideal}(d, k) \in [-2^{31},\, 2^{31})$$

where `ideal` means: decode `d` to a Gregorian date, shift the month or year,
keep the day of the month (clamping it, in the `_clamped` variants), and
re-encode. [§11](#11-the-proof) proves this.

Two things make that stronger than it looks. The _intermediate_ arithmetic never
constrains the caller, even though a shift of a few million years produces a
`delta` that does not fit in `i32` at all; and it is not a claim about what
compilers happen to do, since there is no undefined behaviour anywhere in the four
functions, at any input.

Two caveats, neither of which weakens the above:

**The decoders are narrower.** `to_civil`, `to_civil0` and `to_civil_year_doy`
compute `days += 719468` in `i32`, so they overflow — signed-overflow UB, not a
wrap — above `INT32_MAX - 719468 == 2146764179`. So a caller who _formats_ a
result is bounded more tightly than one who only does arithmetic. That is a
pre-existing property of the era-split decoders and out of scope here.

**Representability is the caller's problem.** Where the true answer does not fit,
the functions return the value congruent to it modulo 2³², which is a well-defined
but wrong date. The bounds, for reference:

|                    | `ideal` is representable only for     |
| ------------------ | ------------------------------------- |
| `civil_add_months` | `months` in `[-141110652, 141110652]` |
| `civil_add_years`  | `years` in `[-11759221, 11759221]`    |

Those are the widest possible: they are attained from `days == INT32_MIN` shifting
up, and `days == INT32_MAX` shifting down. For any particular `days` the usable
range is narrower, and the honest test is `ideal ∈ i32` rather than a bound on `k`.

## 11. The proof

The claim in [§10](#10-the-domain) does not follow from testing: the input space
is 2⁶⁴ pairs. It follows from three facts, each of which is either a short
argument or a finite check.

### 11.1 Fact 1: everything after `total` is a ring homomorphism

Look at what the code does from `total` onwards. `yq`, `cq`, `mp2`, `z2` come out
of `total` by division; after that, every operation is `+`, `−` or `×`:

```
delta  = 365·yq + leap_delta + month_start(mp2) − doc + dom
result = days + delta
```

and the clamp subtracts a value in `[0, 3]`. Addition, subtraction and
multiplication on `u32` are the ring operations of ℤ/2³², and the C++ standard
defines them to wrap. So the map ℤ → ℤ/2³² commutes with the whole tail, and

$$\texttt{result} \equiv \texttt{ideal}(d,k) \pmod{2^{32}}$$

_identically_ — for every `(d, k)`, whether or not anything overflowed, and
whether or not the answer is representable. Since `result` is an `i32` and the
congruence class of `ideal` has exactly one representative in `[-2³¹, 2³¹)`, the
two are **equal** precisely when `ideal` is representable. That is the claim.

This is the part that the unsigned rewrite bought. The same code in `i32` computes
the same bits on every real target, but `365 * yq` overflowing a signed type is UB,
so the standard grants nothing and an optimiser is entitled to assume it cannot
happen. In `u32` the wrap is normative.

### 11.2 Fact 2: `total` is exact wherever the claim applies

Fact 1 needs `yq`, `cq`, `mp2` and `z2` to be the true quotients, because
division is the one operation that does _not_ commute with reduction mod 2³².
They are, provided `total` itself does not overflow — and it cannot, in the
region where the claim says anything:

- `total = (12z + mp) + months` with `12z + mp ≤ 12·99 + 11 = 1199`, so it
  overflows only for `months ≥ INT32_MAX − 1199 = 2147482448`. It never
  underflows, since `12z + mp ≥ 0`. In `add_years_impl` the addend is `z ≤ 99`,
  so the threshold is `INT32_MAX − 99`.
- But `ideal` is representable only for `|months| ≤ 141110652`
  ([§10](#10-the-domain)).

$$\frac{2147482448}{141110652} = 15.2\times \text{ margin (months)}, \qquad \frac{2147483548}{11759221} = 183\times \text{ (years)}$$

So the region where a wrapped `total` could corrupt the divisions and the region
where the claim has any content are disjoint, by better than an order of
magnitude. This is testable directly and it comes out empty: sweeping `|k|` near
2³¹ against the reference yields **zero** pairs with a representable answer.

`leap_delta` needs no such argument — it is provably overflow-free.
`|cq| ≤ INT32_MAX / 1200`, so `24·cq + ⌊((c&3)+cq)/4⌋ + ⌊z2/4⌋` stays under 2²⁶.

### 11.3 Fact 3: the algebra is finitely checkable

Facts 1 and 2 reduce the claim to: _`delta` is the true delta whenever no
intermediate wrapped_. That is a statement about integer arithmetic, and it has
two exact symmetries which make it finite.

**Symmetry A — `days` enters only through `(c & 3, doc)`.** By inspection of the
code: `c` appears only as `c & 3` and as `(c + cq) & 3` inside
`march_year_is_leap`; and `z`, `doy`, `mp`, `dom` are all functions of `doc`
alone. So

$$\texttt{delta}(d, k) = f(c \bmod 4,\ \texttt{doc},\ k)$$

and the number of classes is the total length of four consecutive centuries:

$$3 \times 36524 + 36525 = 146097$$

— one class per day of an era, which is Hinnant's "you only need to debug a
single era" surviving the century split intact.

**Symmetry B — one era of shift adds exactly one era of days.** Take `k → k +
4800` months (= 400 years). Then `total → total + 4800`, so `yq → yq + 400` and
`cq → cq + 4`, while `mp2` and `z2` are unchanged. Substituting:

$$\Delta = 365 \cdot 400 + 24 \cdot 4 + 1 = 146000 + 96 + 1 = 146097$$

where the trailing `+ 1` is `⌊((c & 3) + cq + 4)/4⌋ - ⌊((c & 3) + cq)/4⌋`. So
`delta(k + 4800) = delta(k) + 146097` exactly, and the calendar has the same
period, so it suffices to check one residue of `k` modulo 4800. For
`add_years_impl` the period is 400 years by the same argument.

**Therefore the whole claim rests on a finite check** of

$$146097 \times 4800 = 701{,}265{,}600 \text{ classes (months)}, \qquad 146097 \times 400 = 58{,}438{,}800 \text{ (years)}$$

which is what `TEST( vtz, civil_reduced_space )` enumerates — every class, both
clamped and unclamped, against the `i64` reference, in under a second.

### 11.4 What this is and is not

Composing: Fact 3 establishes the arithmetic, Fact 2 licenses Fact 1, and Fact 1
converts the arithmetic into the machine-level claim. The gaps worth naming:

- **The reduction is argued in prose, not machine-checked.** Symmetries A and B
  are each a few lines of algebra over the code as written, and the test file
  checks both empirically as well, but a transcription error in the reasoning
  would not be caught by the test that the reasoning justifies. Formalising just
  those two lemmas would close this, and would be a much smaller job than
  formalising the function.
- **Fact 1 assumes the compiler implements `u32` as ℤ/2³²**, which the standard
  requires, and that the narrowing conversions `u32 → i32` are modular. The
  latter is implementation-defined before C++20 and normative from C++20; it is
  never undefined. vtz targets C++17, so on a hypothetical non-two's-complement
  target the _sign_ of an out-of-range result could differ — but such a target
  cannot represent `sys_days_t` as assumed anywhere else in the file either.
- **The `i64` reference is itself unverified code.** It is short, independent of
  `civil.h`, and derived from a published algorithm, but it is not a specification
  in any formal sense.

What the composition does buy, over the fuzzing in
[§12](#12-yes-but-how-do-you-know-this-all-really-works), is coverage of a 2⁶⁴
space by a 7×10⁸ check, and a statement about the _boundary_ — where `delta`
exceeds `i32` — that no amount of sampling in the interior would reach.

## 12. Yes, but how do you know this all really works?

Borrowing Hinnant's [section title][yesbut], because the answer has the same
shape: an exhaustive check where one is possible, and an independent reference
implementation everywhere else.

[yesbut]: https://howardhinnant.github.io/date_algorithms.html#Yes,%20but%20how%20do%20you%20know%20this%20all%20really%20works?

### 12.1 Compile-time guards on the constants

Every magic constant is checked by `static_assert`, so a bad one fails the build
rather than a test run. The guards live in
[`etc/test/test_impl/test_civil.cpp`](../etc/test/test_impl/test_civil.cpp)
rather than in `civil.h`, so the checking machinery is not recompiled in every
translation unit that includes the public header. Several are exhaustive rather
than sampled, and the derived constants track the code instead of needing a hand
recomputation:

- **`century_magic_ok`** checks eight points, and eight is exhaustive. Only step
  points need checking, since both the exact quotient and the magic form are
  non-decreasing in `n`, so agreeing wherever the exact value steps forces
  agreement in between. And only the _first and last_ step point of each residue
  class `m % 4` is needed: writing `m = 4q + r` makes `century_start` exactly
  `146097q + 36524r`, so for fixed `r` the floored quantity is affine in `q`, and
  an affine function stays inside a half-open interval over a range iff it does at
  both ends. Four residues × two ends settles every reachable century. A
  brute-force sweep over all of them agrees; it is not used because it costs
  ~0.4 s per TU and exceeds clang's constexpr step limit.
- **`month_magic_ok`** is genuinely exhaustive: it checks both identities of
  [§5](#5-the-month-magic-535-331-14) for every `doy` in `[0, 365]`, against the
  `(5·doy+2)/153` and `month_start` expressions they replace. A companion
  assertion pins `MONTH_MAGIC_FIRST_BAD_DOY = 428` as the place where the
  identity _actually_ first fails, so the 63-day margin is a checked fact rather
  than a claim in a comment.
- **`split_century_bound_ok`** checks that `doy ≤ 365`, which is what the month
  magic depends on. One 4-year cycle covers every case, because `doy` depends on
  `doc` only through `doc % 1461` and a century is a whole number of such cycles.
  It requires `worst == MAX_DOY` with equality, so a change that lowered the real
  maximum would show up rather than leaving a stale constant behind.
- **`march_month_last_bits_ok`** checks all twelve fields of the packed table
  against the month lengths implied by `month_start`, rather than against a
  hand-written list.
- **`march_year_is_leap_ok`** checks the collapsed leap test against the real
  three-clause `is_leap` over four centuries, which is exhaustive for it since
  both sides depend on `c` only through `c % 4`. This one earns its keep: the
  function has been rewritten once, from `(w + 1) % 4 == 0` to `w % 4 == 3`, and
  the guard is what makes that a safe edit rather than a hopeful one.
- **`MAX_CENTURY` is derived, not written out** — `century_of( MAX_SHIFTED_DAYS )`
  — so it follows `split_century` instead of needing a hand recomputation each
  time the shift or its width changes. Two assertions then pin it as the last
  century an `i32` day count reaches: it has begun by `MAX_SHIFTED_DAYS`, and the
  next one has not.
- `CENTURY_ORIGIN`'s two properties from
  [§3.1](#31-an-unsigned-day-count) are asserted at its declaration in `civil.h`,
  along with its minimality and the non-overflow of `(n + 1) * CENTURY_MAGIC`.
  There is also a `static_assert` that `MAX_SHIFTED_DAYS > 4294967295` — the rare
  assertion whose _failure_ would be good news, since it would mean the shifted
  count could go back to being 32-bit.

### 12.2 Dense sweeps

`TEST( vtz, civil_arithmetic )` and the two `*_clamped` tests walk every day from
1570-01-01 to 2370-12-31 and, for each, every offset `k ∈ [-100, 100]`, comparing
against `resolve_civil` at the shifted `(y, m, d)`. That is 292,559 days × 201
offsets, or ~58.8 million cases per function, and it covers 8 century
boundaries, 2 divisible-by-400 years (1600, 2000) and 6 divisible-by-100
non-leap years.

### 12.3 Fuzzing against an independent reference

`TEST( vtz, civil_arithmetic_fuzz )` runs 2,000,000 cases against reference
implementations that work entirely in `i64` and call nothing from `civil.h`, so
they remain valid however the implementation is rewritten and cannot themselves
overflow at the edges of the range being probed. The generator is seeded with a
fixed value, so failures reproduce.

The first million are uniform over the whole usable day range, which is
overwhelmingly far-flung dates — the place where a century or era boundary bug
shows up. The second million are biased towards the inputs most likely to be
wrong: the year is snapped onto a divisible-by-4, -100 or -400 boundary, the
month onto February, and the day onto the end of the month where clamping bites,
with a small nudge so the neighbours get hit too.

### 12.4 The range ends

`TEST( vtz, civil_arithmetic_range_ends )` walks every day of the window that the
`u32` shifted count used to lose — all 131,235 of them — plus 1000 days at each
extreme of `i32`, against the `i64` reference, skipping pairs whose true answer is
not representable. This is the regression test for
[§3.1](#31-an-unsigned-day-count); on the old code about 55% of it failed.

It is deliberately separate from the fuzzing, because the fuzz bounds also encode
the _decoders'_ narrower range and this test goes past it. Nothing in it may call
`to_civil` or `resolve_civil`.

### 12.5 The reduced space

`TEST( vtz, civil_reduced_space )` is the one that discharges
[§11.3](#113-fact-3-the-algebra-is-finitely-checkable). It enumerates every
`(c mod 4, doc, k mod 4800)` class — 701,265,600 for the month functions,
58,438,800 for the years — against the `i64` reference, clamped and unclamped.
Together with the two symmetries it is exhaustive over the algebra, which is why
it is the strongest single check in the file despite touching only a 7×10⁸ slice
of a 2⁶⁴ space.

It also checks the two symmetries themselves, so the reduction is not merely
assumed: that `delta` is invariant across centuries with equal `c % 4`, and that
`delta(k + one era) − delta(k)` is exactly 146097.

It is not a slower restatement of the other tests, and that is measurable. Every
test that predates it uses `|k| ≤ 100`, so none of them ever reaches `|cq| ≥ 2` —
a shift across more than one century boundary. Adding a spurious `+ cq / 4` to
`leap_delta` is therefore invisible below `|cq| = 4`, and:

| mutation                                 | every pre-existing test | `civil_reduced_space` |
| ---------------------------------------- | ----------------------- | --------------------- |
| spurious `+ cq / 4` in `add_months_impl` | passes                  | **fails**             |
| spurious `+ cq / 4` in `add_years_impl`  | passes                  | **fails**             |

1,549,796,978 comparisons, ~800 ms on eight threads.

## 13. What it costs

Instruction counts for each function compiled standalone with
`[[gnu::flatten, gnu::noinline]]`, against the equivalent written as
`to_civil` → shift → `resolve_civil`. Apple clang 21, `-O2`:

| Function                   | Century split (arm64) | Century split (x86-64) | Decode + re-encode (arm64) |
| -------------------------- | --------------------: | ---------------------: | -------------------------: |
| `civil_add_years`          |                    40 |                     46 |                        106 |
| `civil_add_months`         |                    66 |                     73 |                        119 |
| `civil_add_years_clamped`  |                    50 |                     61 |                          — |
| `civil_add_months_clamped` |                    83 |                    100 |                          — |

Instruction counts are a proxy, not a measurement; `etc/bench/src/bench_vtz_civil.cpp`
has the actual benchmarks. Structurally, two things are being avoided. The
re-encode is gone entirely, and with it the `yoe/4 - yoe/100` leap-day
accounting and the floor-division of the year by 400. And the four-division
`yoe` chain is replaced by one widening multiply plus one division by 1461, since
inside a century there are no leap exceptions to correct for.

The clamped variants cost ten to seventeen instructions more than the plain ones
on arm64, fifteen to twenty-seven on x86-64. That is the packed-table lookup and
the conditional step-back.

**The 64-bit shifted count costs exactly one instruction**, on both
architectures — two for `civil_add_years_clamped` on x86-64. On arm64 the sign
extension folds into the add (`add x8, x8, w0, sxtw`) and the widening
multiply-add just becomes a full one; the single extra instruction is
`century_start`'s `m >> 2` losing its free ride as a shifted operand on the
`sub`. The unsigned delta chain of [§11.1](#111-fact-1-everything-after-total-is-a-ring-homomorphism)
costs **nothing** over the signed version it replaced — same instruction count on
both architectures, because it is the same machine arithmetic with the undefined
behaviour removed rather than different arithmetic.

## Where the code lives

All in [`include/impl/vtz/civil.h`](../include/impl/vtz/civil.h). Line numbers
are as of this writing; the names are stable.

|                                            |                                                                                         |
| ------------------------------------------ | --------------------------------------------------------------------------------------- |
| `month_start`                              | [civil.h:32](../include/impl/vtz/civil.h#L32)                                           |
| `century_parts`                            | [civil.h:49](../include/impl/vtz/civil.h#L49)                                           |
| `CENTURY_ORIGIN`, and its three guards     | [civil.h:71](../include/impl/vtz/civil.h#L71)                                           |
| `DAYS_PER_CENTURY`                         | [civil.h:75](../include/impl/vtz/civil.h#L75)                                           |
| `MAX_SHIFTED_DAYS`                         | [civil.h:93](../include/impl/vtz/civil.h#L93)                                           |
| `CENTURY_MAGIC`, `CENTURY_SHIFT`           | [civil.h:98](../include/impl/vtz/civil.h#L98)                                           |
| `century_start`                            | [civil.h:112](../include/impl/vtz/civil.h#L112)                                         |
| `century_of`                               | [civil.h:118](../include/impl/vtz/civil.h#L118)                                         |
| `MARCH_MONTH_LAST_BITS`                    | [civil.h:126](../include/impl/vtz/civil.h#L126)                                         |
| `MONTH_MAGIC_*`                            | [civil.h:135](../include/impl/vtz/civil.h#L135)                                         |
| `split_century`                            | [civil.h:148](../include/impl/vtz/civil.h#L148)                                         |
| `march_year_is_leap`                       | [civil.h:175](../include/impl/vtz/civil.h#L175)                                         |
| `add_months_impl`                          | [civil.h:196](../include/impl/vtz/civil.h#L196)                                         |
| `add_years_impl`                           | [civil.h:280](../include/impl/vtz/civil.h#L280)                                         |
| `civil_add_months` / `_years` / `_clamped` | [civil.h:724](../include/impl/vtz/civil.h#L724)–[755](../include/impl/vtz/civil.h#L755) |
| `is_leap`                                  | [civil.h:502](../include/impl/vtz/civil.h#L502)                                         |
| `to_civil` (era split, unchanged)          | [civil.h:625](../include/impl/vtz/civil.h#L625)                                         |
| `resolve_civil` (era split, unchanged)     | [civil.h:572](../include/impl/vtz/civil.h#L572)                                         |
| `math::div_floor`                          | [math.h:51](../include/api/vtz/impl/math.h#L51)                                         |

Tests, in [`etc/test/test_impl/test_civil.cpp`](../etc/test/test_impl/test_civil.cpp):

|                                  |                                                                                                                  |
| -------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `i64` reference implementations  | [test_civil.cpp:101](../etc/test/test_impl/test_civil.cpp#L101)                                                  |
| Compile-time guards              | [test_civil.cpp:208](../etc/test/test_impl/test_civil.cpp#L208)–[381](../etc/test/test_impl/test_civil.cpp#L381) |
| `march_year_is_leap_ok`          | [test_civil.cpp:364](../etc/test/test_impl/test_civil.cpp#L364)                                                  |
| `civil_arithmetic` (dense sweep) | [test_civil.cpp:671](../etc/test/test_impl/test_civil.cpp#L671)                                                  |
| `civil_add_months_clamped`       | [test_civil.cpp:739](../etc/test/test_impl/test_civil.cpp#L739)                                                  |
| `civil_add_years_clamped`        | [test_civil.cpp:807](../etc/test/test_impl/test_civil.cpp#L807)                                                  |
| `civil_arithmetic_fuzz`          | [test_civil.cpp:871](../etc/test/test_impl/test_civil.cpp#L871)                                                  |
| `civil_arithmetic_range_ends`    | [test_civil.cpp:966](../etc/test/test_impl/test_civil.cpp#L966)                                                  |
| `civil_reduced_space`            | [test_civil.cpp:1044](../etc/test/test_impl/test_civil.cpp#L1044)                                                |

## References

- Howard Hinnant, [_`chrono`-Compatible Low-Level Date Algorithms_][hinnant],
  2021-09-01. The source of the epoch shift, the March-based year, the era
  decomposition, and the `(153·mp + 2)/5` month table. `to_civil`,
  `to_civil0`, `resolve_civil` and `resolve_civil0` in `civil.h` are derived
  from `civil_from_days` and `days_from_civil` and carry that attribution.
- Cassio Neri and Lorenz Schneider, [_Euclidean affine functions and their
  application to calendar algorithms_][ns], 2021. **The decode in
  [§3](#3-the-century-as-the-working-window)–[§5](#5-the-month-magic-535-331-14)
  is theirs.** Their `to_date` cuts on the century, recovers the year from the day
  of the century and the month from the day of the year — the same `4x + 3` affine
  form at each level, each reusing its own remainder for the level below — and
  takes the month and the day within it from the high and low bits of one product.
  `split_century` and the month magic are that algorithm. The correspondence is
  exact and not merely similar: `doc` here and their `N_C` are the same quantity,
  because `146097 ≡ 1 (mod 4)` makes the `+ 3` padding agree at every level. Their
  reference implementation is `algorithms/neri_schneider.hpp` in
  [cassioneri/eaf][eaf], and libstdc++ implements `<chrono>`'s civil conversions
  from the same propositions, citing them by name. This is the state of the art,
  not an optimisation local to vtz.

  Two things here are not in that work. Their century step holds `4N + 3` in a
  `u32`, which bounds the input to `N < 2³⁰`, roughly a quarter of the `i32` day
  range; forming it in 64 bits costs one instruction and buys the rest
  ([§3.1](#31-an-unsigned-day-count)). And their file defines only `to_date` and
  `to_rata_die`, so the day-delta formulation for month and year addition —
  together with the observation that a whole-month shift needs the century only
  through `c mod 4`, which is what makes the delta computable without a decode —
  is what this document is actually about. vtz reached the shared structure
  without reference to the paper, which says more about how tightly the calendar
  constrains the answer than about vtz.

  A century-split variant of the full Neri–Schneider decode, for `to_civil`
  rather than for arithmetic, is a separate and unmerged line of work.
- Torbjörn Granlund and Peter L. Montgomery, _Division by invariant integers
  using multiplication_, PLDI '94. The general form of which `CENTURY_MAGIC` and
  the month magic are instances.

[eaf]: https://github.com/cassioneri/eaf
