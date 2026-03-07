import random


class Colors:
    HEADER: str = "\033[95m"
    OKBLUE: str = "\033[94m"
    OKCYAN: str = "\033[96m"
    OKGREEN: str = "\033[92m"
    WARNING: str = "\033[93m"
    FAIL: str = "\033[91m"
    ENDC: str = "\033[0m"
    BOLD: str = "\033[1m"


def generate() -> None:
    # ... (existing generation code remains untouched)
    groups = random.sample(range(1, 65536), 32)
    hex_groups = [f"{g:04X}" for g in groups]

    with open("group_ids.txt", "w") as f:
        for h in hex_groups:
            _ = f.write(f"{h}\n")

    # Generate Permission Strings as per rules
    perms_a = (
        f"{hex_groups[0]}=---:"
        f"{hex_groups[1]}=R--:"
        f"{hex_groups[2]}=-W-:"
        f"{hex_groups[3]}=RW-:"
        f"{hex_groups[4]}=--C:"
        f"{hex_groups[5]}=R-C:"
        f"{hex_groups[6]}=-WC:"
        f"{hex_groups[7]}=RWC"
    )

    perms_b = f"{hex_groups[2]}=R-C:{hex_groups[3]}=RWC:{hex_groups[4]}=RWC:{hex_groups[7]}=RWC"

    with open("perms_hsmA.txt", "w") as f:
        _ = f.write(perms_a + "\n")

    with open("perms_hsmB.txt", "w") as f:
        _ = f.write(perms_b + "\n")

    # Generate 6-digit random hexadecimal PINs for HSM A and B
    pin_a = f"{random.randint(0, 0xFFFFFF):06x}".upper()
    pin_b = f"{random.randint(0, 0xFFFFFF):06x}".upper()

    with open("pin_hsmA.txt", "w") as f:
        _ = f.write(pin_a + "\n")

    with open("pin_hsmB.txt", "w") as f:
        _ = f.write(pin_b + "\n")

    print(
        f"{Colors.OKGREEN}{Colors.BOLD}✓ Successfully generated 32 random groups.{Colors.ENDC}"
    )
    print(f"{Colors.OKCYAN}HSM A Permissions:{Colors.ENDC} {perms_a}")
    print(f"{Colors.OKCYAN}HSM B Permissions:{Colors.ENDC} {perms_b}")
    print(f"{Colors.WARNING}HSM A PIN:{Colors.ENDC} {Colors.BOLD}{pin_a}{Colors.ENDC}")
    print(f"{Colors.WARNING}HSM B PIN:{Colors.ENDC} {Colors.BOLD}{pin_b}{Colors.ENDC}")


if __name__ == "__main__":
    generate()
