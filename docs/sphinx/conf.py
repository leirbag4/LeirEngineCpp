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
    "sphinxcontrib.jquery",
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
    "fullToctreeMaxDepth": 1,
    "verboseBuild": False,
}

primary_domain = "cpp"
highlight_language = "cpp"

# --- Tema (seleccionable vía LEIR_DOCS_THEME) + Pygments (LEIR_DOCS_PYGMENT) ---
# generate_docs.bat [FILTER] [THEME] [PYGMENT]
#   THEME: furo (default), book, pydata, rtd  (todos dark; immaterial deshabilitado: incompatible con Sphinx 9.1)
#   PYGMENT: default, monokai, dracula, github-dark, native, nord, one-dark, material, solarized-dark ...
_theme_map = {
    "furo": "furo",
    "book": "sphinx_book_theme",
    "pydata": "pydata_sphinx_theme",
    "rtd": "sphinx_rtd_theme",
}
html_theme = _theme_map.get(os.environ.get("LEIR_DOCS_THEME", "furo").lower(), "furo")

_pyg = os.environ.get("LEIR_DOCS_PYGMENT", "").strip()
_pyg_override_css = None
if _pyg and _pyg.lower() != "default":
    pygments_style = _pyg
    pygments_dark_style = _pyg
    # Colorea también las FIRMAS C++ del dominio (Sphinx cpp usa .k/.kt/.nf,
    # estilizadas por el tema en gris) con la paleta del pygment elegido.
    try:
        from pygments.styles import get_style_by_name
        from pygments.token import Token
        st = get_style_by_name(_pyg).styles
        def hexcol(tok):
            val = st.get(tok, "")
            for part in str(val).split():
                if part.startswith("#"):
                    return part
            return None
        kw = hexcol(Token.Keyword)
        kt = hexcol(Token.Keyword.Type) or kw          # void/etc → Keyword color if Type undefined
        nf = hexcol(Token.Name.Function)
        nc = hexcol(Token.Name.Class)
        nn = hexcol(Token.Name)
        stl = hexcol(Token.Literal.String)
        op = hexcol(Token.Operator)
        opw = hexcol(Token.Operator.Word) or op        # override/const → Operator color
        sel = lambda c: c if c else "inherit"
        _pyg_override_css = "\n".join([
            "/* cpp-domain signature colors from pygment: %s */" % _pyg,
            ".sig .k, .sig .k .pre { color: %s; }" % sel(kw),
            ".sig .kt, .sig .kt .pre { color: %s; }" % sel(kt),
            ".sig .nf, .sig .nf .pre { color: %s; }" % sel(nf),
            ".sig .nc, .sig .nc .pre { color: %s; }" % sel(nc),
            ".sig .n, .sig .n .pre { color: %s; }" % sel(nn),
            ".sig .s, .sig .s .pre, .sig .str, .sig .str .pre { color: %s; }" % sel(stl),
            ".sig .o, .sig .o .pre, .sig .p, .sig .p .pre { color: %s; }" % sel(op),
            ".sig .kr, .sig .kr .pre { color: %s; }" % sel(opw),
        ])
    except Exception as e:
        _pyg_override_css = None

html_title = "LeirEngine"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
# sphinx-immaterial carga google_fonts al iniciar; sin internet/config falla con 'font'.
# Desactivamos google fonts para el build offline/portátil.

if _pyg_override_css:
    _ov_path = os.path.join(here, "_static", "cpp-pygment-override.css")
    with open(_ov_path, "w", encoding="utf-8") as _f:
        _f.write(_pyg_override_css)
    html_css_files = html_css_files + ["cpp-pygment-override.css"]
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