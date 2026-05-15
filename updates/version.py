import os
import hashlib
import json

def get_md5(file_path):
    hash_md5 = hashlib.md5()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

# 【关键修改】：因为 Shell 脚本会进入此目录，所以这里用 "." 表示当前目录
update_dir = "." 
manifest = []

for root, dirs, files in os.walk(update_dir):
    for file in files:
        # 排除脚本自身和生成的 json
        if file == "version.json" or file == "version.py": 
            continue
            
        full_path = os.path.join(root, file)
        # 计算相对于 updates 目录的路径（用于 Launcher 下载）
        rel_path = os.path.relpath(full_path, update_dir).replace("\\", "/")
        
        manifest.append({
            "name": file,
            "path": rel_path,
            "md5": get_md5(full_path),
            "size": os.path.getsize(full_path)
        })

with open("version.json", "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2, ensure_ascii=False)