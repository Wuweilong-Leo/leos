#!/usr/bin/env python3
"""
代码行数统计工具 — GUI 版本
支持选择项目路径，按语言分类统计，导出结果
"""

import os
import sys
import threading
import webbrowser
import tempfile
from pathlib import Path
from collections import defaultdict

# ── 扩展名 → 语言 ────────────────────────────────────────
EXT_LANG = {
    ".c": "C", ".h": "C",
    ".cpp": "C++", ".hpp": "C++", ".cc": "C++", ".cxx": "C++", ".hxx": "C++",
    ".py": "Python",
    ".java": "Java",
    ".go": "Go",
    ".rs": "Rust",
    ".js": "JavaScript", ".jsx": "JavaScript",
    ".ts": "TypeScript", ".tsx": "TypeScript",
    ".s": "Assembly", ".S": "Assembly", ".asm": "Assembly",
    ".sh": "Shell", ".bash": "Shell",
    ".rb": "Ruby",
    ".php": "PHP",
    ".cs": "C#",
    ".swift": "Swift",
    ".kt": "Kotlin", ".kts": "Kotlin",
    ".scala": "Scala",
    ".lua": "Lua",
    ".r": "R", ".R": "R",
    ".sql": "SQL",
    ".html": "HTML",
    ".css": "CSS", ".scss": "CSS", ".less": "CSS",
    ".vue": "Vue",
    ".zig": "Zig",
    ".ml": "OCaml", ".mli": "OCaml",
    ".hs": "Haskell",
    ".erl": "Erlang",
    ".ex": "Elixir", ".exs": "Elixir",
    ".clj": "Clojure",
    ".lisp": "Lisp", ".el": "Lisp",
    ".pl": "Perl",
    ".vim": "Vim",
    ".dart": "Dart",
    ".m": "ObjC",
}

# ── 注释风格 ──────────────────────────────────────────────
COMMENT_STYLE = {
    "C":          (["//"], True),
    "C++":        (["//"], True),
    "Java":       (["//"], True),
    "Go":         (["//"], True),
    "Rust":       (["//"], True),
    "JavaScript": (["//"], True),
    "TypeScript": (["//"], True),
    "C#":         (["//"], True),
    "Swift":      (["//"], True),
    "Kotlin":     (["//"], True),
    "Scala":      (["//"], True),
    "CSS":        (None, True),
    "Vue":        (["//"], True),
    "Zig":        (["//"], True),
    "PHP":        (["//", "#"], True),
    "ObjC":       (["//"], True),
    "Dart":       (["//"], True),
    "Python":     (["#"], False),
    "Ruby":       (["#"], False),
    "Shell":      (["#"], False),
    "R":          (["#"], False),
    "Perl":       (["#"], False),
    "Lua":        (["--"], False),
    "SQL":        (["--"], False),
    "Assembly":   ([";"], False),
    "HTML":       (None, True),
}

# ── 排除目录 ──────────────────────────────────────────────
EXCLUDE_DIRS = {
    ".git", ".svn", ".hg",
    "build", "cmake-build", "out", "bin", "obj", "target",
    "node_modules", "vendor", "third_party", "bower_components",
    "__pycache__", ".tox", ".mypy_cache", ".pytest_cache",
    "dist", ".gradle", ".idea", ".vscode", ".vs",
    "venv", ".env", "env", ".venv",
    "Debug", "Release", "x64",
}


def count_file(filepath, lang):
    style = COMMENT_STYLE.get(lang)
    if not style:
        total = blank = 0
        try:
            with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    total += 1
                    if not line.strip():
                        blank += 1
        except (OSError, PermissionError):
            pass
        return total, blank, 0, total - blank

    line_prefixes, has_block = style
    total = blank = comment = code = 0
    in_block = False
    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            for raw in f:
                total += 1
                line = raw.strip()
                if not line:
                    blank += 1
                    continue
                if in_block:
                    comment += 1
                    if "*/" in line:
                        in_block = False
                    continue
                if has_block and line.startswith("/*"):
                    comment += 1
                    if "*/" not in line:
                        in_block = True
                    continue
                if has_block and line.startswith("<!--"):
                    comment += 1
                    if "-->" not in line:
                        in_block = True
                    continue
                if line_prefixes and any(line.startswith(p) for p in line_prefixes):
                    comment += 1
                    continue
                code += 1
    except (OSError, PermissionError):
        pass
    return total, blank, comment, code


def find_files(root):
    result = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
        for fname in filenames:
            ext = Path(fname).suffix
            if ext in EXT_LANG:
                result.append((os.path.join(dirpath, fname), EXT_LANG[ext]))
    return sorted(result)


