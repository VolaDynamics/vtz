# /// script
# requires-python = ">=3.11"
# ///
"""Generate benchmark comparison tables from Google Benchmark JSON output."""

from __future__ import annotations

import argparse
import json
import math
import re
import tomllib
from dataclasses import dataclass, field
from pathlib import Path


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

@dataclass
class BenchmarkConfig:
    """A single ``[[benchmark]]`` entry from the config file."""
    name: str
    pattern: str | None = None


@dataclass
class Config:
    """Parsed contents of a ``bm_table_config.toml`` file."""
    entries: list[str]
    basis: str
    benchmarks: list[BenchmarkConfig]

    entry_aliases: dict[str, list[str]] = field(default_factory=dict)
    rename_entries: dict[str, str] = field(default_factory=dict)
    default_pattern: str = "${name}_${entries}$"

    place_units_after_range: bool = False
    times_use_error_bar: bool = False
    separate_speedup_table: bool = False
    benchmark_use_backtick: bool = False
    speedup_column_name: str = "${entry}"

    def display_name(self, entry: str) -> str:
        """Return the display name for an entry, applying ``rename_entries``."""
        return self.rename_entries.get(entry, entry)

    @classmethod
    def from_toml(cls, path: Path) -> Config:
        with open(path, "rb") as f:
            raw = tomllib.load(f)

        benchmarks = [
            BenchmarkConfig(
                name=b["name"],
                pattern=b.get("pattern"),
            )
            for b in raw["benchmark"]
        ]

        return cls(
            entries=raw["entries"],
            basis=raw["basis"],
            benchmarks=benchmarks,
            entry_aliases=raw.get("entry_aliases", {}),
            rename_entries=raw.get("rename_entries", {}),
            default_pattern=raw.get("default_pattern", cls.default_pattern),
            place_units_after_range=raw.get("place_units_after_range", False),
            times_use_error_bar=raw.get("times_use_error_bar", False),
            separate_speedup_table=raw.get("separate_speedup_table", False),
            benchmark_use_backtick=raw.get("benchmark_use_backtick", False),
            speedup_column_name=raw.get(
                "speedup_column_name", cls.speedup_column_name),
        )


# ---------------------------------------------------------------------------
# Number formatting helpers
# ---------------------------------------------------------------------------

def round_sigfigs(x: float, n: int) -> float:
    """Round *x* to *n* significant figures."""
    if x == 0:
        return 0.0
    d = math.floor(math.log10(abs(x)))
    power = n - 1 - d
    if power >= 0:
        factor = 10**power
        return round(x * factor) / factor
    else:
        factor = 10 ** (-power)
        return round(x / factor) * factor


def _decimal_places(value: float, sigfigs: int) -> int:
    """Number of decimal places when formatting *value* with *sigfigs* significant figures."""
    if value == 0:
        return 0
    magnitude = math.floor(math.log10(abs(value)))
    return max(0, sigfigs - 1 - magnitude)


def format_number(value: float, sigfigs: int) -> str:
    """Format a plain number with the given significant figures (no unit suffix)."""
    rounded = round_sigfigs(value, sigfigs)
    if rounded == 0:
        return "0"
    dp = _decimal_places(rounded, sigfigs)
    if dp == 0:
        return str(int(round(rounded)))
    rounded = round(rounded, dp)
    return f"{rounded:.{dp}f}"


def _format_fixed(value: float, dp: int) -> str:
    """Format a number with exactly *dp* decimal places."""
    if dp <= 0:
        return str(int(round(value)))
    return f"{round(value, dp):.{dp}f}"


# ---------------------------------------------------------------------------
# Time cell formatting
# ---------------------------------------------------------------------------

def format_time_cell(
    times: list[float],
    sigfigs: int = 3,
    error_bar: bool = False,
    units_after: bool = False,
) -> str:
    """Format benchmark times as a single value, range, or error bar."""
    unit = "ns"
    lo, hi = min(times), max(times)
    lo_r = round_sigfigs(lo, sigfigs)
    hi_r = round_sigfigs(hi, sigfigs)
    lo_s = format_number(lo_r, sigfigs)
    hi_s = format_number(hi_r, sigfigs)

    if lo_s == hi_s:
        return f"{lo_s}{unit}"

    if error_bar:
        mid = (lo_r + hi_r) / 2
        err = (hi_r - lo_r) / 2
        dp = max(_decimal_places(lo_r, sigfigs),
                 _decimal_places(hi_r, sigfigs))
        mid_s = _format_fixed(mid, dp)
        err_s = _format_fixed(err, dp)
        if units_after:
            return f"{mid_s} \u00b1 {err_s} {unit}"
        return f"{mid_s} \u00b1 {err_s}{unit}"

    if units_after:
        return f"{lo_s} - {hi_s} {unit}"
    return f"{lo_s}{unit} - {hi_s}{unit}"


