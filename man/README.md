# Equinox Framework Man Pages

This directory contains Linux manual pages for all Equinox Framework APIs.

## Available Man Pages

### Section 3 - Library Functions

- **application(3)** - Application lifecycle management
- **module(3)** - Modular component management
- **service_controller(3)** - Service endpoint management
- **http_server(3)** - HTTP/1.1 and HTTP/2 server functions
- **http_route(3)** - HTTP route management and matching
- **framework(3)** - Core framework initialization and utilities

### Section 7 - Miscellaneous

- **equinox(7)** - Overview of the Equinox Framework

## Installation

### System-wide Installation (requires sudo)

```bash
cd man
sudo make install
```

This installs the man pages to `/usr/share/man/` and updates the man database.

### Viewing Man Pages After Installation

```bash
# View overview
man 7 equinox

# View specific APIs
man 3 http_server
man 3 application
man 3 module
man 3 framework
```

### Viewing Without Installation

You can view the man pages directly without installing:

```bash
# From the man directory
man ./http_server.3
man ./equinox.7

# Or use groff directly
groff -man -Tascii http_server.3 | less
```

## Uninstallation

```bash
cd man
sudo make uninstall
```

## Man Page Format

The man pages are written in groff/troff format with standard man macros:

- `.TH` - Title heading
- `.SH` - Section heading
- `.TP` - Tagged paragraph
- `.B` - Bold text
- `.I` - Italic text
- `.BI` - Bold/italic alternating

## Man Page Sections

Each man page includes:

1. **NAME** - Brief description
2. **SYNOPSIS** - Function prototypes and includes
3. **DESCRIPTION** - Detailed description of functions
4. **RETURN VALUE** - Return values and error codes
5. **EXAMPLE** - Code examples
6. **NOTES** - Additional information
7. **SEE ALSO** - Related man pages
8. **AUTHOR** - Author information

## Updating Man Pages

When API changes occur, update the corresponding man page:

1. Edit the `.3` file for library functions
2. Edit the `.7` file for overview documentation
3. Reinstall: `sudo make install`
4. Update man database: `sudo mandb`

## Testing Man Pages

Test the formatting before installation:

```bash
# Check for errors
groff -man -wall -z http_server.3

# View formatted output
groff -man -Tascii http_server.3 | less

# View with color
man -l http_server.3
```

## Contributing

When adding new APIs:

1. Create a new `.3` man page file
2. Follow the existing format and style
3. Add to `MAN3_PAGES` in the Makefile
4. Cross-reference from other related pages
5. Update the main `equinox.7` overview

## Reference

- Man page format: `man 7 groff_man`
- Man page sections: `man 7 man`
- Groff macros: `man 7 groff`