def analyze(root, on_progress=None):
    files = find_files(root)
    if not files:
        return [], {}

    lang_stats = defaultdict(lambda: {"files": 0, "code": 0, "comment": 0, "blank": 0, "total": 0})
    file_stats = []
    total_files = len(files)

    for i, (fpath, lang) in enumerate(files):
        t, b, c, co = count_file(fpath, lang)
        ls = lang_stats[lang]
        ls["files"] += 1
        ls["code"] += co
        ls["comment"] += c
        ls["blank"] += b
        ls["total"] += t
        file_stats.append((fpath, lang, co, c, b, t))
        if on_progress and i % 50 == 0:
            on_progress(i + 1, total_files)

    return file_stats, dict(lang_stats)


# ══════════════════════════════════════════════════════════
# GUI
# ══════════════════════════════════════════════════════════

def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    # ── 颜色主题 ─────────────────────────────────────────
    BG       = "#1e1e2e"
    BG2      = "#181825"
    FG       = "#cdd6f4"
    ACCENT   = "#89b4fa"
    GREEN    = "#a6e3a1"
    YELLOW   = "#f9e2af"
    RED      = "#f38ba8"
    SURFACE  = "#313244"
    BORDER   = "#45475a"

    root = tk.Tk()
    root.title("LoC — 代码行数统计")
    root.geometry("820x620")
    root.minsize(700, 500)
    root.configure(bg=BG)

    style = ttk.Style(root)
    style.theme_use("clam")

    style.configure(".", background=BG, foreground=FG, borderwidth=0)
    style.configure("TFrame", background=BG)
    style.configure("TLabel", background=BG, foreground=FG, font=("Microsoft YaHei UI", 10))
    style.configure("Title.TLabel", font=("Microsoft YaHei UI", 18, "bold"), foreground=ACCENT)
    style.configure("Summary.TLabel", font=("Consolas", 12, "bold"), foreground=GREEN)
    style.configure("Path.TLabel", font=("Consolas", 9), foreground=YELLOW)
    style.configure("TButton", background=SURFACE, foreground=FG, font=("Microsoft YaHei UI", 10),
                     borderwidth=0, focusthickness=0, padding=(16, 8))
    style.map("TButton", background=[("active", ACCENT)], foreground=[("active", BG2)])
    style.configure("Accent.TButton", background=ACCENT, foreground=BG2, font=("Microsoft YaHei UI", 10, "bold"))
    style.map("Accent.TButton", background=[("active", GREEN)])

    # Treeview 样式
    style.configure("Treeview", background=BG2, foreground=FG, fieldbackground=BG2,
                     font=("Consolas", 10), rowheight=28, borderwidth=0)
    style.configure("Treeview.Heading", background=SURFACE, foreground=ACCENT,
                     font=("Microsoft YaHei UI", 10, "bold"), borderwidth=0, relief="flat")
    style.map("Treeview.Heading", background=[("active", SURFACE)])
    style.map("Treeview", background=[("selected", SURFACE)], foreground=[("selected", ACCENT)])

    # ── 状态变量 ─────────────────────────────────────────
    path_var = tk.StringVar(value=os.getcwd())
    status_var = tk.StringVar(value="选择项目路径，点击「开始统计」")
    result_data = {"file_stats": [], "lang_stats": {}, "root": ""}

    # ── 布局 ─────────────────────────────────────────────
    # 顶部
    top = ttk.Frame(root)
    top.pack(fill="x", padx=20, pady=(20, 0))

    ttk.Label(top, text="LoC", style="Title.TLabel").pack(side="left")
    ttk.Label(top, text="  代码行数统计", font=("Microsoft YaHei UI", 12), foreground=FG).pack(side="left", padx=(4, 0))

    # 路径选择
    path_frame = ttk.Frame(root)
    path_frame.pack(fill="x", padx=20, pady=(16, 0))

    ttk.Label(path_frame, text="项目路径").pack(side="left")
    path_entry = ttk.Entry(path_frame, textvariable=path_var, font=("Consolas", 10),
                            width=60)
    path_entry.pack(side="left", padx=(8, 8), fill="x", expand=True)

    def browse():
        d = filedialog.askdirectory(initialdir=path_var.get())
        if d:
            path_var.set(d)

    ttk.Button(path_frame, text="浏览...", command=browse).pack(side="left")

    # 按钮
    btn_frame = ttk.Frame(root)
    btn_frame.pack(fill="x", padx=20, pady=(12, 0))

    def start_analysis():
        p = path_var.get().strip()
        if not p or not os.path.isdir(p):
            messagebox.showwarning("提示", "请选择有效的项目目录")
            return
        btn_start.configure(state="disabled")
        status_var.set("正在扫描...")
        tree_lang.delete(*tree_lang.get_children())
        tree_file.delete(*tree_file.get_children())

        def done(future):
            file_stats, lang_stats = future
            result_data["file_stats"] = file_stats
            result_data["lang_stats"] = lang_stats
            result_data["root"] = p
            refresh_tables(file_stats, lang_stats)
            btn_start.configure(state="normal")
            total_code = sum(ls["code"] for ls in lang_stats.values())
            total_files = sum(ls["files"] for ls in lang_stats.values())
            status_var.set(f"完成 — {total_files} 个文件，{total_code:,} 行代码")

        def run():
            return analyze(p)

        import concurrent.futures
        executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        future = executor.submit(run)
        future.add_done_callback(lambda f: root.after(0, done, f.result()))

    btn_start = ttk.Button(btn_frame, text="▶  开始统计", command=start_analysis, style="Accent.TButton")
    btn_start.pack(side="left")

    def export_html():
        if not result_data["lang_stats"]:
            messagebox.showinfo("提示", "请先执行统计")
            return
        lang_stats = result_data["lang_stats"]
        file_stats = result_data["file_stats"]
        proj = os.path.basename(result_data["root"])

        sorted_langs = sorted(lang_stats.items(), key=lambda x: x[1]["code"], reverse=True)
        g_code = sum(ls["code"] for ls in lang_stats.values())
        g_comment = sum(ls["comment"] for ls in lang_stats.values())
        g_blank = sum(ls["blank"] for ls in lang_stats.values())
        g_total = sum(ls["total"] for ls in lang_stats.values())
        g_files = sum(ls["files"] for ls in lang_stats.values())

        lang_rows = ""
        for lang, ls in sorted_langs:
            pct = ls["code"] / g_code * 100 if g_code else 0
            bar = "█" * int(pct / 2) + "░" * (50 - int(pct / 2))
            lang_rows += f"""<tr>
                <td><b>{lang}</b></td><td>{ls['files']}</td>
                <td>{ls['code']:,}</td><td>{ls['comment']:,}</td><td>{ls['blank']:,}</td><td>{ls['total']:,}</td>
                <td><code>{bar}</code> {pct:.1f}%</td>
            </tr>"""

        top_files = sorted(file_stats, key=lambda x: x[2], reverse=True)[:20]
        file_rows = ""
        for fpath, lang, co, c, b, t in top_files:
            rel = os.path.relpath(fpath, result_data["root"]).replace("\\", "/")
            file_rows += f"<tr><td>{lang}</td><td>{co:,}</td><td>{c:,}</td><td>{b:,}</td><td>{rel}</td></tr>"

        html = f"""<!DOCTYPE html><html><head><meta charset="utf-8"><title>{proj} — LoC</title>
<style>
body{{font-family:'Segoe UI',sans-serif;background:#1e1e2e;color:#cdd6f4;max-width:960px;margin:40px auto;padding:0 20px}}
h1{{color:#89b4fa}}h2{{color:#a6e3a1;border-bottom:1px solid #45475a;padding-bottom:8px}}
table{{border-collapse:collapse;width:100%;margin:16px 0}}
th{{background:#313244;color:#89b4fa;text-align:left;padding:10px 12px}}
td{{padding:8px 12px;border-bottom:1px solid #313244}}
tr:hover{{background:#313244}}code{{font-size:11px;color:#6c7086}}
.summary{{background:#313244;border-radius:12px;padding:20px;margin:20px 0;display:flex;gap:40px}}
.summary .num{{font-size:32px;font-weight:bold;color:#a6e3a1}}
.summary .label{{font-size:13px;color:#6c7086;margin-top:4px}}
</style></head><body>
<h1>📊 {proj}</h1>
<div class="summary">
<div><div class="num">{g_code:,}</div><div class="label">代码行</div></div>
<div><div class="num">{g_comment:,}</div><div class="label">注释行</div></div>
<div><div class="num">{g_blank:,}</div><div class="label">空行</div></div>
<div><div class="num">{g_files:,}</div><div class="label">文件数</div></div>
</div>
<h2>按语言统计</h2><table>
<tr><th>Language</th><th>Files</th><th>Code</th><th>Comment</th><th>Blank</th><th>Total</th><th>占比</th></tr>
{lang_rows}
</table>
<h2>Top 20 文件</h2><table>
<tr><th>Lang</th><th>Code</th><th>Comment</th><th>Blank</th><th>File</th></tr>
{file_rows}
</table></body></html>"""

        out = os.path.join(tempfile.gettempdir(), f"loc_{proj}.html")
        with open(out, "w", encoding="utf-8") as f:
            f.write(html)
        webbrowser.open(out)

    ttk.Button(btn_frame, text="📄 导出 HTML", command=export_html).pack(side="left", padx=(12, 0))

    # Notebook
    notebook = ttk.Notebook(root)
    notebook.pack(fill="both", expand=True, padx=20, pady=(16, 0))

    # 语言表格
    lang_frame = ttk.Frame(notebook)
    notebook.add(lang_frame, text="  按语言  ")
    cols_lang = ("lang", "files", "code", "comment", "blank", "total")
    tree_lang = ttk.Treeview(lang_frame, columns=cols_lang, show="headings", selectmode="browse")
    for col, text, w in [("lang", "Language", 120), ("files", "Files", 80),
                          ("code", "Code", 100), ("comment", "Comment", 100),
                          ("blank", "Blank", 100), ("total", "Total", 100)]:
        tree_lang.heading(col, text=text, anchor="w")
        tree_lang.column(col, width=w, anchor="e" if col != "lang" else "w", minwidth=60)

    sb1 = ttk.Scrollbar(lang_frame, orient="vertical", command=tree_lang.yview)
    tree_lang.configure(yscrollcommand=sb1.set)
    tree_lang.pack(side="left", fill="both", expand=True)
    sb1.pack(side="right", fill="y")

    # 文件表格
    file_frame = ttk.Frame(notebook)
    notebook.add(file_frame, text="  按文件 (Top 50)  ")
    cols_file = ("lang", "code", "comment", "blank", "file")
    tree_file = ttk.Treeview(file_frame, columns=cols_file, show="headings", selectmode="browse")
    for col, text, w in [("lang", "Lang", 80), ("code", "Code", 80), ("comment", "Comment", 80),
                          ("blank", "Blank", 80), ("file", "File", 400)]:
        tree_file.heading(col, text=text, anchor="w")
        tree_file.column(col, width=w, anchor="e" if col != "file" else "w", minwidth=60)

    sb2 = ttk.Scrollbar(file_frame, orient="vertical", command=tree_file.yview)
    tree_file.configure(yscrollcommand=sb2.set)
    tree_file.pack(side="left", fill="both", expand=True)
    sb2.pack(side="right", fill="y")

    # 底部状态栏
    status_bar = tk.Label(root, textvariable=status_var, bg=BG2, fg="#6c7086",
                           font=("Microsoft YaHei UI", 9), anchor="w", padx=16, pady=6)
    status_bar.pack(fill="x", side="bottom")

    # ── 刷新表格 ─────────────────────────────────────────
    def refresh_tables(file_stats, lang_stats):
        sorted_langs = sorted(lang_stats.items(), key=lambda x: x[1]["code"], reverse=True)
        g_code = sum(ls["code"] for ls in lang_stats.values())

        for lang, ls in sorted_langs:
            pct = ls["code"] / g_code * 100 if g_code else 0
            tree_lang.insert("", "end", values=(
                lang, ls["files"], f"{ls['code']:,}", f"{ls['comment']:,}",
                f"{ls['blank']:,}", f"{ls['total']:,}"
            ), tags=("lang",))

        # 合计行
        g_files = sum(ls["files"] for ls in lang_stats.values())
        g_comment = sum(ls["comment"] for ls in lang_stats.values())
        g_blank = sum(ls["blank"] for ls in lang_stats.values())
        g_total = sum(ls["total"] for ls in lang_stats.values())
        tree_lang.insert("", "end", values=(
            "Total", g_files, f"{g_code:,}", f"{g_comment:,}", f"{g_blank:,}", f"{g_total:,}"
        ), tags=("total",))

        # Top 50 文件
        top_files = sorted(file_stats, key=lambda x: x[2], reverse=True)[:50]
        for fpath, lang, co, c, b, t in top_files:
            rel = os.path.relpath(fpath, result_data["root"])
            tree_file.insert("", "end", values=(
                lang, f"{co:,}", f"{c:,}", f"{b:,}", rel
            ))

        tree_lang.tag_configure("total", foreground=ACCENT, font=("Consolas", 10, "bold"))

    # 拖拽目录支持
    def on_drop(files):
        if files:
            p = files[0].strip("{}")
            if os.path.isdir(p):
                path_var.set(p)

    # Windows 拖拽支持 (windnd)
    try:
        import windnd
        windnd.hook_dropfiles(root, func=on_drop)
    except ImportError:
        pass

    root.mainloop()


if __name__ == "__main__":
    run_gui()