# ---------------------------------------------------------------------------
# Speedup formatting
# ---------------------------------------------------------------------------

def _format_speedup_number(value: float, sigfigs: int) -> str:
    """Format a number for speedup display.

    Uses *sigfigs* significant figures, but never rounds away integral
    digits.  For example, with ``sigfigs=3`` a value of 1868.6 is
    rendered as ``"1869"`` (4 integral digits preserved) rather than
    ``"1870"`` (rounded to 3 sig figs).
    """
    if value == 0:
        return "0"
    int_digits = math.floor(math.log10(abs(value))) + 1
    effective = max(sigfigs, int_digits)
    rounded = round_sigfigs(value, effective)
    dp = _decimal_places(rounded, effective)
    if dp == 0:
        return str(int(round(rounded)))
    rounded = round(rounded, dp)
    return f"{rounded:.{dp}f}"


def format_speedup(lo: float, hi: float) -> str:
    """Format speedup as single value, range, or approximate value.

    Single values use 3 sig figs, ranges use 2.  If a range collapses at
    2 sig figs, shows ``~mean`` at 3 sig figs instead.  Integral digits
    are never rounded away (see :func:`_format_speedup_number`).
    """
    lo_3 = _format_speedup_number(lo, 3)
    hi_3 = _format_speedup_number(hi, 3)
    if lo_3 == hi_3:
        return f"{lo_3}x"
    lo_2 = _format_speedup_number(lo, 2)
    hi_2 = _format_speedup_number(hi, 2)
    if lo_2 != hi_2:
        return f"{lo_2}x - {hi_2}x"
    mean = (lo + hi) / 2
    return f"~{_format_speedup_number(mean, 3)}x"


def _speedup_bounds(basis_times: list[float], other_times: list[float]) -> tuple[float, float]:
    """Compute speedup bounds: ``other / basis`` using ``min/min`` and ``max/max``."""
    b_min, b_max = min(basis_times), max(basis_times)
    o_min, o_max = min(other_times), max(other_times)
    b1 = o_min / b_min
    b2 = o_max / b_max
    return min(b1, b2), max(b1, b2)


# ---------------------------------------------------------------------------
# Table output
# ---------------------------------------------------------------------------

def print_table(header: list[str], data_rows: list[list[str]]) -> None:
    """Print a markdown table with aligned columns."""
    all_rows = [header] + data_rows
    col_widths = [
        max(len(r[i]) for r in all_rows) for i in range(len(header))
    ]

    def fmt_row(cells: list[str]) -> str:
        return (
            "| "
            + " | ".join(c.ljust(col_widths[i]) for i, c in enumerate(cells))
            + " |"
        )

    print(fmt_row(header))
    print("| " + " | ".join("-" * w for w in col_widths) + " |")
    for row in data_rows:
        print(fmt_row(row))


# ---------------------------------------------------------------------------
# Benchmark matching
# ---------------------------------------------------------------------------

def _collect_times(
    cfg: Config,
    benchmarks_json: list[dict],
) -> list[tuple[str, dict[str, list[float]]]]:
    """Match JSON benchmarks to config groups, returning per-entry times."""
    rows: list[tuple[str, dict[str, list[float]]]] = []

    for bm_cfg in cfg.benchmarks:
        pattern_tmpl = bm_cfg.pattern or cfg.default_pattern

        entry_times: dict[str, list[float]] = {}
        for entry in cfg.entries:
            names = [entry] + cfg.entry_aliases.get(entry, [])
            names.sort(key=len, reverse=True)
            entry_re = "(" + "|".join(re.escape(n) for n in names) + ")"
            pat = pattern_tmpl.replace("${name}", re.escape(bm_cfg.name))
            pat = pat.replace("${entries}", entry_re)
            regex = re.compile(pat)

            times = [bm["cpu_time"]
                     for bm in benchmarks_json if regex.match(bm["name"])]
            if times:
                entry_times[entry] = times

        rows.append((bm_cfg.name, entry_times))

    return rows


