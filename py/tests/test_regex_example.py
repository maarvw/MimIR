from pathlib import Path

import mim
import mim.plug.regex as regex


def test_python_package_works(tmp_path, monkeypatch):
    repo_root = Path(__file__).resolve().parents[2]
    plugin_dir = repo_root / "build" / "lib" / "mim"
    if not plugin_dir.is_dir():
        raise FileNotFoundError(f"expected plugin directory at '{plugin_dir}'")

    monkeypatch.chdir(tmp_path)

    driver = mim.Driver()
    driver.add_search_path(plugin_dir)
    builder = regex.RegBuilder(driver, "regex", mim.Level.Error)

    alpha = builder.range("a", "z") | builder.range("A", "Z")  # [a-zA-Z]
    under_hyph = builder.lit("_") | builder.lit("-")  # [_\-]
    dot_under_hyph = builder.lit(".").disj([builder.lit("_"), builder.lit("-")])  # [._\-]
    an = builder.alnum()
    # Local: [a-zA-Z0-9](?:[a-zA-Z0-9]*[._\-]+[a-zA-Z0-9])*[a-zA-Z0-9]*
    local = an + (an["*"] + dot_under_hyph["+"] + an)["*"] + an["*"]

    # Domain: [a-zA-Z0-9](?:[a-zA-Z0-9]*[_\-]+[a-zA-Z0-9])*[a-zA-Z0-9]*
    domain = an + (an["*"] + under_hyph["+"] + an)["*"] + an["*"]

    # Subdomain: (?:(?:[a-zA-Z0-9]*[_\-]+[a-zA-Z0-9])*[a-zA-Z0-9]+\.)*
    subdomain = ((an["*"] + under_hyph["+"] + an)["*"] + an["+"] + builder.lit("."))["*"]

    # Top Level Domain (.com, .de, etc...): [a-zA-Z][a-zA-Z]+
    tld = alpha + alpha["+"]

    email = local + builder.lit("@") + domain + builder.lit(".") + subdomain + tld
    library = email.jit()

    assert library["match_func"]("test@test.de".encode("utf-8"))
