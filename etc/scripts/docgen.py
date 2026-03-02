import pathlib
import re
import subprocess
import sys
import tomllib


def format_text(text, cmd):
    """Pipe text through a shell formatting command and return the result."""
    result = subprocess.run(
        cmd, shell=True, input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print(f'warning: format command failed: {result.stderr.strip()}',
              file=sys.stderr)
        return text
    return result.stdout


def load_config(config_path):
    with open(config_path, 'rb') as f:
        return tomllib.load(f)


def split_segments(text):
    """Split source text into segments separated by one or more blank lines.

    Returns a list of lists-of-strings (each segment is a list of its lines).
    """
    raw_segments = re.split(r'\n(?:[ \t]*\n)+', text)
    segments = []
    for seg in raw_segments:
        lines = seg.split('\n')
        while lines and not lines[-1].strip():
            lines.pop()
        while lines and not lines[0].strip():
            lines.pop(0)
        if lines:
            segments.append(lines)
    return segments


def classify_segment(lines):
    """Classify a segment as 'comment' or 'code'.

    A segment is 'comment' if ALL non-empty lines start with '//' after
    stripping leading whitespace. Otherwise it is 'code'.
    """
    non_empty = [line for line in lines if line.strip()]
    if not non_empty:
        return 'code'
    if all(line.lstrip().startswith('//') for line in non_empty):
        return 'comment'
    return 'code'


def strip_comment_prefix(line):
    """Strip leading whitespace, then strip the '//' prefix.

    '    // foo bar'    -> 'foo bar'
    '    //'            -> ''
    '    //   indented' -> '  indented'
    """
    s = line.lstrip()
    if s.startswith('// '):
        return s[3:]
    if s.startswith('//'):
        return s[2:]
    return s


def comment_to_markdown(lines):
    return [strip_comment_prefix(line) for line in lines]


def trim_to_initial_indent(lines):
    """Trim a code block based on the first line's indentation.

    The first non-blank line's indent sets the base. The block stops at
    any subsequent non-blank line that is less-indented than the base.
    """
    base_indent = None
    for line in lines:
        if line.strip():
            base_indent = len(line) - len(line.lstrip())
            break
    if base_indent is None:
        return lines
    result = []
    for line in lines:
        if line.strip() and (len(line) - len(line.lstrip())) < base_indent:
            break
        result.append(line)
    while result and not result[-1].strip():
        result.pop()
    return result


def strip_common_indent(lines):
    """Remove the minimum common indentation from non-blank lines."""
    non_blank = [line for line in lines if line.strip()]
    if not non_blank:
        return lines
    min_indent = min(len(line) - len(line.lstrip()) for line in non_blank)
    return [line[min_indent:] if line.strip() else '' for line in lines]


def has_output_match(code_lines, matchers):
    """Check if any line in a code block matches any output matcher."""
    return any(m.search(line) for line in code_lines for m in matchers)


def run_example(exe_path, env_vars=None):
    """Run an example and return its stdout split into chunks on blank lines."""
    env = {}
    if env_vars:
        for item in env_vars:
            key, val = item.split('=', 1)
            env[key] = val
    result = subprocess.run(
        [str(exe_path)], capture_output=True, text=True, check=True,
        env=env or None)
    raw = result.stdout.strip()
    if not raw:
        return []
    return re.split(r'\n\n+', raw)


def generate_markdown(title, source_text, output_chunks=None,
                      output_matchers=None, source_path=None,
                      exe_command=None, intro=None):
    segments = split_segments(source_text)
    classified = [(classify_segment(seg), seg) for seg in segments]

    matchers = [re.compile(p) for p in (output_matchers or [])]
    chunks = list(output_chunks or [])
    chunk_idx = 0

    output_parts = []
    output_parts.append(f'# {title}')
    output_parts.append('')
    if intro:
        output_parts.append(intro)
        output_parts.append('')

    pending_code_lines = None

    def flush_code():
        nonlocal pending_code_lines, chunk_idx
        if pending_code_lines is None:
            return
        trimmed = trim_to_initial_indent(pending_code_lines)
        stripped = strip_common_indent(trimmed)
        output_parts.append('```cpp')
        output_parts.extend(stripped)
        output_parts.append('```')
        output_parts.append('')
        if matchers and chunks:
            if has_output_match(trimmed, matchers) \
                    and chunk_idx < len(chunks):
                combined = chunks[chunk_idx]
                chunk_idx += 1
                output_parts.append('<details>')
                output_parts.append('<summary>Example Output</summary>')
                output_parts.append('')
                output_parts.append('```')
                output_parts.append(combined)
                output_parts.append('```')
                output_parts.append('')
                output_parts.append('</details>')
                # output_parts.append('<br>')
                output_parts.append('')
        pending_code_lines = None

    # Skip leading code segments — the document starts at the first comment.
    started = False
    for kind, lines in classified:
        if not started:
            if kind != 'comment':
                continue
            started = True
        if kind == 'comment':
            flush_code()
            prose = comment_to_markdown(lines)
            output_parts.extend(prose)
            output_parts.append('')
        else:
            if pending_code_lines is not None:
                pending_code_lines.append('')
                pending_code_lines.extend(lines)
            else:
                pending_code_lines = list(lines)

    flush_code()

    # Footer
    if source_path:
        output_parts.append('---')
        output_parts.append('')
        output_parts.append(f'Code for this example can be found at `{source_path}`.')
        output_parts.append('')
    if exe_command:
        output_parts.append('This example can be run as:')
        output_parts.append('')
        output_parts.append('```sh')
        output_parts.append(exe_command)
        output_parts.append('```')
        output_parts.append('')

    text = '\n'.join(output_parts)
    text = text.rstrip('\n') + '\n'
    return text


def heading_level(line):
    """Return the heading level of a markdown heading line, or None."""
    stripped = line.lstrip()
    if not stripped.startswith('#'):
        return None
    hashes = len(stripped) - len(stripped.lstrip('#'))
    if hashes > 0 and (len(stripped) == hashes or stripped[hashes] == ' '):
        return hashes
    return None


def adjust_heading_depth(lines, min_depth):
    """Shift all headings so the shallowest is at least min_depth."""
    current_min = None
    for line in lines:
        h = heading_level(line)
        if h is not None and (current_min is None or h < current_min):
            current_min = h
    if current_min is None or current_min >= min_depth:
        return lines
    shift = min_depth - current_min
    return [('#' * shift + line if heading_level(line) is not None else line)
            for line in lines]


def insert_into_file(target_path, insert_at, content):
    """Replace a section in a markdown file.

    Finds the heading matching insert_at (must be unique), then replaces
    everything between it and the next heading of equal or greater weight.
    """
    lines = target_path.read_text().splitlines()

    # Find the insert_at heading
    matches = [i for i, line in enumerate(lines) if line.strip() == insert_at]
    if len(matches) == 0:
        print(f'error: heading not found in {target_path}: {insert_at}',
              file=sys.stderr)
        sys.exit(1)
    if len(matches) > 1:
        print(f'error: heading is not unique in {target_path}: {insert_at}',
              file=sys.stderr)
        sys.exit(1)

    idx = matches[0]
    level = heading_level(lines[idx])
    if level is None:
        print(f'error: insert_at is not a markdown heading: {insert_at}',
              file=sys.stderr)
        sys.exit(1)

    # Find the next heading of equal or greater weight (level <= current)
    end = len(lines)
    for i in range(idx + 1, len(lines)):
        h = heading_level(lines[i])
        if h is not None and h <= level:
            end = i
            break

    # Strip the title line from the generated content since the heading
    # in the target file serves that role.
    content_lines = content.splitlines()
    if content_lines and heading_level(content_lines[0]) is not None:
        content_lines = content_lines[1:]
        # Also strip the blank line that follows the title
        if content_lines and not content_lines[0].strip():
            content_lines = content_lines[1:]

    content_lines = adjust_heading_depth(content_lines, level + 1)

    replacement = [lines[idx], ''] + content_lines + ['']
    new_lines = lines[:idx] + replacement + lines[end:]
    target_path.write_text('\n'.join(new_lines) + '\n')


def main():
    script_dir = pathlib.Path(__file__).resolve().parent
    project_root = script_dir.parent.parent

    config_path = project_root / 'etc' / 'examples' / 'docgen.toml'
    if not config_path.exists():
        print(f'error: config not found: {config_path}', file=sys.stderr)
        sys.exit(1)

    config = load_config(config_path)
    output_dir = project_root / config['output_dir']
    exe_dir_rel = config.get('exe_dir', 'build/etc/examples')
    exe_dir = project_root / exe_dir_rel
    default_env = config.get('env', [])
    format_cmd = config.get('format_cmd')
    output_dir.mkdir(parents=True, exist_ok=True)

    for entry in config['examples']:
        source_path = project_root / entry['source']
        title = entry['title']

        if not source_path.exists():
            print(f'warning: skipping missing file: {source_path}',
                  file=sys.stderr)
            continue

        source_text = source_path.read_text()

        env_vars = entry.get('env', default_env)
        exe_name = entry.get('exe_name', source_path.stem)
        exe_rel = f'{exe_dir_rel}/{exe_name}'

        output_chunks = None
        output_matchers = entry.get('output_matchers')
        exe_command = None
        if entry.get('include_output'):
            exe_path = exe_dir / exe_name
            if not exe_path.exists():
                print(f'warning: executable not found: {exe_path}',
                      file=sys.stderr)
            else:
                output_chunks = run_example(exe_path, env_vars)
                if env_vars:
                    exe_command = f'env {" ".join(env_vars)} {exe_rel}'
                else:
                    exe_command = exe_rel

        example_intro = entry.get('example_intro')
        intro_is_standalone = entry.get('intro_is_standalone', True)

        common_args = dict(output_chunks=output_chunks,
                           output_matchers=output_matchers,
                           source_path=entry['source'],
                           exe_command=exe_command)

        standalone_intro = example_intro if intro_is_standalone else None
        markdown = generate_markdown(title, source_text, **common_args,
                                     intro=standalone_intro)
        if format_cmd:
            markdown = format_text(markdown, format_cmd)

        out_name = source_path.stem + '.md'
        out_path = output_dir / out_name
        out_path.write_text(markdown)
        print(f'  {out_path.relative_to(project_root)}')

        if 'insert_into' in entry and 'insert_at' in entry:
            insert_md = generate_markdown(title, source_text, **common_args,
                                          intro=None if intro_is_standalone
                                          else example_intro)
            if format_cmd:
                insert_md = format_text(insert_md, format_cmd)
            target_path = project_root / entry['insert_into']
            insert_into_file(target_path, entry['insert_at'], insert_md)
            print(f'  -> {entry["insert_into"]} @ "{entry["insert_at"]}"')

    print('done.')


if __name__ == '__main__':
    main()