# ---------------------------------------------------------------------------
# Table construction
# ---------------------------------------------------------------------------

def _build_times_table(
    cfg: Config,
    rows: list[tuple[str, dict[str, list[float]]]],
    active_entries: list[str],
) -> tuple[list[str], list[list[str]]]:
    """Build the header and data rows for the times table."""
    fmt_name = _name_formatter(cfg)

    display_entries = [cfg.display_name(e) for e in active_entries]
    if cfg.separate_speedup_table:
        header = [""] + display_entries
    else:
        header = [""] + display_entries + ["speedup"]

    table_data: list[list[str]] = []
    for name, entry_times in rows:
        row = [fmt_name(name)]
        for entry in active_entries:
            if entry in entry_times:
                row.append(format_time_cell(
                    entry_times[entry],
                    error_bar=cfg.times_use_error_bar,
                    units_after=cfg.place_units_after_range,
                ))
            else:
                row.append("")

        if not cfg.separate_speedup_table:
            row.append(_inline_speedup(cfg, entry_times, active_entries))

        table_data.append(row)

    return header, table_data


def _inline_speedup(
    cfg: Config,
    entry_times: dict[str, list[float]],
    active_entries: list[str],
) -> str:
    """Compute the inline speedup column value (basis vs nearest competitor)."""
    basis_times = entry_times.get(cfg.basis)
    if not basis_times:
        return ""

    nearest: str | None = None
    nearest_min = float("inf")
    for entry in active_entries:
        if entry == cfg.basis or entry not in entry_times:
            continue
        entry_min = min(entry_times[entry])
        if entry_min < nearest_min:
            nearest_min = entry_min
            nearest = entry

    if nearest is None:
        return ""

    lo, hi = _speedup_bounds(basis_times, entry_times[nearest])
    return format_speedup(lo, hi)


def _build_speedup_table(
    cfg: Config,
    rows: list[tuple[str, dict[str, list[float]]]],
    active_entries: list[str],
) -> tuple[list[str], list[list[str]]]:
    """Build the header and data rows for the separate speedup table."""
    fmt_name = _name_formatter(cfg)
    sp_entries = [e for e in active_entries if e != cfg.basis]

    header = [""] + [
        cfg.speedup_column_name
        .replace("${basis}", cfg.display_name(cfg.basis))
        .replace("${entry}", cfg.display_name(e))
        for e in sp_entries
    ]

    table_data: list[list[str]] = []
    for name, entry_times in rows:
        row = [fmt_name(name)]
        basis_times = entry_times.get(cfg.basis)
        for entry in sp_entries:
            if not basis_times or entry not in entry_times:
                row.append("")
            else:
                lo, hi = _speedup_bounds(basis_times, entry_times[entry])
                row.append(format_speedup(lo, hi))
        table_data.append(row)

    return header, table_data


def _name_formatter(cfg: Config):
    """Return a function that optionally wraps benchmark names in backticks."""
    if cfg.benchmark_use_backtick:
        return lambda name: f"`{name}`"
    return lambda name: name


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate benchmark comparison table from Google Benchmark JSON"
    )
    parser.add_argument("input", help="Google Benchmark JSON file")
    parser.add_argument("--config", default=None, help="Config TOML file")
    args = parser.parse_args()

    if args.config is None:
        config_path = Path(__file__).resolve(
        ).parent.parent / "bm_table_config.toml"
    else:
        config_path = Path(args.config)

    cfg = Config.from_toml(config_path)

    with open(args.input) as f:
        benchmarks_json = json.load(f)["benchmarks"]

    rows = _collect_times(cfg, benchmarks_json)
    active_entries = [e for e in cfg.entries if any(e in et for _, et in rows)]

    header, table_data = _build_times_table(cfg, rows, active_entries)
    print_table(header, table_data)

    if cfg.separate_speedup_table:
        print()
        sp_header, sp_data = _build_speedup_table(cfg, rows, active_entries)
        print_table(sp_header, sp_data)


if __name__ == "__main__":
    main()
