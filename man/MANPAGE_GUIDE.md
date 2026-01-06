# Equinox Framework - Man Page Quick Reference

## Viewing Man Pages

### After Installation (requires `sudo make install`)

```bash
# Overview of the framework
man 7 equinox

# API function references
man 3 application
man 3 module
man 3 service_controller
man 3 http_server
man 3 http_route
man 3 framework
```

### Without Installation

```bash
cd man

# View with man command
man ./http_server.3
man ./equinox.7

# View with groff (formatted ASCII)
groff -man -Tascii http_server.3 | less

# View with groff (formatted UTF-8)
groff -man -Tutf8 http_server.3 | less
```

## Installation

```bash
# Install library and man pages
sudo make install

# Or install only man pages
cd man
sudo make install
```

After installation, the man database is automatically updated.

## Searching Man Pages

```bash
# Search for HTTP server functions
man -k http_server

# Search all equinox-related pages
man -k equinox

# Search for specific function
man -k http_response_set

# Full-text search in man pages
man -K "path parameter"
```

## Man Page Sections

The Equinox man pages use standard sections:

- **Section 3**: Library function calls (application, module, http_server, etc.)
- **Section 7**: Miscellaneous (equinox overview)

## Quick Function Lookup

```bash
# Find which man page documents a function
man -f application_create    # Shows: application(3)
man -f http_server_run       # Shows: http_server(3)

# Or use whatis (equivalent)
whatis application_create
whatis http_server_run
```

## Reading Formats

### Terminal (default)
```bash
man 3 http_server
```

### ASCII text
```bash
man 3 http_server | col -b > http_server.txt
```

### HTML
```bash
man -Thtml 3 http_server > http_server.html
```

### PDF (requires groff with PDF support)
```bash
man -Tps 3 http_server | ps2pdf - http_server.pdf
```

## Navigation in Man Pages

When viewing a man page:

- `Space` or `f` - Forward one page
- `b` - Backward one page
- `/pattern` - Search forward for pattern
- `?pattern` - Search backward for pattern
- `n` - Repeat last search forward
- `N` - Repeat last search backward
- `q` - Quit
- `h` - Help (shows all commands)

## Common Tasks

### Find all HTTP server functions
```bash
man 3 http_server
# Then look in SYNOPSIS section
```

### See an example
```bash
man 3 http_server
# Scroll to EXAMPLE section
```

### Check return values
```bash
man 3 application
# Scroll to RETURN VALUE section
```

### Find related functions
```bash
man 3 http_server
# Scroll to SEE ALSO section
```

## Man Page Structure

Each man page follows this standard format:

1. **NAME** - Function name and brief description
2. **SYNOPSIS** - Include files and function prototypes
3. **DESCRIPTION** - Detailed description of each function
4. **RETURN VALUE** - What each function returns
5. **EXAMPLE** - Working code example
6. **NOTES** - Additional information
7. **SEE ALSO** - Related man pages
8. **AUTHOR** - Author information

## Tips

### View multiple pages
```bash
# Compare two functions
man 3 application &
man 3 module &
```

### Export all as text
```bash
cd man
for page in *.3; do
    man -l "$page" | col -b > "../docs/txt/${page%.3}.txt"
done
```

### Check for errors
```bash
cd man
# Check all pages for formatting errors
for page in *.3 *.7; do
    echo "Checking $page..."
    groff -man -wall -z "$page"
done
```

### Quick reference card
```bash
# Print synopsis sections only
man 3 http_server | sed -n '/^SYNOPSIS/,/^DESCRIPTION/p' | head -n -1
```

## Environment Variables

Control man page behavior:

```bash
# Set pager
export MANPAGER="less -R"

# Set default width
export MANWIDTH=80

# Set color output
export LESS_TERMCAP_mb=$'\e[1;31m'
export LESS_TERMCAP_md=$'\e[1;36m'
export LESS_TERMCAP_me=$'\e[0m'
export LESS_TERMCAP_se=$'\e[0m'
export LESS_TERMCAP_so=$'\e[1;44;33m'
export LESS_TERMCAP_ue=$'\e[0m'
export LESS_TERMCAP_us=$'\e[1;32m'
```

## Integration with IDEs

### VS Code
Install "Man Pages" extension to view man pages in the editor.

### Vim
```vim
:Man http_server
```

### Emacs
```elisp
M-x man RET http_server RET
```

## Troubleshooting

### Man page not found after installation
```bash
# Rebuild man database
sudo mandb

# Check if file exists
ls -la /usr/share/man/man3/http_server.3.gz
```

### Formatting issues
```bash
# View raw source
cat man/http_server.3

# Check for syntax errors
groff -man -wall -z man/http_server.3
```

### Permission denied
```bash
# Make sure you used sudo for installation
sudo make install

# Check file permissions
ls -la /usr/share/man/man3/equinox*
```

## Reference

- Man page format: `man 7 groff_man`
- Man page sections: `man 7 man`
- Man-db documentation: `man 1 man`
