# Civil month and year arithmetic

How `civil_add_months`, `civil_add_years`, and their clamped variants work.

This is a companion to Howard Hinnant's [*chrono-Compatible Low-Level Date
Algorithms*][hinnant], and it assumes you have read that page. Everything about
the epoch shift, the March-based year, the era decomposition, and the
`(153*mp + 2)/5` month table is taken from there and is not re-derived here. What
*is* derived here is the part that differs: the four `civil_add_*` functions do
not decode to a `(y, m, d)` triple and re-encode. They compute a day *delta* and
add it, and to do that they cut the day count on the **century** rather than on
the 400-year era.

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
- [10. Ranges and failure modes](#10-ranges-and-failure-modes)
- [11. Yes, but how do you know this all really works?](#11-yes-but-how-do-you-know-this-all-really-works)
- [12. What it costs](#12-what-it-costs)
- [Where the code lives](#where-the-code-lives)
- [References](#references)

---

## 1. What is borrowed, and what is new

Borrowed from [`civil_from_days`][cfd] and [`days_from_civil`][dfc] without
change:

- **The epoch shift.** `719468` moves day 0 from 1970-01-01 to 0000-03-01.
- **The March-based year.** Months are renumbered so that March is month 0 and
  February is month 11. Hinnant's reason is the whole reason this document has
  anything easy to say: it "puts the leap day, Feb. 29 as the last day of the
  year". A leap day is therefore *appended to a year* and never *inserted into
  one*.
- **The month table.** `month_start( mp ) == ( 153 * mp + 2 ) / 5` is the number
  of days from March 1st to the first of March-based month `mp`, derived in
  [*Computing day-of-year from month and day-of-month*][doyfm]. vtz spells it out
  as a named function in `civil.h`, and both `to_civil` and `resolve_civil` use
  it inline.
- **The names.** `doe`, `yoe`, `doy`, `mp`, `z` mean what they mean on Hinnant's
  page.

[cfd]: https://howardhinnant.github.io/date_algorithms.html#civil_from_days
[dfc]: https://howardhinnant.github.io/date_algorithms.html#days_from_civil
[doyfm]: https://howardhinnant.github.io/date_algorithms.html#Computing%20day-of-year%20from%20month%20and%20day-of-month
[mfdoy]: https://howardhinnant.github.io/date_algorithms.html#Computing%20month%20from%20day-of-year

New here, and the subject of the rest of this document:

- **The window is a century, not an era.** So `yoe`/`doe` become `z` (year of
  century, `[0, 99]`) and `doc` (day of century, `[0, 36524]`).
- **Nothing is re-encoded.** The four public functions return `days + delta`.
- **The absolute year never appears.** Only differences are needed, and the
  century index enters every formula through its low two bits alone.

The last point deserves emphasis, because it is what makes the century window
legal. A century is *not* a self-similar unit of the Gregorian calendar the way
an era is — 36524 days is the length of a century only three times in four. The
century works as a window here because these functions never need to know which
century they are in, only how many century boundaries a shift crosses.

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

But a month or year shift does not need an absolute year. It needs to know how
many days lie between where you are and where you are going, and the calendar's
irregularities are the *same* at both ends. If the window is chosen so that the
irregularities vanish inside it, they cancel out of the difference for free.

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

The hundredth year sits at the window boundary. That is not a coincidence to be
grateful for, it is the reason the window was chosen this way, and it means the
one hard case can be handled once per boundary crossing instead of once per
query. `century_start`'s `m >> 2` does exactly that; [§6](#6-the-collapsed-leap-test)
is what remains of the three-clause test afterwards.

Concretely: inside a century, the year lengths run 365, 365, 365, 366 for
`z ∈ [0, 95]`, then 365, 365, 365 for `z ∈ [96, 98]`, and year 99 is 366 only if
the century itself is a leap century. A century is therefore 36524 days long
three times in four, and 36525 once.

### 3.1 An unsigned day count

Every division below is a truncating machine division. For unsigned operands
truncation *is* floor division, so all of Hinnant's `(z >= 0 ? z : z - 146096)`
style fixups disappear if the day count can be made non-negative first. So:

```cpp
constexpr u32 CENTURY_ORIGIN = 2147614883u;

u32 n = u32( days ) + CENTURY_ORIGIN;
```

`u32( days )` is the well-defined two's-complement reinterpretation, and
`CENTURY_ORIGIN` has to satisfy exactly two properties, both asserted at the
declaration:

1. **It is `719468` plus a whole number of eras.** `2147614883 - 719468 ==
   146097 * 14695`. Whole eras cancel out of every difference the code computes,
   and they do not disturb any `% 4` or `% 400` test, so adding 14695 of them is
   free. Century index 0 therefore begins on March 1st of the year
   `-400 * 14695 == -5878000`, which is divisible by 400 — so the century index
   is aligned with the leap rules, and "century `c` is a leap century" is
   `c % 4 == 0`.
2. **It is at least 2³¹.** For `days == INT32_MIN`, `u32( days )` is `2³¹`, and
   `2³¹ + 2147614883` wraps modulo 2³² to `131235` — a small positive number.
   Every negative `days` maps somewhere above that. This is why the *bottom* of
   the `i32` range is safe and the *top* is not; see
   [§10](#10-ranges-and-failure-modes).

### 3.2 Where a century starts

```cpp
constexpr u32 DAYS_PER_CENTURY = 36524u;

constexpr u32 century_start( u32 m ) noexcept {
    return DAYS_PER_CENTURY * m + ( m >> 2 );
}
```

36524 days per century, plus one extra day for each divisible-by-400 leap day
already passed. The closed form is worth writing down, because both magic
constants in this file fall straight out of it:

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

The `4n + 3` form will be familiar if you have read Neri and Schneider's
[*Euclidean affine functions*][ns] paper — it is the first step of their
`to_date`. The `-1` in `4(n+1) - 1` is load-bearing: without it, the formula
reads one century too high on every day that is the last day of an era.

[ns]: https://arxiv.org/abs/2102.06959

The problem is that `4n` does not fit in a `u32`. `n` runs to 2³² − 1, so `4n`
needs 34 bits, and the whole point of the unsigned day count was to stay in
32-bit arithmetic. Widening to `u64` and dividing would work but buys a 64-bit
division by a constant. Instead:

```cpp
constexpr u64 CENTURY_MAGIC = 3853261555ull;
constexpr u32 CENTURY_SHIFT = 47u;

constexpr u32 century_of( u32 n ) noexcept {
    return u32( ( ( u64( n ) + 1 ) * CENTURY_MAGIC ) >> CENTURY_SHIFT );
}
```

One 32×64 widening multiply and one shift. The multiplier is

$$\texttt{CENTURY\_MAGIC} = \left\lfloor \frac{2^{47} \cdot 4}{146097} \right\rfloor = \lfloor 3853261555.14 \rfloor = 3853261555$$

so `((n+1) * M) >> 47` is a scaled, rounded-down stand-in for
`4(n+1) / 146097`. Rounded *down* is the essential part, and it is what supplies
the `-1`: at `n + 1 == 146097k`, the exact quotient `4(n+1)/146097` is the
integer `4k`, but the truncated multiplier undershoots it and the shift yields
`4k - 1`, which is the century index we actually want. Using the ceiling
(3853261556) instead gives the wrong answer at the very first era boundary,
`n == 146096`.

**Exactness.** The `+ 1` inside `century_of` is the `B` of the affine form, not
an off-by-one patch, and the identity

$$\left\lfloor \frac{(n+1) \cdot 3853261555}{2^{47}} \right\rfloor = \left\lfloor \frac{4n+3}{146097} \right\rfloor$$

holds for every `n` reachable from a `u32` day count. Three separate bounds are
worth keeping straight:

| Bound | Value | What it is |
| --- | --- | --- |
| Reachable | century 117592 | The last century whose first day fits in a `u32`. Asserted in the tests. |
| `u64`-safe | century 131071 | Beyond `n == 4787306495`, `(n+1) * M` leaves `u64`. |
| Mathematically exact | century 188178 | Where the identity itself first fails. |

There is a factor of 1.6 of headroom over what is reachable, and the failure
mode past the end is not silent: the reachable bound is checked, so a change
that widened the day count would trip an assertion rather than start returning
wrong centuries. Perturbing the multiplier by ±1 also fails: `M + 1` breaks at
century 4 and `M - 1` at century 23135, and both are caught by the compile-time
guard described in [§11](#11-yes-but-how-do-you-know-this-all-really-works).

## 4. `split_century`

Everything above assembles into six statements:

```cpp
VTZ_INLINE constexpr century_parts split_century( sys_days_t days ) noexcept {
    u32 n   = u32( days ) + CENTURY_ORIGIN;
    u32 c   = century_of( n );
    u32 doc = n - century_start( c );
    u32 t   = 4 * doc + 3;
    u32 z   = t / 1461;              // [0, 99]
    u32 doy = ( t - 1461 * z ) >> 2; // [0, ( 1461 - 1 ) >> 2] == [0, 365]
    return { c, z, doc, doy };
}
```

The first three lines are [§3](#3-the-century-as-the-working-window). The last
three extract the year within the century, and they are the *same derivation one
level down*.

Inside a century, March-based year `j` begins `365j + ⌊j/4⌋` days after the
century start — 365 days a year plus the divisible-by-4 leap days, with no
exceptions to correct for, which is the payoff for cutting on the century. And
just as before,

$$365j + \left\lfloor \frac{j}{4} \right\rfloor = \left\lfloor \frac{1461j}{4} \right\rfloor$$

so inverting it is the same maximisation with 1461 in place of 146097:

$$z = \left\lfloor \frac{4 \cdot \texttt{doc} + 3}{1461} \right\rfloor$$

which is `t = 4 * doc + 3; z = t / 1461`. No multiplier is needed this time:
`doc ≤ 36524`, so `4 * doc + 3` cannot overflow, and the division is by a
compile-time constant. (`4 * doc` has two zero low bits, so compilers emit the
`+ 3` as a shifted `orr` rather than an add.)

**The remainder is the day of the year, times four.** Write `s = z % 4`. Since
`1461 ≡ 1 (mod 4)`:

$$4 \cdot \texttt{doy} = 4\,\texttt{doc} - 1461z + s \quad\Longrightarrow\quad t - 1461z = 4\,\texttt{doy} + (3 - s)$$

and `3 - s ∈ [0, 3]`, so `>> 2` recovers `doy` exactly and discards the junk.
This is why the `+ 3` is the right offset and not, say, `+ 1`: it is
simultaneously the `-1` that makes the quotient land on year boundaries and the
padding that keeps the remainder's low two bits from borrowing.

`doy` reaches 365 only on a February 29th, since February is the last month of a
March-based year. That fact is used twice below.

Note what is *absent*: there is no correction term. The
`- doe/1460 + doe/36524 - doe/146096` apparatus of the era split exists to repair
leap exceptions that are simply not present inside a century.

## 5. The month magic: 535, 331, 14

Given `doy`, Hinnant's `civil_from_days` recovers the March-based month and the
day within it with two divisions:

```cpp
const u32 mp = ( 5 * doy + 2 ) / 153;      // [0, 11]
const u32 d  = doy - ( 153 * mp + 2 ) / 5; // [0, 30]
```

The `+ 2` in the first is not algebra; [*Computing month from day-of-year*][mfdoy]
shows the naive inverse `(5·doy − 2)/153` failing on real inputs, and arrives at
the shipped form by fixing the slope and then tuning the intercept until every
month boundary lands right. What follows is the same exercise done once more, with
two extra requirements: the denominator must be a power of two, so the divide
becomes a shift, and the *remainder* must stay usable, so that one product yields
both halves of the answer.

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

The high bits of a single product give the month; the low bits, still inside the
same product, give the 0-based day within it. The claim is not that `535/16384`
approximates `5/153` — it does, but that is not enough and not really the point.
The right way to see it is as a condition on the twelve month boundaries.

Define, for each March-based month `mp`,

$$r_{mp} = 535 \cdot \texttt{month\_start}(mp) + 331 - 16384 \cdot mp$$

which is the value of the low 14 bits on the *first* day of month `mp`. For a
`doy` inside month `mp`, with `dom = doy - month_start(mp)`, the product is

$$t = 16384 \cdot mp + 535 \cdot \texttt{dom} + r_{mp}$$

so both identities hold for every day of month `mp` exactly when:

- `0 ≤ r_mp < 535` — so that the low bits divided by 535 give `dom` and not
  `dom ± 1`, and so that `t >> 14` has not stepped early; **and**
- `535 · (len_mp − 1) + r_mp < 16384` — so that the low bits do not carry into
  bit 14 before the month is over, i.e. `t >> 14` does not step late.

Both are satisfied, with room to spare in most months:

| `mp` | month | `month_start` | `r_mp` | slack to the next carry |
| ---: | --- | ---: | ---: | ---: |
| 0 | March | 0 | 331 | **3** |
| 1 | April | 31 | 532 | 337 |
| 2 | May | 61 | 198 | 136 |
| 3 | June | 92 | 399 | 470 |
| 4 | July | 122 | 65 | 269 |
| 5 | August | 153 | 266 | 68 |
| 6 | September | 184 | 467 | 402 |
| 7 | October | 214 | 133 | 201 |
| 8 | November | 245 | 334 | 535 |
| 9 | December | 275 | 0 | 334 |
| 10 | January | 306 | 201 | 133 |
| 11 | February | 337 | 402 | 1002 |

March is the tight one: on March 31st the product is `535·30 + 331 == 16381`,
three short of the carry into bit 14. This triple is not a comfortable choice
that happens to work — it is one of a very small set, and the guards in the test
file exist because of that.

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
    u32 w = z == 99 ? c + 1 : z + 1;
    return ( w & 3 ) == 0;
}
```

"Does March-based year `z` of century `c` end with a February 29th?" That
February belongs to the *following* calendar year, hence the `+ 1` on both
branches. Writing the calendar year as `Y = 100c + z` (up to the multiple of 400
that `CENTURY_ORIGIN` contributes, which no `% 4` or `% 400` test can see):

- **`z < 99`.** The question is whether `Y + 1 = 100c + (z+1)` is a leap year.
  `z + 1 ∈ [1, 99]`, so `Y + 1` is not a multiple of 100 and both exception
  clauses are dead. `100c` is a multiple of 4, so `Y + 1` is a leap year iff
  `(z + 1) % 4 == 0`.
- **`z == 99`.** Then `Y + 1 = 100(c + 1)`, which *is* a multiple of 100, so it
  is a leap year iff it is a multiple of 400 — iff `(c + 1) % 4 == 0`.

So `z == 99` is the only case in which `c` is consulted at all, and even then
only its low two bits. The three-clause Gregorian test never appears, and neither
does the absolute year.

## 7. `add_years_impl`

Take the March-based framing seriously and year addition is almost trivial.
Every date sits at a fixed offset `doy` from its own March 1st; a leap day is
appended to the *end* of a March-based year, never inserted into the middle. So
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
    i32 total = i32( z ) + years;
    i32 cq    = math::div_floor<100>( total );
    u32 z2    = u32( total - 100 * cq ); // [0, 99]

    // Same leap-day count as add_months_impl
    i32 leap_delta
        = 24 * cq + ( ( i32( c & 3 ) + cq ) >> 2 ) + i32( z2 >> 2 );

    // 365 days per year, plus the leap days crossed, minus where we
    // started within the century
    i32 delta = 365 * years + leap_delta - i32( z >> 2 );

    if constexpr( Clamp )
    {
        delta
            -= parts.doy == 365 && !march_year_is_leap( c + u32( cq ), z2 );
    }

    return days + delta;
}
```

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
delta -= parts.doy == 365 && !march_year_is_leap( c + u32( cq ), z2 );
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

    i32 total = i32( 12 * z + mp ) + months;
    i32 yq    = math::div_floor<12>( total );
    i32 cq    = math::div_floor<1200>( total );
    u32 mp2   = u32( total - 12 * yq ); // [0, 11]
    u32 z2    = u32( yq - 100 * cq );   // [0, 99]

    i32 leap_delta
        = 24 * cq + ( ( i32( c & 3 ) + cq ) >> 2 ) + i32( z2 >> 2 );

    i32 delta = 365 * yq + leap_delta + i32( month_start( mp2 ) )
                - i32( parts.doc ) + i32( dom );
    /* ... Clamp ... */
    return days + delta;
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
March 3rd, not February 28th. Nothing in the formula implements that. The reason
it happens anyway is that `delta` is built from `month_start(mp2) + dom` and
never normalises the result back into a month — so when `dom` exceeds the target
month's length, the sum simply runs past the month's end.

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
delta -= dom > last ? i32( dom - last ) : 0;
```

`MARCH_MONTH_LAST_BITS == 12652612` is a 24-bit table, two bits per month, `mp =
0` (March) in the low bits. Each field holds `30 - last`, where `last` is the
0-based last day of the month, so the field is 0 for a 31-day month, 1 for a
30-day month, and 3 for a 28-day February. Two bits suffice because month lengths
span only 28–31 days.

The March-based ordering pays off a third time here. February is the only month
whose length depends on the leap rule, and in a March-based year it is the *last*
month — so its field is the *top* one, and the leap adjustment is a single
subtraction of `1 << 22` applied to the whole word. The other eleven months
cannot be affected by it, so they cost nothing:

| `mp` | 0 (Mar) | 1 (Apr) | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 (Jan) | 11 (Feb) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| field | 0 | 1 | 0 | 1 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 3 → 2 if leap |
| `last` | 30 | 29 | 30 | 29 | 30 | 30 | 29 | 30 | 29 | 30 | 30 | 27 → 28 if leap |

Had February been left in its calendar position, the leap bump would have landed
in the middle of the word and every month after it would have needed a shifted
correction.

## 10. Ranges and failure modes

Three separate limits apply, and they do not coincide. The tightest one that
applies to your call is the one that matters.

### 10.1 The origin add wraps at the top of `i32`

`u32( days ) + CENTURY_ORIGIN` must not exceed 2³² − 1. Since
`CENTURY_ORIGIN == 2147614883`:

- **Every negative `days` is fine**, including `INT32_MIN`, which maps to
  `n == 131235` (century 3).
- **`days` is fine up to 2147352412**, which is 5881221-03-20. That is
  `INT32_MAX - 131235`.
- **Above that the day count wraps** and the four `civil_add_*` functions return
  wrong answers. The first misbehaviour is at `days == 2147352413`
  (5881221-03-21): `civil_add_months( 2147352413, -1 )` returns 2147352384 where
  the correct answer is 2147352385.

The lost window is the top 131,235 days of the `i32` range — about 359 years
ending at 5881580-07-11. Note that `k == 0` still returns the input unchanged
even inside the wrapped window, because `delta` is then identically zero
regardless of how the decode came out; do not read a correct answer there as
evidence that the range is wider than this.

### 10.2 The decoders are narrower still

`to_civil`, `to_civil0` and `to_civil_year_doy` compute `days += 719468` in
`i32`, so they overflow — signed-overflow UB, not a wrap — above
`INT32_MAX - 719468 == 2146764179`. Anything that formats or decodes a result is
therefore already limited more tightly than the `civil_add_*` family is, and the
test suite bounds itself by that number rather than by §10.1. This is a
pre-existing property of the era-split decoders and is out of scope here.

### 10.3 The result, and one intermediate

`days + delta` must fit in `i32`; that is the ordinary caller's-responsibility
limit, and it bounds the shift to roughly ±5.88 million years.

One intermediate binds marginally earlier. `365 * years` in `add_years_impl`
overflows `i32` at `|years| ≥ 5883517`, and the corresponding `365 * yq` in
`add_months_impl` at `|months| ≳ 70602204`. Both are reachable in principle —
shifting +5.9 million years from `INT32_MIN` lands comfortably inside the `i32`
range — and both are UB when they happen. In practice the whole computation is
exact modulo 2³², so on a wrapping target the final answer still comes out right
whenever the true result is representable; that is an observation about what
compilers currently do, not a guarantee. Shifts of that magnitude are outside
what the tests cover.

### 10.4 Summary

| Input | Behaviour |
| --- | --- |
| `days` in `[INT32_MIN, 2147352412]`, result in range, shift under 5883517 years | Exact |
| `days` above 2147352412 | Wrong answers — the day count wraps |
| `days` above 2146764179 | Also unusable via `to_civil` — signed overflow |
| `days + delta` outside `i32` | Caller error |
| Shift of 5883517 years / 70602204 months or more | Signed-overflow UB in an intermediate |

## 11. Yes, but how do you know this all really works?

Borrowing Hinnant's [section title][yesbut], because the answer has the same
shape: an exhaustive check where one is possible, and an independent reference
implementation everywhere else.

[yesbut]: https://howardhinnant.github.io/date_algorithms.html#Yes,%20but%20how%20do%20you%20know%20this%20all%20really%20works?

### 11.1 Compile-time guards on the constants

Every magic constant is checked by `static_assert`, so a bad one fails the build
rather than a test run. The guards live in
[`etc/test/test_impl/test_civil.cpp`](../etc/test/test_impl/test_civil.cpp)
rather than in `civil.h`, so the checking machinery is not recompiled in every
translation unit that includes the public header. Several of them are exhaustive
rather than sampled, which is worth spelling out:

- **`century_magic_ok`** checks eight points, and eight is exhaustive. Only step
  points need checking, since both the exact quotient and the magic form are
  non-decreasing in `n`, so agreeing wherever the exact value steps forces
  agreement in between. And only the *first and last* step point of each residue
  class `m % 4` is needed: writing `m = 4q + r` makes `century_start` exactly
  `146097q + 36524r`, so for fixed `r` the floored quantity is affine in `q`, and
  an affine function stays inside a half-open interval over a range iff it does at
  both ends. Four residues × two ends settles all 117,593 reachable centuries. A
  brute-force sweep over every century agrees; it is not used because it costs
  ~0.4 s per TU and exceeds clang's constexpr step limit.
- **`month_magic_ok`** is genuinely exhaustive: it checks both identities of
  [§5](#5-the-month-magic-535-331-14) for every `doy` in `[0, 365]`, against the
  `(5·doy+2)/153` and `month_start` expressions they replace. A companion
  assertion pins `MONTH_MAGIC_FIRST_BAD_DOY = 428` as the place where the
  identity *actually* first fails, so the 63-day margin is a checked fact rather
  than a claim in a comment.
- **`split_century_bound_ok`** checks that `doy ≤ 365`, which is what the month
  magic depends on. One 4-year cycle covers every case, because `doy` depends on
  `doc` only through `doc % 1461` and a century is a whole number of such cycles.
  It requires `worst == MAX_DOY` with equality, so a change that lowered the real
  maximum would show up rather than leaving a stale constant behind.
- **`march_month_last_bits_ok`** checks all twelve fields of the packed table
  against the month lengths implied by `month_start`, rather than against a
  hand-written list.
- Two assertions pin `MAX_CENTURY` as the last century reachable from a `u32` day
  count — its first day fits, and the next century's does not. The second is
  deliberately computed in `u64`, since the quantity it tests is past the end of
  `u32` and computing it in `u32` would wrap and test nothing.
- `CENTURY_ORIGIN`'s two properties from
  [§3.1](#31-an-unsigned-day-count) are asserted at its declaration in
  `civil.h`, along with the non-overflow of `(n + 1) * CENTURY_MAGIC`.

One gap worth naming: `march_year_is_leap` has no compile-time guard, although a
four-century sweep would be exhaustive for it (both sides depend on `c` only
through `c % 4`). It is covered by the tests below, but not the way its
neighbours are.

### 11.2 Dense sweeps

`TEST( vtz, civil_arithmetic )` and the two `*_clamped` tests walk every day from
1570-01-01 to 2370-12-31 and, for each, every offset `k ∈ [-100, 100]`, comparing
against `resolve_civil` at the shifted `(y, m, d)`. That is 292,559 days × 201
offsets, or ~58.8 million cases per function, and it covers 8 century
boundaries, 2 divisible-by-400 years (1600, 2000) and 6 divisible-by-100
non-leap years.

### 11.3 Fuzzing against an independent reference

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

## 12. What it costs

Instruction counts for each function compiled standalone with
`[[gnu::flatten, gnu::noinline]]`, against the equivalent written as
`to_civil` → shift → `resolve_civil`. arm64, Apple clang 21, `-O2`:

| Function | Century split | Decode + re-encode |
| --- | ---: | ---: |
| `civil_add_years` | 39 | 106 |
| `civil_add_months` | 65 | 119 |
| `civil_add_years_clamped` | 49 | — |
| `civil_add_months_clamped` | 82 | — |

Instruction counts are a proxy, not a measurement; `etc/bench/src/bench_vtz_civil.cpp`
has the actual benchmarks. Structurally, two things are being avoided. The
re-encode is gone entirely, and with it the `yoe/4 - yoe/100` leap-day
accounting and the floor-division of the year by 400. And the four-division
`yoe` chain is replaced by one widening multiply plus one division by 1461, since
inside a century there are no leap exceptions to correct for.

The clamped variants cost ten to seventeen instructions more than the plain
ones, which is the packed-table lookup and the conditional step-back.

## Where the code lives

All in [`include/impl/vtz/civil.h`](../include/impl/vtz/civil.h). Line numbers
are as of this writing; the names are stable.

| | |
| --- | --- |
| `month_start` | [civil.h:33](../include/impl/vtz/civil.h#L33) |
| `century_parts` | [civil.h:48](../include/impl/vtz/civil.h#L48) |
| `CENTURY_ORIGIN`, and its guards | [civil.h:66](../include/impl/vtz/civil.h#L66) |
| `DAYS_PER_CENTURY` | [civil.h:70](../include/impl/vtz/civil.h#L70) |
| `CENTURY_MAGIC`, `CENTURY_SHIFT` | [civil.h:84](../include/impl/vtz/civil.h#L84) |
| `century_start` | [civil.h:94](../include/impl/vtz/civil.h#L94) |
| `century_of` | [civil.h:99](../include/impl/vtz/civil.h#L99) |
| `MARCH_MONTH_LAST_BITS` | [civil.h:107](../include/impl/vtz/civil.h#L107) |
| `MONTH_MAGIC_*` | [civil.h:116](../include/impl/vtz/civil.h#L116) |
| `split_century` | [civil.h:129](../include/impl/vtz/civil.h#L129) |
| `march_year_is_leap` | [civil.h:150](../include/impl/vtz/civil.h#L150) |
| `add_months_impl` | [civil.h:171](../include/impl/vtz/civil.h#L171) |
| `add_years_impl` | [civil.h:240](../include/impl/vtz/civil.h#L240) |
| `civil_add_months` / `_years` / `_clamped` | [civil.h:683](../include/impl/vtz/civil.h#L683)–[714](../include/impl/vtz/civil.h#L714) |
| `is_leap` | [civil.h:459](../include/impl/vtz/civil.h#L459) |
| `to_civil` (era split, unchanged) | [civil.h:584](../include/impl/vtz/civil.h#L584) |
| `resolve_civil` (era split, unchanged) | [civil.h:531](../include/impl/vtz/civil.h#L531) |
| `math::div_floor` | [math.h:51](../include/api/vtz/impl/math.h#L51) |

Tests, in [`etc/test/test_impl/test_civil.cpp`](../etc/test/test_impl/test_civil.cpp):

| | |
| --- | --- |
| `i64` reference implementations | [test_civil.cpp:99](../etc/test/test_impl/test_civil.cpp#L99) |
| Compile-time guards | [test_civil.cpp:206](../etc/test/test_impl/test_civil.cpp#L206)–[342](../etc/test/test_impl/test_civil.cpp#L342) |
| `civil_arithmetic` (dense sweep) | [test_civil.cpp:632](../etc/test/test_impl/test_civil.cpp#L632) |
| `civil_add_months_clamped` | [test_civil.cpp:679](../etc/test/test_impl/test_civil.cpp#L679) |
| `civil_add_years_clamped` | [test_civil.cpp:733](../etc/test/test_impl/test_civil.cpp#L733) |
| `civil_arithmetic_fuzz` | [test_civil.cpp:792](../etc/test/test_impl/test_civil.cpp#L792) |

## References

- Howard Hinnant, [*`chrono`-Compatible Low-Level Date Algorithms*][hinnant],
  2021-09-01. The source of the epoch shift, the March-based year, the era
  decomposition, and the `(153·mp + 2)/5` month table. `to_civil`,
  `to_civil0`, `resolve_civil` and `resolve_civil0` in `civil.h` are derived
  from `civil_from_days` and `days_from_civil` and carry that attribution.
- Cassio Neri and Lorenz Schneider, [*Euclidean affine functions and their
  application to calendar algorithms*][ns], 2021. Not an input to this
  implementation, but the `4n + 3` scaling in
  [§3.3](#33-inverting-it) and [§4](#4-split_century) is the first step of their
  `to_date`, and their framework is the right one for reasoning about why these
  multiply-shift forms are exact. A century-split variant of the full
  Neri–Schneider decode is a separate, unmerged line of work.
- Torbjörn Granlund and Peter L. Montgomery, *Division by invariant integers
  using multiplication*, PLDI '94. The general form of which `CENTURY_MAGIC` and
  the month magic are instances.
