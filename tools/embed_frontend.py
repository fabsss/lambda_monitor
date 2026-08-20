Import("env")
import os
import hashlib

WEB_DIR = os.path.join(env["PROJECT_DIR"], "web")
OUT_FILE = os.path.join(env["PROJECT_DIR"], "src", "frontend_assets.h")

FILES = [
    ("INDEX_HTML", "index.html"),
    ("STYLE_CSS", "style.css"),
    ("APP_JS", "app.js"),
    ("UPLOT_JS", "uplot.min.js"),
    ("UPLOT_CSS", "uplot.min.css"),
]

def c_escape(text):
    return (text.replace("\\", "\\\\")
                .replace('"', '\\"')
                .replace("?", "\\?")
                .replace("\n", "\\n\"\n    \""))

def compute_hash():
    """Compute hash of all web files to detect changes."""
    hasher = hashlib.md5()
    for _, filename in FILES:
        path = os.path.join(WEB_DIR, filename)
        with open(path, "rb") as f:
            hasher.update(f.read())
    return hasher.hexdigest()

def generate():
    """Generate frontend_assets.h only if web files changed."""
    current_hash = compute_hash()

    # Check if output exists and has matching hash
    if os.path.exists(OUT_FILE):
        try:
            with open(OUT_FILE, "r", encoding="utf-8") as f:
                content = f.read()
                # Extract hash from comment at top of file
                for line in content.split("\n"):
                    if line.startswith("// Hash: "):
                        stored_hash = line.split(": ")[1]
                        if stored_hash == current_hash:
                            print(f"[frontend_assets.h] Up to date (hash: {current_hash[:8]}...)")
                            return
        except:
            pass

    # Regenerate
    print(f"[frontend_assets.h] Regenerating (hash changed)")
    lines = [f"// Hash: {current_hash}", "#ifndef FRONTEND_ASSETS_H", "#define FRONTEND_ASSETS_H", ""]
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
