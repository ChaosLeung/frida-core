import lief
import sys
import random

if __name__ == "__main__":
    input_file = sys.argv[1]
    random_name = "".join(random.sample("ABCDEFGHIJKLMNO", 5))
    print(f"[*] Patch `frida` to `{random_name}``")

    binary = lief.parse(input_file)

    for symbol in binary.symbols:
        if symbol.name == "frida_agent_main":
            symbol.name = "main"

        if "frida" in symbol.name:
            symbol.name = symbol.name.replace("frida", random_name)

        if "FRIDA" in symbol.name:
            symbol.name = symbol.name.replace("FRIDA", random_name)

    binary.write(input_file)