@mainpage Design

# Introduction

Welcome to the **SHIFT** repository! This project implements a cryptographically
enforced permission-based embedded filesystem as a Hardware Security Module
(HSM) designed specifically for the **ECTF 2026** competition.

The focus of this project is to ensure that no unauthorized operations can be
performed on the stored file data via either hardware or software exploits. This
assurance is gained by designing a system whose ability to perform any given
action is a function of the cryptographic state of the data it is acting on.

If any operation is cryptographically invalid, it is by extension also
functionaly invalid. This reduces the problem of protecting all data on the
HSM to simply protecting the cryptographic state of the hardware at the time
of operation. The only way in which a valid operation can be performed is by
rederiving the correct cryptographic state.

This project is built to run on the `TI MSPM0L2228` microcontroller. It sports
an `arm cortex m0+` cpu that runs at 32MHz with 32KB SRAM and 256KB flash. This
severely limits the types and extent of protection we can apply via the choice
of cryptographic algorithm, and the choices made seek to provide the maximum
protection possible on the given hardware.

For more detailed information on specific components, see:

- @subpage firmware_design "Firmware Design and Architecture"
- @subpage key_generation "Key Generation"

### Generating the Doxygen Website

To view the complete API reference, function call graphs, and structure
definitions, you can generate the local HTML documentation site:

1. Ensure you have [Doxygen](https://www.doxygen.nl/) installed on your machine.

1. From the root of the repository, run:

   ```bash
   doxygen Doxyfile

   ```

1. Open `docs/html/index.html` in your preferred web browser.

### Exporting documentation to PDF

The `Doxyfile` is also configured to support high-quality PDF generation.
If you have a LaTeX distribution (like `texlive`) installed:

1. Run `doxygen Doxyfile` as shown above.

1. Navigate to the newly created `latex` directory:

   ```bash
   cd docs/latex
   ```

1. Build the PDF:

   ```bash
   make
   ```

1. Open the generated `refman.pdf` document.
