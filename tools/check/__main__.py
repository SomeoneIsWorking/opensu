import sys

from tools.check.cli import main

try:
    sys.exit(main())
except RuntimeError as refusal:
    sys.exit(f"tools.check: {refusal}")
