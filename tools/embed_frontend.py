Import("env")
import os

WEB_DIR = os.path.join(env["PROJECT_DIR"], "web")
OUT_FILE = os.path.join(env["PROJECT_DIR"], "src", "frontend_assets.h")

FILES = [
    ("INDEX_HTML", "index.html"),
    ("STYLE_CSS", "style.css"),
    ("APP_JS", "app.js"),
]

def c_escape(text):
    return (text.replace("\\", "\\\\")
                .replace('"', '\\"')
                .replace("\n", "\\n\"\n    \""))

def generate():
    lines = ["#ifndef FRONTEND_ASSETS_H", "#define FRONTEND_ASSETS_H", ""]
    for var_name, filename in FILES:
        path = os.path.join(WEB_DIR, filename)
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        escaped = c_escape(content)
        lines.append(f'static const char {var_name}[] =')
        lines.append(f'    "{escaped}";')
        lines.append("")
    lines.append("#endif")

    with open(OUT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

generate()
