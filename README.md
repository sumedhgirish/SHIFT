# SHIFT (Secure Hardware Interface for File Transfer)

Welcome to the **SHIFT** repository! This project implements an advanced, cryptographically verifiable append-only filesystem and Host Security Module (HSM) designed specifically for the **ECTF 2026** competition.

## Project Structure

The project is divided into several key directories:

- **`firmware/`**: Contains the core C codebase for the MSPM0L2228 microcontroller. This is where the primary HSM logic, cryptographic handlers, and flash driver reside.
  - **[`firmware/README.md`](firmware/README.md)**: Start here for an architectural overview of how the firmware operates, handles UART communications, and secures data in flash.
- **`firmware/utils/`**: Contains the Python tooling required to derive and provision device-specific cryptographic identities (`derive_secrets.py`).
  - **[`firmware/utils/README.md`](firmware/utils/README.md)**: Read this to understand how the asymmetric shared-secret derivation handles user permissions securely at build-time.
- **`firmware/external/`**: Submodules containing third-party cryptographic primitives (`ascon`, `micro-ecc`).

## Documentation & API Reference

We have heavily documented the internal C source code and Python utilities using a concise, Linux-kernel documentation style to explain the *how* and *why* behind the critical hardware security mechanisms.

### Generating the Doxygen Website

To view the complete API reference, function call graphs, and structural definitions, you can generate the local HTML documentation site:

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

### Authors

1. Sumedh Girish
2. Aditya Naskar
3. Shriniketh Kana
