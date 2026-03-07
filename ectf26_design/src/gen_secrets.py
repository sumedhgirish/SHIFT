##
# @file gen_secrets.py
# @author Sumedh Girish
# @brief Script to generate master secrets for the SHIFT HSM system.
#
# Generates symmetric magic keys and ECC keypairs for groups and interrogation
# protocols, saving them to a pickled secrets file used during the firmware
# build process.
#

import argparse
import pickle
import secrets
from pathlib import Path

from loguru import logger

from cryptography.hazmat.primitives.asymmetric import ec

MAGIC_KEY_SIZE = 16
PRIVATE_KEY_SIZE = 32
PUBLIC_KEY_SIZE = 64


def gen_keys() -> dict[str, bytes]:
    """
    @brief Generates an ECC P-256 Master Keypair for a group.

    @return dict: A dictionary containing 'private' and 'public' byte keys.
    """
    priv = ec.generate_private_key(ec.SECP256R1())
    pub = priv.public_key()
    pub_nums = pub.public_numbers()

    return {
        "private": priv.private_numbers().private_value.to_bytes(32, "big"),
        "public": pub_nums.x.to_bytes(32, "big") + pub_nums.y.to_bytes(32, "big"),
        "tweak": secrets.token_bytes(MAGIC_KEY_SIZE),
    }


def makegroupkeys() -> dict[str, dict[str, bytes]]:
    read_write = gen_keys()
    send_recv = gen_keys()

    return {
        "read": {"master": read_write["private"], "tweak": read_write["tweak"]},
        "write": {"master": read_write["public"], "tweak": read_write["tweak"]},
        "recv": {"master": send_recv["private"], "tweak": send_recv["tweak"]},
        "send": {"master": send_recv["public"], "tweak": send_recv["tweak"]},
    }


def gen_secrets(groups: list[int]) -> bytes:
    """
    @brief Orchestrates secret generation for all supported groups and interrogation.

    @param groups (list[int]): List of group IDs to generate secrets for.

    @return dict: A nested dictionary mapping group IDs and 'interrogate' to their secrets.
    """

    group_keys: dict[str, dict[str, dict[str, bytes]]] = {
        f"{group:04x}": makegroupkeys() for group in groups
    }
    group_keys["global"] = {
        "interrogate": {"value": secrets.token_bytes(MAGIC_KEY_SIZE)}
    }
    return pickle.dumps(group_keys)


def parse_args() -> argparse.Namespace:
    """Define and parse the command line arguments"""
    parser = argparse.ArgumentParser()
    _ = parser.add_argument(
        "--force",
        "-f",
        action="store_true",
        help="Force creation of secrets file, overwriting existing file",
    )
    _ = parser.add_argument(
        "secrets_file",
        type=Path,
        help="Path to the secrets file to be created",
    )
    _ = parser.add_argument(
        "groups",
        nargs="+",
        type=lambda x: int(x, 16),
        help="Supported group IDs",
    )
    return parser.parse_args()


def main():
    # Parse the command line arguments
    args = parse_args()
    secrets = gen_secrets(args.groups)

    with open(args.secrets_file, "wb" if args.force else "xb") as f:
        f.write(secrets)

    logger.success(f"Wrote secrets to {str(args.secrets_file.absolute())}")


if __name__ == "__main__":
    main()
