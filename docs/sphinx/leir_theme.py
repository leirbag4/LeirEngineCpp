# docs/sphinx/leir_theme.py — Pygments style "leir_theme" para LeirEngine.
# Uso: generate_docs.bat UI pydata leir_theme  →  conf.py importa y registra.
# Basado en monokai con nombres en #D09EF9 (UIButton/SetText/UIElement).
from pygments.style import Style
from pygments.token import Token, Keyword, Name, Comment, String, Error, Number, Operator, Generic, Whitespace

class LeirThemeStyle(Style):
    background_color = "#272822"
    highlight_color = "#49483e"
    styles = {
        Token:              "#f8f8f2",
        Whitespace:         "#f8f8f2",
        Comment:            "#75715e",
        Comment.Preproc:    "#75715e",
        Keyword:            "#66d9ef",
        Keyword.Constant:   "#66d9ef",
        Keyword.Declaration:"#66d9ef",
        Keyword.Namespace:  "#f92672",
        Keyword.Pseudo:     "#66d9ef",
        Keyword.Reserved:   "#66d9ef",
        Keyword.Type:       "#f262bf",
        Operator:           "#ff4689",
        Operator.Word:      "#ff4689",
        Name:               "#cdd6f4",
        Name.Attribute:     "#a6e22e",
        Name.Builtin:       "#66d9ef",
        Name.Builtin.Pseudo:"#66d9ef",
        Name.Class:         "#D09EF9",
        Name.Constant:      "#66d9ef",
        Name.Decorator:     "#a6e22e",
        Name.Entity:        "#a6e22e",
        Name.Exception:     "#D09EF9",
        Name.Function:      "#D09EF9",
        Name.Function.Magic:"#D09EF9",
        Name.Property:      "#D09EF9",
        Name.Label:         "#f8f8f2",
        Name.Namespace:     "#cdd6f4",
        Name.Other:         "#D09EF9",
        Name.Tag:           "#f92672",
        Name.Variable:      "#f8f8f2",
        Name.Variable.Class:"#f8f8f2",
        Name.Variable.Global:"#f8f8f2",
        Name.Variable.Instance:"#f8f8f2",
        Number:             "#ae81ff",
        String:             "#85d6ad",
        String.Doc:         "#75715e",
        Generic.Heading:    "#f8f8f2",
        Generic.Subheading: "#75715e",
        Generic.Deleted:    "#f92672",
        Generic.Inserted:   "#a6e22e",
        Generic.Error:      "#960050 bg:#1e0010",
        Generic.Emph:       "italic #f8f8f2",
        Generic.Strong:     "bold #f8f8f2",
        Generic.Prompt:     "#75715e",
        Generic.Output:     "#f8f8f2",
        Generic.Traceback:  "#f8f8f2",
        Error:              "#960050 bg:#1e0010",
    }
