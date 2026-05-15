# -*- coding: utf-8 -*-
import os
import hashlib
import json

def get_md5(file_path):
    hash_md5 = hashlib.md5()
    try:
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()
    except Exception as e:
        return ""

# 使用 "." 因为安装脚本会 cd 进入 updates 目录
update_dir = "." 
manifest = []

for root, dirs, files in os.walk(update_dir):
    for file in files:
        # 排除脚本自身和生成的 json
        if file == "version.json" or file == "version.py": 
            continue
            
        full_path = os.path.join(root, file)
        # 计算相对路径并统一使用斜杠
        rel_path = os.path.relpath(full_path, update_dir).replace("\\", "/")
        
        file_size = os.path.getsize(full_path)
        file_md5 = get_md5(full_path)
        
        if file_md5:
            manifest.append({
                "name": file,
                "path": rel_path,
                "md5": file_md5,
                "size": file_size
            })

# 显式指定 utf-8 写入
with open("version.json", "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2, ensure_ascii=False)

print("Manifest generated successfully.")