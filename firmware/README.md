# SHIFT Firmware

An advanced, cryptographically verifiable append-only filesystem and Host Security Module (HSM) designed for the ECTF 2026 competition.

## Architecture Overview

The firmware implements a strict boundary between Host requests and internal secure operations. All data written to the HSM is protected via state-of-the-art AEAD cryptography (ASCON-128a), ensuring confidentiality and tamper-evident integrity.

```text
+----------------+       +-------------------+       +-----------------+
| Host Interface |       |  Command Handlers |       | Flash Filesystem|
|   (UART 0)     | ----> | (Parsing & Crypto)| ----> |  (Append-Only)  |
| Decodes frames |       | Decrypts payloads |       |  Wear-leveling  |
+----------------+       +-------------------+       +-----------------+
        ^                          v                          |
        |                  +---------------+                  |
        +----------------  | Peer HSM Tx/Rx| <----------------+
                           |   (UART 1)    |
                           +---------------+
```

## Source Layout

- `include/` & `src/`: Core implementation.
- `utils/`: Contains `derive_secrets.py`, the core script for provisioning identities and permissions during the build process.

## Building & the Doxygen Setup

```bash
cd SHIFT/
doxygen Doxyfile
# Open docs/html/index.html in your browser
```
