# LeirEngine docs — Sphinx configuration (TODO_DOCS.md §3.3).
# Stack: Sphinx + Furo + MyST + Breathe + Exhale.
# Consume el XML de Doxygen (docs/sphinx/_xml) generado por el target `docs` /
# `doxygen docs/Doxyfile` (corrido ANTES de sphinx-build).
import os

here = os.path.abspath(os.path.dirname(__file__))

project = "LeirEngine"
copyright = "2026 LeirEngine"
author = "LeirEngine"
release = "1.0"

extensions = [
    "myst_parser",
    "breathe",
    "exhale",
    "sphinx_design",
    "sphinx_copybutton",
]

# --- Markdown (MyST): escribimos las guías en .md ---
myst_enable_extensions = [
    "colon_fence",
    "tasklist",
    "deflist",
    "dollarmath",
    "html_image",
    "fieldlist",
]

# --- Breathe (Doxygen XML -> directivas Sphinx) ---
breathe_projects = {"LeirEngine": os.path.join(here, "_xml")}
breathe_default_project = "LeirEngine"
breathe_default_members = ("members", "undoc-members")

# --- Exhale (árbol de la API automático desde el XML) ---
exhale_args = {
    "containmentFolder": "api",
    "rootFileName": "index.rst",
    "rootFileTitle": "API Reference",
    "createTreeView": True,
    "exhaleExecutesDoxygen": False,
    "doxygenStripFromPath": os.path.abspath(os.path.join(here, "..", "..")),
    "verboseBuild": False,
}

primary_domain = "cpp"
highlight_language = "cpp"

# --- Tema Furo ---
html_theme = "furo"
html_title = "LeirEngine"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_copy_source = False
html_show_sourcelink = False

master_doc = "index"

# Baseline de warnings benignos del API auto-generado (Exhale/Breathe sobre Doxygen
# con EXTRACT_ALL: duplicados de nested classes, algunas declaraciones que Breathe no
# parsea, símbolos globales de tests/). No afectan el render; se silencian para un
# log limpio en el .bat. Se revisitan con el retrofit de docblocks (§3.6).
suppress_warnings = ["*"]

# --- Fix Furo: Exhale genera `.. contents::` sin la clase mágica que Furo exige ---
# Furo lanza ERROR si encuentra `.. contents::` bare. Lo parcheamos post-generación.
def _patch_furo_contents(app):
    import pathlib
    api_dir = pathlib.Path(here) / "api"
    if not api_dir.is_dir():
        return
    for p in list(api_dir.glob("*.rst")) + list(api_dir.glob("*.rst.include")):
        try:
            text = p.read_text(encoding="utf-8")
        except Exception:
            continue
        if ".. contents::" in text and "this-will-duplicate-information" not in text:
            text = text.replace(".. contents::", ".. contents::\n   :class: this-will-duplicate-information-and-it-is-still-useful-here")
            p.write_text(text, encoding="utf-8")

def setup(app):
    app.connect("builder-inited", _patch_furo_contents)