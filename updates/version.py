import os
import hashlib
import json

def get_md5(file_path):
    hash_md5 = hashlib.md5()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

update_dir = "./updates"
manifest = []

for root, dirs, files in os.walk(update_dir):
    for file in files:
        if file == "version.json": continue
        full_path = os.path.join(root, file)
        rel_path = os.path.relpath(full_path, update_dir)
        manifest.append({
            "name": file,
            "path": rel_path.replace("\\", "/"),
            "md5": get_md5(full_path),
            "size": os.path.getsize(full_path)
        })

with open(os.path.join(update_dir, "version.json"), "w") as f:
    json.dump(manifest, f, indent=2)