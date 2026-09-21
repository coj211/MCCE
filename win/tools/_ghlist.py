import json
import sys

d = json.load(sys.stdin)
if isinstance(d, dict):
    print("MSG:", d.get("message"))
else:
    for it in d:
        print(it.get("type"), it.get("name"), it.get("size", ""))
